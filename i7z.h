/* This  file is modified from source available at http://www.kernel.org/pub/linux/utils/cpu/msr-tools/
 for Model specific cpu registers
 Modified to take i7 into account by Abhishek Jaiantilal abhishek.jaiantilal@colorado.edu

// Information about i7's MSR in 
// http://download.intel.com/design/processor/applnots/320354.pdf
// Appendix B of http://www.intel.com/Assets/PDF/manual/253669.pdf


#ident "$Id: rdmsr.c,v 1.4 2004/07/20 15:54:59 hpa Exp $"
 ----------------------------------------------------------------------- *
 *   
 *   Copyright 2000 Transmeta Corporation - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <memory.h>
#include <ncurses.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

//#include "version.h"

//#define ULLONG_MAX 18446744073709551615

#ifndef x64_BIT
//http://www.mcs.anl.gov/~kazutomo/rdtsc.html
//code for 64 bit
__inline__ unsigned long long int rdtsc()
{
	unsigned long long int x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}
#endif

#ifdef x64_BIT
static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

struct family_info{
    char stepping;
    char model;
    char family;
    char processor_type;
    char extended_model;
    int extended_family;
};

void print_family_info(struct family_info *proc_info){
//print CPU info
	printf("    Stepping %x\n", proc_info->stepping);
	printf("    Model %x\n", proc_info->model);
	printf("    Family %x\n", proc_info->family);
	printf("    Processor Type %x\n", proc_info->processor_type);
	printf("    Extended Model %x\n", proc_info->extended_model);
	//    printf("    Extended Family %x\n", (short int*)(&proc_info->extended_family));
	//    printf("    Extended Family %d\n", proc_info->extended_family);
}


#ifdef x64_BIT
void get_vendor(char* vendor_string){
//get vendor name
        unsigned int b,c,d,e;
        int i;
        asm volatile (   "mov %4, %%eax; " // 0 into eax
                "cpuid;"                
                "mov %%eax, %0;"  // eeax into b
                "mov %%ebx, %1;"  // eebx into c
                "mov %%edx, %2;"  // eeax into d
                "mov %%ecx, %3;"  // eeax into e                
                :"=r"(b),"=r"(c),"=r"(d),"=r"(e)        /* output */
                :"r"(0)         /* input */
                :"%eax","%ebx","%ecx","%edx"         /* clobbered register, will be modifying inside the asm routine so dont use them */
        );        
        memcpy(vendor_string  ,&c,4);
        memcpy(vendor_string+4,&d,4);
        memcpy(vendor_string+8,&e,4);
        vendor_string[12]='\0';
//        printf("Vendor %s\n",vendor_string);
}
#endif

int turbo_status(){
//turbo state flag
        unsigned int eax;
        int i;
        asm volatile (   "mov %1, %%eax; " // 0 into eax
                "cpuid;"                
                "mov %%eax, %0;"  // eeax into b
                :"=r"(eax)       /* output */
                :"r"(6)         /* input */
                :"%eax"         /* clobbered register, will be modifying inside the asm routine so dont use them */
        );        
        
        //printf("eax %d\n",(eax&0x2)>>1);
	
	return((eax&0x2)>>1);	
}

void get_familyinformation(struct family_info *proc_info){
  //get info about CPU
	unsigned int b;
	asm volatile(   "mov %1, %%eax; " // 0 into eax
			"cpuid;"                
			"mov %%eax, %0;"  // eeax into b
			:"=r"(b)        /* output */
			:"r"(1)         /* input */
			:"%eax"         /* clobbered register, will be modifying inside the asm routine so dont use them */
	);        
	printf("eax %x\n",b);
	proc_info->stepping= b & 0x0000000F; //bits 3:0
	proc_info->model   = (b & 0x000000F0)>>4; //bits 7:4
	proc_info->family  = (b & 0x00000F00)>>8; //bits 11:8
	proc_info->processor_type  = (b & 0x00007000)>>12; //bits 13:12
	proc_info->extended_model  = (b & 0x000F0000)>>16; //bits 19:16
	proc_info->extended_family = (b & 0x0FF00000)>>20; //bits 27:20
}

