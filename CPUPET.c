/**************************************************/
/*
	CPUPET.c - main program source file for the the CPUPET tool. Version 1.0
	CPUPET - The CPU Performance Evaluation Tool

	Created by Ashish Jha
	iashishjha@yahoo.com
	August 13 2010

	COPYRIGHT 2010 Ashish Jha. All rights reserved.
	This software is licensed under the "The GNU General Public License (GPL) Version 2". Before viewing, copying or distribution this software, please check the full  terms of the license in the file LICENSE.txt or COPYING.txt

	ALL SOURCE BY AUTHOR UNLESS AS CITED BELOW
	1. Method to find num of CPU's borrowed from http://lists.gnu.org/archive/html/autoconf/2002-08/msg00126.html
	2. Method to subtract timeval values borrowed from code borrowed from http://www.gnu.org/s/libc/manual/html_node/Elapsed-Time.html#Elapsed-Time


	PROGRAM DETAILS
	In this current version 1.0, the tool runs Matrix Multiplication in parallel across all hardware threads. As program input, one could run a single thread or multiple threads (max is the # of CPU's on your system) and the tool would deliver results in MOPS i.e. Million Operations Per Sec. The Operation is one full 4x4 matrix multiply i.e. float C[4][4] = A[4][4] * B[4][4]. You could increase the size of the problem set from command line which is set-in InputData.Length e.g. with a value of 1000 means 1000 4x4 matrix multiplies. For repeatability and stability of results each full Length of matrix multiply is iterated several times using the command input (parameter InputData.LoopIter). The higher the InputData.LoopIter, the more stable the results for smaller Input data size i.e. InputData.Length. One could also set program input to bind/pin threads to a particular CPU (right now chosen in thread creation order), which allows to look at OS scheduling issues. Example of input command line to the program:
	./CPUPET_v4_x64 1000 3 2 0 2600 6000
	#loopIterationCount=1000, 
	memLength=3 i.e. 3*[4*4*4]*3 = 576 bytes, 
	numThreadsToRun=2 i.e. run with 2 Threads, 
	doPROCBind=0 i.e. no CPU binding, 
	PROCFreqMHz=2600 - Processor Frequency in MHz,
	RDTSCFreqMHz=2600 i.e. rdtsc clks is same as Proc Clks

	The program sums up result for every thread (which could help in diagnostics) and finally results is as comma seperated values which could be grep'ed (cat *.log | grep "*** Op/sec Results for," from run log file. 
	
	Sample result when running one Thread i.e. ./CPUPET_v4_x64 100000 1000 1 1 2800 2800
		*** Op/sec Results for, MatMul 1 threads with doPROCBind=1,Total Mops=,19.634 ,THREAD#00=, 19.634
	==>It gives ~19M Matrix Multiply Operations Per Sec
	
	Sample result when running 2 Threads i.e. ./CPUPET_v4_x64 100000 1000 2 1 2800 2800
	*** Op/sec Results for, MatMul 2 threads with doPROCBind=1,Total Mops=,39.228 ,Avg Mops=,19.614 ,THREAD#00=, 19.612 ,THREAD#01=, 19.616

	For further questions and details please refer to the FAQ.txt and README.txt
*/
/**************************************************/

#define _GNU_SOURCE
#include <gnu/libc-version.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#define DOPRINT 1
#define DODEBUG 1

#define FALSE 0
#define TRUE 1
#define CACHELINE_ALIGN 64

//define input and output data types
typedef struct INPUTDATA {
  unsigned long Length;
  unsigned long LoopIter;
  unsigned long DelayCount;
} INPUT_DATA, *PINPUT_DATA;

//typedef 
struct OUTPUTDATA {
  uint64_t StartClockticks;
  uint64_t EndClockticks;
  //TSC not synchronized across CPU's, resorting to basic time method
	struct timeval StartTimeval;
	struct timeval EndTimeval;  
  unsigned long Length;
  unsigned long LoopIter;  
  int isValidTestResult;
} OUTPUT_DATA, *POUTPUTDATA;

//Data
INPUT_DATA InputData;

//struct OUTPUTDATA OutputData;

unsigned int loopIterationCount=100;
char* funcToCall="2T";

unsigned long set_cpu=0;

