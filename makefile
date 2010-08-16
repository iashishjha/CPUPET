#!/bin/sh

#	makefile - for compiling the CPUPET tool
#	CPUPET - The CPU Performance Evaluation Tool
#
#	Created by Ashish Jha
#	iashishjha@yahoo.com
#	August 13 2010
#
#	Copyright 2010 Ashish Jha. All rights reserved.
#	This software is licensed under the "The GNU General Public License (GPL) Version 2". Before viewing, copying or distribution this software, please check the full terms of the license in the file LICENSE.txt or COPYING.txt
#


CPUPET_v1_x64: CPUPET.c 
	gcc -O3 -g -o CPUPET_v1_x64 -lpthread -lm -w CPUPET.c