double estimate_MHz(){
	//copied blantantly from http://www.cs.helsinki.fi/linux/linux-kernel/2001-37/0256.html
/*
* $Id: MHz.c,v 1.4 2001/05/21 18:58:01 davej Exp $
* This file is part of x86info.
* (C) 2001 Dave Jones.
*
* Licensed under the terms of the GNU GPL License version 2.
*
* Estimate CPU MHz routine by Andrea Arcangeli <andrea@suse.de>
* Small changes by David Sterba <sterd9am@ss1000.ms.mff.cuni.cz>
*
*/
	struct timezone tz;
	struct timeval tvstart, tvstop;
	unsigned long long int cycles[2]; /* gotta be 64 bit */
	unsigned long long int microseconds; /* total time taken */

	memset(&tz, 0, sizeof(tz));

	/* get this function in cached memory */
	gettimeofday(&tvstart, &tz);
	cycles[0] = rdtsc();
	gettimeofday(&tvstart, &tz);

	/* we don't trust that this is any specific length of time */
	//1 sec will cause rdtsc to overlap multiple times perhaps. 100msecs is a good spot
	usleep(100000);

	cycles[1] = rdtsc();
	gettimeofday(&tvstop, &tz);
	microseconds = ((tvstop.tv_sec-tvstart.tv_sec)*1000000) +
	(tvstop.tv_usec-tvstart.tv_usec);
	
	unsigned long long int elapsed=0;
	if (cycles[1]<cycles[0]){
		//printf("c0 = %llu   c1 = %llu",cycles[0],cycles[1]);
		elapsed  = UINT32_MAX-cycles[0];
		elapsed  = elapsed+cycles[1];		
		//printf("c0 = %llu  c1 = %llu max = %llu elapsed=%llu\n",cycles[0], cycles[1], UINT32_MAX,elapsed);		
	}else{
		elapsed = cycles[1]-cycles[0];
		//printf("\nc0 = %llu  c1 = %llu elapsed=%llu\n",cycles[0], cycles[1],elapsed);		
	}
	
	double mhz = elapsed/microseconds;
	
	
	//printf("%llg MHz processor (estimate).  diff cycles=%llu  microseconds=%llu \n", mhz, elapsed, microseconds);
	//printf("%g  elapsed %llu  microseconds %llu\n",mhz, elapsed, microseconds);
	return(mhz);
}

/* Number of decimal digits for a certain number of bits */
/* (int) ceil(log(2^n)/log(10)) */
int decdigits[] = {
  1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10,
  10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15,
  15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19,
  20
};

#define mo_hex  0x01
#define mo_dec  0x02
#define mo_oct  0x03
#define mo_raw  0x04
#define mo_uns  0x05
#define mo_chx  0x06
#define mo_mask 0x0f
#define mo_fill 0x40
#define mo_c    0x80

const char *program;


uint64_t get_msr_value(int cpu, uint32_t reg,unsigned int highbit, unsigned int lowbit){
	uint64_t data;
	int c, fd;
	char *pat;
	int width;
	char msr_file_name[64];
	int bits;
	

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_RDONLY);
	if ( fd < 0 ) {
	    if ( errno == ENXIO ) {
	      fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
	      exit(2);
	    } else if ( errno == EIO ) {
	      fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
	      exit(3);
	    } else {
	      perror("rdmsr:open");
	      exit(127);
	    }
	}
	  
	if ( pread(fd, &data, sizeof data, reg) != sizeof data ) {
	    perror("rdmsr:pread");
	    exit(127);
	}

	close(fd);

	bits = highbit-lowbit+1;
	if ( bits < 64 ) {
	   /* Show only part of register */
	    data >>= lowbit;
	    data &= (1ULL << bits)-1;
	}

	pat = NULL;

	/* Make sure we get sign correct */
	if ( data & (1ULL << (bits-1)) ) {
	     data &= ~(1ULL << (bits-1));
	     data = -data;
	}
	pat = "%*lld\n";
	  
	width = 1;			/* Default */
	  
	if ( width < 1 )
	    width = 1;
	
	return(data);
}

uint64_t set_msr_value(int cpu, uint32_t reg,uint64_t data){
	int c, fd;
	char *pat;
	int width;
	char msr_file_name[64];
	int bits;

	  sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	  fd = open(msr_file_name, O_WRONLY);
	  if ( fd < 0 ) {
	    if ( errno == ENXIO ) {
	      fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
	      exit(2);
	    } else if ( errno == EIO ) {
	      fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n", cpu);
	      exit(3);
	    } else {
	      perror("wrmsr:open");
	      exit(127);
	    }
	  }
	  
	  if ( pwrite(fd, &data, sizeof data, reg) != sizeof data ) {
	      perror("wrmsr:pwrite");
	      exit(127);
	    }
}


#ifdef USE_INTEL_CPUID
void get_CPUs_info(unsigned int* num_Logical_OS, unsigned int* num_Logical_process,unsigned int* num_Processor_Core,
	unsigned int* num_Physical_Socket);
#endif