uint64_t PROCFreqMHz=1;
uint64_t RDTSCFreqMHz=1;
char* funcName_PrintResults=NULL;

double clksPerOP, clksPerOP2;
double normFactor, normFactorNumr, normFactorDenr = 1.0;

//Thread_MatVecMul
int set_matvecmul_count_thread1, get_matvecmul_count_thread1=0; //main thread
int set_matvecmul_count_thread2, get_matvecmul_count_thread2=0;	//child thread	

uint64_t start_time_thread1, start_time_thread2, end_time_thread1, end_time_thread2=0;

///////////

#define ROWS 4
#define COLS 4
float srcArrayA_4x4[4][4]={{11,12,13,14}, {21,22,23,24}, {31,32,33,34}, {41,42,43,44}};
float srcArrayB_4x4[4][4]={{1,10,100,1000}, {2,20,200,2000}, {3,30,300,3000}, {4,40,400,4000}};
float dstArrayC_4x4[4][4]={{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}};    

#define MAX_CPU_SUPPORT 16
volatile int mainThreadReady=0;
volatile int anyWorkerThreadDone=0;
long numCPU=1;
volatile int numThreadsToRun=1;
int doPROCBind=1;

pthread_t threads[MAX_CPU_SUPPORT];    //index 0 is the MAIN thread
void *fAVector_Threads[MAX_CPU_SUPPORT];
void *fBVector_Threads[MAX_CPU_SUPPORT];
void *fCVector_Threads[MAX_CPU_SUPPORT];
struct OUTPUTDATA OutputData_Threads[MAX_CPU_SUPPORT];

cpu_set_t new_mask_thread2;  	
pthread_cond_t cond_thread1;
pthread_mutex_t mutex_thread1;


long find_num_proc() { 
	//method to find num of processors borrowed from http://lists.gnu.org/archive/html/autoconf/2002-08/msg00126.html
  long num_proc = -1;
  long num_proc_max = -1;
  
  #ifdef _SC_NPROCESSORS_ONLN
    num_proc = sysconf(_SC_NPROCESSORS_ONLN);
    
    if (num_proc < 1)
    	printf("Problem finding num Procs online: %s\n",	strerror (errno));		
      
    num_proc_max = sysconf(_SC_NPROCESSORS_CONF);
    
    if (num_proc_max < 1) {
      printf("Problem finding num Procs configured: %s\n", strerror (errno));
    } else {
      printf ("%ld of %ld Procs Online\n",num_proc, num_proc_max);
    }
  #else
    printf("*** Could not find num Procs\n");
  #endif
  return num_proc;
}

__attribute__((noinline)) void mem_alloc_init(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
    
  myid = (long) pmyid;
//allocate mem
  if(j = posix_memalign(&fAVector_Threads[myid], CACHELINE_ALIGN, sizeof(float)*ROWS*COLS*InputData.Length)) 
    printf("THREAD#%02ld: posix_memalign ERROR %s\n", myid, strerror(j));
  else
    if(DODEBUG) printf("THREAD#%02ld: posix_memalign %p\n", myid, fAVector_Threads[myid]);
  
  if(j = posix_memalign(&fBVector_Threads[myid], CACHELINE_ALIGN, sizeof(float)*ROWS*COLS*InputData.Length)) 
    printf("THREAD#%02ld: posix_memalign ERROR %s\n", myid, strerror(j));
  else
    if(DODEBUG) printf("THREAD#%02ld: posix_memalign %p\n", myid, fBVector_Threads[myid]);
  
  if(j = posix_memalign(&fCVector_Threads[myid], CACHELINE_ALIGN, sizeof(float)*ROWS*COLS*InputData.Length)) 
    printf("THREAD#%02ld: posix_memalign ERROR %s\n", myid, strerror(j));
  else
    if(DODEBUG) printf("THREAD#%02ld: posix_memalign %p\n", myid, fCVector_Threads[myid]);

//initialize    
  src1 = (float*)fAVector_Threads[myid];
  src2 = (float*)fBVector_Threads[myid];
  dst = (float*)fCVector_Threads[myid];

  for (l = 0; l < InputData.Length; l++) {
    asrc1 = &srcArrayA_4x4[0][0];
    asrc2 = &srcArrayB_4x4[0][0];  
    for (i = 0; i < ROWS; i++) {
      for (j = 0; j < COLS; j++) {        
        *src1 = *asrc1;
        *src2 = *asrc2;
        *dst = 0;

        src1++;
        src2++;
        dst++;

        asrc1++;
        asrc2++;
      } //j
    } //i       
  } //l   

} //mem_alloc_init

