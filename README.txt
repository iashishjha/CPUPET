EADME.txt - README for the CPUPET Tool
CPUPET - The CPU Performance Evaluation Tool

Created by Ashish Jha
iashishjha@yahoo.com
August 13 2010

COPYRIGHT 2010 Ashish Jha. All rights reserved.
This software is licensed under the "The GNU General Public License (GPL) Version 2". Before viewing, copying or distribution this software, please check the full  terms of the license in the file LICENSE.txt or COPYING.txt

CPUPET is an Open source tool for evaluating performance of CPU and its surrounding systems like the memory, inter-connect, etc. It can also be used to evaluate multi-core systems to look at scaling across cores, full memory bandwidth and cache sharing issues as well as evulate OS scheduling policies when number of threads are less than number of logical CPU's.

The following steps would guide you through running and using the tool. The steps are also layed out in run.sh

S1. Goto 	http://github.com/iashishjha/CPUPET
	
S1.1. Full clone could also be done by
	git clone git@github.com:iashishjha/CPUPET.git
	
	If you are interested in further developing the CPUPET tool, please feel free to email me at iashishjha@yahoo.com for secret key to clone my project.

S2. Download complete tar file or download individual files

	create dir CPUPET
	
	cd CPUPET

S2.1. Download the distribution CPUPET_v1.tar.gz to some dir		
		tar -xvzf CPUPET_v1.tar.gz
	
		cd v1
		goto step S3
		
S2.2. Or Download the individual source files
	mkdir v1	
	cd v1
	
	Now download the individual source files from http://github.com/iashishjha/CPUPET to this dir i.e. CPUPET/v1
		CPUPET.c
		makefile
		run.sh
		FAQ.txt
		README.txt

S3. vi run.sh and enter	
	The shell file walks through every step which is fully detailed. It also compiles the source file or you could also compile source files by typing
	make
	Enter - the program compiles and creates an executable called CPUPET_v1_x64
	
	Make sure you have gcc version >=4.2 and GLIBC version >= 2.4
	
	For gcc version type 
	gcc --version
	
	which should give you something like:
		gcc (Ubuntu 4.4.3-4ubuntu5) 4.4.3

	The program if compiled by GCC would be able to print out the GLIBC version. The CPUPET can only compile and work with GCC >= 4.2 and GLIBC 2.4 which have proper support for CPU affinity using pthreads API's.
	
	Once compiled, change any of the input parameters in run.sh such as
	#set the # of Threads to run. If set to more than max CPU count, the program set's it to the MAX CPU count.
	export NUM_THREADS=1

	#set memory size to be set. Each Matrix is MEM_LENGTH * (4*4*sizeof(float)) bytes. Further, Multiply by 3 as there are 3 matrix'es.
	export MEM_LENGTH=1000

	#set number of loop iterations you want
	export LOOP_ITER=10000

	#set it to 0 for no CPU binding (OS chooses for you). Set to 1 to bind threads to CPU 0, 1...max_cpu_count in thread creation order.
	export PROC_BIND=1

S4. Run the program on command window
	./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ}

	one such example is:
	./CPUPET_v1_x64 10000 1000 1 1 2800 2800
	
	NOTE: Run the program to log into a log file and Make several run - to check for repeatability across runs

S5. Checking Results

S5.1. For direct runs without logging into a log file, look at line which begins with  "*** Op/sec Results for,"
	*** Op/sec Results for, MatMul 1 threads with doPROCBind=1,Total Mops=,25.777 ,THREAD#00=, 25.777
	
	Note that there is lots of data output which would aid in debugging and also help verify results. But the final result as shown above is what is most important.
	
S5.2. You could grep results from the log files. Check the sample log files (*.log) from runs on my machine.
	cat *.log | grep "Results for,"
	
	which would show something like (for 2 runs logged into 2 different log files):
	*** Op/sec Results for, MatMul 1 threads with doPROCBind=1,Total Mops=,25.777 ,THREAD#00=, 25.777
	*** Op/sec Results for, MatMul 1 threads with doPROCBind=1,Total Mops=,25.617 ,THREAD#00=, 25.617	
	
S5.2. You could also grep results from the log files and log it into a csv file for later viewing. Helps when you make lots of run with different input configuration parameters. Check the sample *.csv file for results from sample *.log files as run on my machine.
	cat *.log | grep "Results for," >${FILE_NAME}.csv	
	
	To aid in data nalysis, you could open csv file using openoffice on linux or even bring it onto a windows system.
	
S6. Happy analysis of results!	


		

