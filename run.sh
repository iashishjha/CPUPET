#!/bin/sh

#	run.sh - Program for compiling and running the CPUPET tool
#	CPUPET - The CPU Performance Evaluation Tool
#
#	Created by Ashish Jha
#	iashishjha@yahoo.com
#	August 13 2010
#
#	Copyright 2010 Ashish Jha. All rights reserved.
#	This software is licensed under the "The GNU General Public License (GPL) Version 2". Before viewing, copying or distribution this software, please check the full terms of the license in the file LICENSE.txt or COPYING.txt
#

echo "CPUPET START"

#S0. PREREQUESITES for compiling and running this program on Linux
#S0.1. Make sure you have gcc version 4.2 and above. To check type
#	gcc --version
#	It prints something like: gcc (Ubuntu 4.4.3-4ubuntu5) 4.4.3 ==>this is version above 4.2 and so good
#S0.2. Make sure you have LIBC version 2.4 and above. Otherwise, the program won't compile as CPU binding method using pthread API's not supported in versions below 2.4.
#	The program checks at the very beginning and dumps something like: the LIBC version should be > 2.4 for this program, yours is 2.11.1


#S1. compile

export PROC_MODE=x64
export EXE_NAME=CPUPET
export EXE_VER=v1
export EXE=${EXE_NAME}_${EXE_VER}_${PROC_MODE}

rm -f ./${EXE}
make

#S2. Set the proper Processor Frequency and its RDTSC frequency
export PROC_FREQ_MHZ=2800
export RDTSC_FREQ_MHZ=2800
export NUM_PROC=8PHT
export PROC_AND_STEP=XeonX5570

export STACKSIZE=1572906
ulimit -s unlimited

#S3. Set command line input params

#set the # of Threads to run. If set to more than max CPU count, the program set's it to the MAX CPU count.
export NUM_THREADS=1

#set memory size to be set. Each Matrix is MEM_LENGTH * (4*4*sizeof(float)) bytes. Further, Multiply by 3 as there are 3 matrix'es.
export MEM_LENGTH=1000

#set number of loop iterations you want
export LOOP_ITER=10000

#set it to 0 for no CPU binding (OS chooses for you). Set to 1 to bind threads to CPU 0, 1...max_cpu_count in thread creation order.
export PROC_BIND=1

#S4. Run the program on command window
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ}

#S4.1. Run the program to log into a log file
export FILE_NAME=${PROC_AND_STEP}${NUM_PROC}${PROC_FREQ_MHZ}MHz_${EXE}.LI${LOOP_ITER}.ML${MEM_LENGTH}.PB${PROC_BIND}

#S4.2. Make several run - to check for repeatability across runs
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ} >${FILE_NAME}.r1.log
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ} >${FILE_NAME}.r2.log
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ} >${FILE_NAME}.r3.log
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ} >${FILE_NAME}.r4.log
./${EXE} ${LOOP_ITER} ${MEM_LENGTH} ${NUM_THREADS} ${PROC_BIND} ${PROC_FREQ_MHZ} ${RDTSC_FREQ_MHZ} >${FILE_NAME}.r5.log

#S6. To grep results from the log files
cat *.log | grep "Results for,"

#S6.1. To grep results from the log files and log it into a csv file
cat *.log | grep "Results for," >${FILE_NAME}.csv

echo "CPUPET END"