__attribute__((noinline)) void mem_dealloc(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
    
  myid = (long) pmyid;
  
  free(fAVector_Threads[myid]);
  free(fBVector_Threads[myid]);
  free(fCVector_Threads[myid]);  
} //mem_dealloc

__attribute__((noinline)) void matmul_c_ref() {  //reference kernel - only main thread calls it
  long myid, validateLength, length, i, j, k, l, m;
  float sum;  
  
  sum=0.0;
  for (i = 0; i < ROWS; i++) {    //row
    for (j = 0; j < ROWS; j++) {  //col
      for (k = 0; k < COLS; k++) {  //col
        sum += srcArrayA_4x4[i][k] * srcArrayB_4x4[k][j];  //src1-row * src2-col
      }    
      dstArrayC_4x4[i][j] = sum;
      printf("%-7.1f ", sum);          
      sum=0.0;
    }
    printf("\n");
  }  
} //matmul_c_ref

__attribute__((noinline)) void worker_thread_sync(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  int isValid = 1;  
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
    
  myid = (long) pmyid;
  
  if(DODEBUG) fflush(stdout);  
  if(myid) {
    i=pthread_mutex_lock(&mutex_thread1);		
    if(DODEBUG) printf("THREAD#%02ld: pthread_mutex_lock=%ld\n", myid, i);
      while(!mainThreadReady) {
        i=pthread_cond_wait(&cond_thread1, &mutex_thread1);
        if(DODEBUG) printf("THREAD#%02ld: pthread_cond_wait=%ld\n", myid, i);
      }
    i=pthread_mutex_unlock(&mutex_thread1);		
    if(DODEBUG) printf("THREAD#%02ld: pthread_mutex_unlock=%ld\n", myid, i);
  } else { //THREAD0 = main thread    
//reference kernel - only main thread calls it
    matmul_c_ref();  
      
    i=pthread_mutex_lock(&mutex_thread1);		
    if(DODEBUG) printf("THREAD#%02d: pthread_mutex_lock=%d\n", myid, i);

    mainThreadReady=1;	
    i=pthread_cond_broadcast(&cond_thread1);
    if(DODEBUG) printf("THREAD#%02d: pthread_cond_broadcast=%d\n", myid, i);
      
    i=pthread_mutex_unlock(&mutex_thread1);		
    if(DODEBUG) printf("THREAD#%02d: pthread_mutex_unlock=%d\n", myid, i);      
  }
  fflush(stdout);
  //init output for thread before start
  OutputData_Threads[myid].StartClockticks=0;
  OutputData_Threads[myid].EndClockticks=0;
  OutputData_Threads[myid].StartTimeval.tv_sec=0;
  OutputData_Threads[myid].StartTimeval.tv_usec=0;
  OutputData_Threads[myid].EndTimeval.tv_sec=0;
  OutputData_Threads[myid].EndTimeval.tv_usec=0; 
  OutputData_Threads[myid].Length=0;
  OutputData_Threads[myid].LoopIter=0;
} //worker_thread_sync

void copytime(void *pmyid, struct timeval *StartTimeval, struct timeval *EndTimeval) {
	long myid = (long) pmyid;
	OutputData_Threads[myid].StartTimeval.tv_sec=StartTimeval->tv_sec;
	OutputData_Threads[myid].StartTimeval.tv_usec=StartTimeval->tv_usec;      
	OutputData_Threads[myid].EndTimeval.tv_sec=EndTimeval->tv_sec;      
	OutputData_Threads[myid].EndTimeval.tv_usec=EndTimeval->tv_usec;	
} //copytime

__attribute__((noinline)) void matmul_c(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
  
  volatile uint64_t StartClockticks, EndClockticks;
  struct timeval StartTimeval, EndTimeval;
    
  myid = (long) pmyid;
  
  sum=0.0;
  src1 = (float*)fAVector_Threads[myid];
  src2 = (float*)fBVector_Threads[myid];
  dst = (float*)fCVector_Threads[myid];
  
//sync-point
  worker_thread_sync(pmyid);  
  
  gettimeofday(&StartTimeval, NULL);
  
  for (m = 0; m < InputData.LoopIter; m++) {    //row
    for (l = 0; l < InputData.Length; l++) {    //row
      for (i = 0; i < ROWS; i++) {    //row
        for (j = 0; j < ROWS; j++) {  //col
          for (k = 0; k < COLS; k++) {  //col
            sum += src1[i*ROWS+k+l*ROWS*COLS] * src2[k*ROWS+j+l*ROWS*COLS];  //src1-row * src2-col
          } //k    
          dst[i*ROWS+j+l*ROWS*COLS] = sum;
          //printf("THREAD#%02d: %-7.1f ", myid, sum);          
          sum=0.0;
        } //j
        //printf("\n");
      } //i
      if(anyWorkerThreadDone) break;
      //printf("\n\n");   
    } //l
    if(anyWorkerThreadDone) break;
    sum=0.0;
  } //m
  anyWorkerThreadDone=1;
  
  gettimeofday(&EndTimeval, NULL);
  
  copytime(pmyid, &StartTimeval, &EndTimeval);   
  OutputData_Threads[myid].Length=l;
  OutputData_Threads[myid].LoopIter=m;
  
} //matmul_c

__attribute__((noinline)) void matmul_validate(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  int isValid = 1;  
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
    
  myid = (long) pmyid;
    
  dst = (float*)fCVector_Threads[myid];
  adst = &dstArrayC_4x4[0][0];
  
  if (InputData.Length > 3)
    validateLength=3;
  else
    validateLength =  InputData.Length;
      
  for (l = 0; l < validateLength; l++) {
    for (i = 0; i < ROWS; i++) {
      for (j = 0; j < COLS; j++) {        
        if (*dst != *adst) {
          printf("THREAD#%02ld *** BAD - Results NOT same as reference code l=%ld i=%ld j=%ld dst=%f dstArrayC_4x4=%f \n", myid, l, i, j, *dst, *adst);
          isValid=0;
          break;
        }
        dst++;
        adst++;
      } //j
      if(!isValid) break;
    } //i
    if(!isValid) break;
    adst = &dstArrayC_4x4[0][0];        
  } //l 
  if(isValid) printf("THREAD#%02ld MatMul Validation successfull\n", myid);
  
  OutputData_Threads[myid].isValidTestResult=isValid;
} //matmul_validate

__attribute__((noinline)) void pinCPU(void *pmyid) {
  long myid, i;
  pthread_t thread;
  cpu_set_t cpuset;
  cpu_set_t cpumask;  
  unsigned long cpumask_old;
	unsigned int len;  
  
  if(!doPROCBind) return;
  
  len = sizeof(cpu_set_t);  
  myid = (long) pmyid;      
  thread = pthread_self();

  CPU_SET(myid,&cpuset);
  printf("@@@THREAD#%02ld: cpumask desired=%d\n", myid, cpuset);      
  
  if(DODEBUG) printf("THREAD#%02d: pinCPU pthread_mutex_lock=%d\n", myid, i);  
  
  errno=0;
  
  CPU_ZERO(&cpuset);  
  if(pthread_getaffinity_np(pthread_self(), len, &cpuset) !=0) {
    printf("THREAD#%02ld: pthread_getaffinity_np returned error %s\n", myid, strerror(errno));
    //exit(-1);
  }
  printf("THREAD#%02ld: OLD affinity=%d, cpuset.__bits[0]=%d\n", myid, cpuset, cpuset.__bits[0]);

  CPU_ZERO(&cpuset);
  CPU_SET(myid,&cpuset);
  if(pthread_setaffinity_np(pthread_self(), len, &cpuset) !=0) {
    printf("\t THREAD#%02ld: pthread_setaffinity_np returned error %s\n",myid, strerror(errno));
    //exit(-1);
  }
	CPU_ZERO(&cpuset);
  if(pthread_getaffinity_np(pthread_self(), len, &cpuset) !=0) {
    printf("THREAD#%02ld: pthread_getaffinity_np returned error %s\n", myid, strerror(errno));
    //exit(-1);
  }
  printf("THREAD#%02ld: NEW affinity=%d, cpuset.__bits[0]=%d\n", myid, cpuset, cpuset.__bits[0]);
} //pinCPU

void *worker_thread(void *pmyid) {
  long myid, validateLength, length, i, j, k, l, m;
  float sum;
  
  float *src1, *src2, *dst;
  float *asrc1, *asrc2, *adst;
    
  myid = (long) pmyid;
  printf("THREAD#%02ld START!!!\n", myid);

//cpu affinity  
  pinCPU(pmyid);
  
//init and alloc  
  mem_alloc_init(pmyid);
  
//sync-point
//  worker_thread_sync(pmyid);

//do-work 
  matmul_c(pmyid); 
   
//validate
  matmul_validate(pmyid);
    
//clean-up
  mem_dealloc(pmyid);
  
  if(DODEBUG) fflush(stdout);
  printf("THREAD#%02ld END!!!\n", myid);
  if(DODEBUG) fflush(stdout);
    
  if(myid) pthread_exit((void*)pmyid);
}


__attribute__((noinline)) void Basic_MATMUL_4x4f(int ThreadNo) {
  long i,j,k;
  float sum=0;
  float *ptrC;   
  
    for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
        for (k = 0; k < 4; k++) {
              sum += srcArrayA_4x4[i][k] * srcArrayB_4x4[k][j];
        }
        dstArrayC_4x4[i][j] =sum;
        sum=0.0;
      }
    }

  ptrC= (float *) &dstArrayC_4x4[0];
  for (i=0;i<4*4;i=i+4)
    printf("THREAD%d-Main: OUTPUT func %s row=%d of dstArrayC_4x4[%d]=%-7.3f %-7.3f %-7.3f %-7.3f\n", ThreadNo, __FUNCTION__, (i/4), (i/4), ptrC[i+0], ptrC[i+1], ptrC[i+2], ptrC[i+3]);

  fflush(stdout);
} //eo-Basic_MATMUL_4x4f

int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y) { 
 //code borrowed from http://www.gnu.org/s/libc/manual/html_node/Elapsed-Time.html#Elapsed-Time
 /* Perform the carry for the later subtraction by updating y. */
 if (x->tv_usec < y->tv_usec) {
   int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
   y->tv_usec -= 1000000 * nsec;
   y->tv_sec += nsec;
 }
 if (x->tv_usec - y->tv_usec > 1000000) {
   int nsec = (x->tv_usec - y->tv_usec) / 1000000;
   y->tv_usec += 1000000 * nsec;
   y->tv_sec -= nsec;
 }

 /* Compute the time remaining to wait.
    tv_usec is certainly positive. */
 result->tv_sec = x->tv_sec - y->tv_sec;
 result->tv_usec = x->tv_usec - y->tv_usec;

// printf("sec elapsed=%ld\n", result->tv_sec);
// printf("usec elapsed=%ld\n", result->tv_usec);

 /* Return 1 if result is negative. */
 return x->tv_sec < y->tv_sec;
}

void printResults(void) {
  long i,j,k, iLength, iLoopIter, oLength, oLoopIter;
  double clksPerOP[MAX_CPU_SUPPORT];
  double opsPerSec[MAX_CPU_SUPPORT];  
  double clksPerOP_total=0;
  double opsPerSec_total=0;
  double ops_total=0;
  double tStart, tEnd, SpentTimeInSec=0;
  int isValidTestResult;
  
  uint64_t SpentClockticks;
  struct timeval SpentTimeval;
  
  printf("\n### START ###\n");
  printf("*** Results for MatMul with %d threads\n", numThreadsToRun);
  printf("PROCFreqMHz = %lld\n", PROCFreqMHz);
  printf("RDTSCFreqMHz = %lld\n", RDTSCFreqMHz);
  printf("doPROCBind=%d\n", doPROCBind);
  iLength = InputData.Length;
  printf("InputData.Length=%u\n", iLength);
  iLoopIter = InputData.LoopIter;
  printf("InputData.LoopIter=%u\n", iLoopIter);
  
  printf("\n");  
  for(i=0; i<numThreadsToRun; i++) {
    printf("*** START STAT for THREAD#%02ld\n", i);
    isValidTestResult = OutputData_Threads[i].isValidTestResult;
    if(isValidTestResult)
    	printf("\t$$$TEST VALIDATION PASSED!!!\n");
    else
    	printf("\t@@@TEST VALIDATION FAILED...INVALID TEST RESULTs!!!\n");
    			
    printf("\tstarttime %ldsec %ldusec\n", OutputData_Threads[i].StartTimeval.tv_sec, OutputData_Threads[i].StartTimeval.tv_usec);
    printf("\tendtime %ldsec %ldusec\n", OutputData_Threads[i].EndTimeval.tv_sec, OutputData_Threads[i].EndTimeval.tv_usec);	
	    
    oLength = OutputData_Threads[i].Length;
    oLoopIter = OutputData_Threads[i].LoopIter;    
    printf("\tLength = %u\n", oLength); 
    printf("\tLoopIter = %u\n", oLoopIter); 
    
    if(oLength == 0) 
    	oLength=iLength;    
    if(oLength != iLength) {
      	ops_total = (double)oLength + (double)iLength * (oLoopIter-1);      
    } else { //equal
      ops_total = (double)oLength * oLoopIter;
    }
		
		printf("\tTotal ops = %-7.3f\n", ops_total);
		
		
		timeval_subtract(&SpentTimeval, &OutputData_Threads[i].EndTimeval, &OutputData_Threads[i].StartTimeval);
		SpentTimeInSec=SpentTimeval.tv_sec+(SpentTimeval.tv_usec/1000000.0);
    
   	printf("\tspent time %.6lf seconds(%ldsec + %ldusec)\n", SpentTimeInSec, SpentTimeval.tv_sec, SpentTimeval.tv_usec);
    
    opsPerSec[i] = (ops_total/1000000) / SpentTimeInSec ;
    
    opsPerSec_total += opsPerSec[i];
    
    printf("\tMillion op/sec = %-7.3f\n", opsPerSec[i]);
    printf("\tMillion op/sec w/ normFactor = %-7.3f\n", normFactor*opsPerSec[i]); 
    printf("\n");       
  } //for
   
    printf("*** Op/sec Results for, MatMul %d threads with doPROCBind=%d", numThreadsToRun, doPROCBind);
    printf(",Total Mops=,%-7.3f", opsPerSec_total); 
    if(numThreadsToRun > 1) 
      printf(",Avg Mops=,%-7.3f", opsPerSec_total/numThreadsToRun);      
    for(i=0; i<numThreadsToRun; i++) {
      if(PROCFreqMHz == RDTSCFreqMHz)
        printf(",THREAD#%02ld=, %-7.3f", i, opsPerSec[i]);
      else
        printf(",THREAD#%02ld=, %-7.3f", i, normFactor*opsPerSec[i]);      
    }
    printf("\n");    

  printf("### END ###\n");
  fflush(stdout);

} //PrintResults

int main(int argc, char *argv[]) {
  long i,j, status, myid;
  pid_t pid, pid_child_process, wait_pid_child_process;	
  cpu_set_t new_mask,cur_mask;
  unsigned int len = sizeof(new_mask);
  pthread_t thread2;
  void *thread2_status;
  pthread_attr_t attr; 

  if(argc <6) {
    printf("enter valid value as: loopIterationCount memLength{totalmemsize=memLength*([4*4*4]*3)} numThreadsToRun[1/2/4...16] doPROCBind(0 or 1) PROCFreqMHz RDTSCFreqMHz\nUsage:\n %s 1000 3 2 0 2600 6000 ;#loopIterationCount=1000, memLength=3 i.e. 3*[4*4*4]*3=576bytes numThreadsToRun=2 i.e. run with 2 Threads, doPROCBind=0 i.e. no CPU binding, PROCFreqMHz=2600 (PNR), RDTSCFreqMHz=2600 i.e. rdtsc clks is same as Proc Clks\n", argv[0]);
    exit(-1);
  }
  
  if (atoi(argv[1]) <= 0) {
    printf("using default value of loopIterationCount=%d\n", loopIterationCount);
  } else {
    loopIterationCount = atoi(argv[1]);
  }
  if (atoi(argv[2]) <= 1) {
    printf("using default value of mem Length=1\n");
    InputData.Length=1;
  } else {
    InputData.Length = atoi(argv[2]);
  }  
     //always
  if (atoi(argv[3]) <= 0) {
    printf("numThreadsToRun cannot be 0, using default value of %d\n", numThreadsToRun);
  } else {
    numThreadsToRun = atoi(argv[3]);
  }
  
  doPROCBind =  atoi(argv[4]);
  if(doPROCBind < 0) {
  	doPROCBind = 0;
  	printf("doPROCBind cannot be < 0, using default value of %d\n", doPROCBind);  	
  } else if (doPROCBind > 1) {
  	doPROCBind = 1;
  	printf("doPROCBind cannot be > 1, using default value of %d\n", doPROCBind);  	  	
  }
  PROCFreqMHz = (uint64_t) atoi(argv[5]);
  RDTSCFreqMHz = (uint64_t) atoi(argv[6]);

  InputData.LoopIter = loopIterationCount;
  if (InputData.LoopIter == 0) InputData.LoopIter = 1;
  InputData.DelayCount=0; //always

  normFactor = (1.0 * PROCFreqMHz/RDTSCFreqMHz);
  
  printf("the LIBC version should be > 2.4 for this program, yours is ");
  puts(gnu_get_libc_version());
  
  printf("### START CMDLINE PARAMS ###\n");
    printf("\tfull_cmdline=");
      for(i=0; i<argc; i++) printf("%s ", argv[i]);
    printf("\n");
    printf("\tloopIterationCount = %u\n", loopIterationCount);
    printf("\tInputData.Length=%u\n", InputData.Length);
    printf("\tnumThreadsToRun = %u\n", numThreadsToRun);
    printf("\tdoPROCBind = %d\n", doPROCBind);        
    printf("\tPROCFreqMHz = %lld\n", PROCFreqMHz);
    printf("\tRDTSCFreqMHz = %lld\n", RDTSCFreqMHz);
    printf("other derived/constant params\n");              
    printf("\tnormFactor (PROCFreqMHz/RDTSCFreqMHz) = %-7.3f\n", normFactor);    
    printf("\tInputData.DelayCount=%u\n", InputData.DelayCount);
  printf("### END ###\n\n");

    numCPU=find_num_proc();
    if(numCPU > MAX_CPU_SUPPORT) {
      printf("numCPU=%d > MAX_CPU_SUPPORT=%d, limiting to MAX_CPU_SUPPORT or change MAX_CPU_SUPPORT and re-compile\n", numCPU, MAX_CPU_SUPPORT);
      numCPU = MAX_CPU_SUPPORT;
    }
    if(numThreadsToRun > numCPU ) {
      printf("\n\n*** numThreadsToRun %d > numCPU %d, running only %d threads!!!\n\n", numThreadsToRun, numCPU, numCPU);
      numThreadsToRun=numCPU;
    }   

    myid=0; //Main Thread's ID    
    
    long tstatus;
    pthread_cond_init(&cond_thread1, NULL);		
    pthread_mutex_init(&mutex_thread1, NULL);
  
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
      
    for(i=1; i<numThreadsToRun; i++) {  //i=0 is reserved for Main
      if ( (tstatus=pthread_create( &threads[i], &attr, worker_thread, (void *)i)) ) {
        printf("THREAD#%02d: error code %d when creating thread# %d\n",  myid,tstatus, i);
        abort();
      }	        
    }                
    
    worker_thread((void*)myid);
    
    void *texitstatus;
    for(i=1; i<numThreadsToRun; i++) {  //i=0 is reserved for Main
      if ( tstatus=pthread_join(threads[i], &texitstatus) ) {
        printf("THREAD#%02d: error code %d when creating thread# %d\n", myid, tstatus, i);
        abort();
      }
      printf("THREAD#%02d: thread %d joined with status %d\n", myid, i, texitstatus);	        
    }            	    
    pthread_attr_destroy(&attr);
    pthread_cond_destroy(&cond_thread1);
    pthread_mutex_destroy(&mutex_thread1);        

    printResults();
    
  exit(EXIT_SUCCESS);

}


