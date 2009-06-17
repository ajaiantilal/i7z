/* This  file is modified from source available at http://www.kernel.org/pub/linux/utils/cpu/msr-tools/
/* for Model specific cpu registers
/* Modified to take i7 into account by Abhishek Jaiantilal abhishek.jaiantilal@colorado.edu

// Information about i7's MSR in 
// http://download.intel.com/design/processor/applnots/320354.pdf
// Appendix B of http://www.intel.com/Assets/PDF/manual/253669.pdf


#ident "$Id: rdmsr.c,v 1.4 2004/07/20 15:54:59 hpa Exp $"
/* ----------------------------------------------------------------------- *
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

#include "version.h"
#include "Intel_CPUID/cputopology.h"

#define ULLONG_MAX 18446744073709551615

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
extern void get_CPUs_info(unsigned int num_Logical_OS, unsigned int num_Logical_process,unsigned int num_Processor_Core,
	unsigned int num_Physical_Socket);
#endif

int main(int argc, char *argv[])
{
    int row,col;				/* to store the number of rows and *
					 * the number of colums of the screen */
   unsigned long long int microseconds; /* total time taken */

   struct family_info proc_info;
   char vendor_string[13];
    
   get_vendor(vendor_string);
   if (strcmp(vendor_string,"GenuineIntel")==0)
	printf("Found Intel Processor\n");
   else{
	printf("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
	exit(1);
  }    
  get_familyinformation(&proc_info);	
  
  //check if its nehalem or exit
 if (proc_info.family!=6 ||proc_info.processor_type!=0 || proc_info.extended_model!=1){
	 printf("Not a nehalem (i7)");
	 exit(1);
 }
  int width=1;
  int i,j;
  
  //MSR number and hi:low bit of that MSR
  int PLATFORM_INFO_MSR = 206; //CE 15:8
  int PLATFORM_INFO_MSR_low = 8; 
  int PLATFORM_INFO_MSR_high = 15; 
	
  int IA32_MISC_ENABLE = 416;
  int TURBO_FLAG_low = 38;	
  int TURBO_FLAG_high = 38;	
	
  int MSR_TURBO_RATIO_LIMIT=429;	
	
  int CPU_NUM;
  int CPU_Multiplier;	
  float BLCK;	
  char TURBO_MODE;

  printf("modprobbing for msr");
  system("modprobe msr"); sleep(1);

  system("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > cpufreq.txt");
  unsigned int num_Logical_OS, num_Logical_process, num_Processor_Core, num_Physical_Socket;
  
  #ifdef USE_INTEL_CPUID
  get_CPUs_info(&num_Logical_OS, &num_Logical_process, &num_Processor_Core, &num_Physical_Socket);
  #endif
  
  int MAX_TURBO_1C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 7,0);
  int MAX_TURBO_2C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 15,8);
  int MAX_TURBO_3C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 23,16);
  int MAX_TURBO_4C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 31,24);

  FILE *cpufreq_file;
  cpufreq_file = fopen("cpufreq.txt","r");
  char cpu_freq_str[30];
  fgets(cpu_freq_str, 30, cpufreq_file);
    
  double cpu_freq_cpuinfo = atof(cpu_freq_str);
  char * pat = "%*lld\n";
  
  initscr();				/* start the curses mode */
  start_color();
  getmaxyx(stdscr,row,col);		/* get the number of rows and columns */
  refresh(); 	
  mvprintw(0,0,"Cpu speed from cpuinfo %0.2fMhz\n",cpu_freq_cpuinfo);  
  mvprintw(1,0,"cpuinfo might be wrong if cpufreq is enabled. To guess correctly try estimating via tsc\n");
  mvprintw(2,0,"Linux's inbuilt cpu_khz code emulated now\n\n");
  
  //for(i=0;i<100;i++){
  cpu_freq_cpuinfo = estimate_MHz();
  mvprintw(3,0,"True Frequency (without accounting Turbo) %f\n",cpu_freq_cpuinfo);
  //}
  CPU_NUM=0;
   
  CPU_Multiplier=get_msr_value(CPU_NUM,PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,PLATFORM_INFO_MSR_low);    
  BLCK = cpu_freq_cpuinfo/CPU_Multiplier;
  mvprintw(4,0,"CPU Multiplier %dx || Bus clock frequency (BCLK) %f MHz \n", CPU_Multiplier, BLCK);
  TURBO_MODE = turbo_status();//get_msr_value(CPU_NUM,IA32_MISC_ENABLE, TURBO_FLAG_high,TURBO_FLAG_low);
  
  //ask linux how many cpus are enabled
  int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
  
  float TRUE_CPU_FREQ;   
  if (TURBO_MODE==1){// && (CPU_Multiplier+1)==MAX_TURBO_2C){
	mvprintw(5,0,"TURBO ENABLED on %d Cores\n",numCPUs);
	TRUE_CPU_FREQ = BLCK*(CPU_Multiplier+1);
  }else{
	mvprintw(5,0,"TURBO DISABLED on %d Cores\n",numCPUs);
	TRUE_CPU_FREQ = BLCK*(CPU_Multiplier);
  }

  mvprintw(6,0,"True Frequency %0.2f MHz\n",TRUE_CPU_FREQ);
  
  
  if(numCPUs>4){
	  mvprintw(7,0,"Max TURBO (if Enabled) with 1,2 Core  %dx\n",MAX_TURBO_1C);
	  mvprintw(8,0,"Max TURBO (if Enabled) with 3,4 Cores %dx\n",MAX_TURBO_2C);
	  mvprintw(9,0,"Max TURBO (if Enabled) with 5,6 Cores %dx\n",MAX_TURBO_3C);
	  mvprintw(10,0,"Max TURBO (if Enabled) with 7,8 Cores %dx\n",MAX_TURBO_4C); 
	  mvprintw(20,0,"C0 = Processor running without halting");
	 mvprintw(21,0,"C1 = Processor running with halts (States >C0 are power saver)");
	 mvprintw(22,0,"C3 = Cores running with PLL turned off and core cache turned off");
	 mvprintw(23,0,"C6 = Everything in C3 + core state saved to last level cache");
	 mvprintw(24,0,"  Above values in table are in percentage over the last 1 sec");
	 #ifdef USE_INTEL_CPUID
  	 mvprintw(25,0," Total cores seen [%d] (to OS)   [%d] (to process) \n",num_Logical_OS, num_Logical_process);
	 mvprintw(26,0," Total physical cores [%d], Total Sockets [%d]\n", num_Processor_Core, num_Physical_Socket);
	 #endif
  }else{
	  mvprintw(7,0," Max TURBO (if Enabled) with 1 Core  %dx\n",MAX_TURBO_1C);
	  mvprintw(8,0," Max TURBO (if Enabled) with 2 Cores %dx\n",MAX_TURBO_2C);
	  mvprintw(9,0," Max TURBO (if Enabled) with 3 Cores %dx\n",MAX_TURBO_3C);
	  mvprintw(10,0," Max TURBO (if Enabled) with 4 Cores %dx\n",MAX_TURBO_4C); 
	  mvprintw(20,0,"C0 = Processor running without halting");
	  mvprintw(21,0,"C1 = Processor running with halts (States >C0 are power saver)");
	  mvprintw(22,0,"C3 = Cores running with PLL turned off and core cache turned off");
	  mvprintw(23,0,"C6 = Everything in C3 + core state saved to last level cache");
	  mvprintw(24,0,"  Above values in table are in percentage over the last 1 sec");
	  #ifdef USE_INTEL_CPUID
  	  mvprintw(25,0," Total cores seen [%d] (to OS)   [%d] (to process) \n",num_Logical_OS, num_Logical_process);
	  mvprintw(26,0," Total physical cores [%d], Total Sockets [%d]\n", num_Processor_Core, num_Physical_Socket);
	  #endif
   }
   mvprintw(28,0,"  Ctrl+C to exit");
	  
   
  int IA32_PERF_GLOBAL_CTRL=911;//3BF
  int IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
  int IA32_FIXED_CTR_CTL=909; //38D
  int IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);
  
  //printf("IA32_PERF_GLOBAL_CTRL %d\n",IA32_PERF_GLOBAL_CTRL_Value);
  //printf("IA32_FIXED_CTR_CTL %d\n",IA32_FIXED_CTR_CTL_Value);
  
  unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF, CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;
  
  CPU_CLK_UNHALTED_CORE=get_msr_value(CPU_NUM,778, 63,0);
  CPU_CLK_UNHALTED_REF   =get_msr_value(CPU_NUM,779, 63,0);
  
  unsigned long int old_val_CORE[numCPUs],new_val_CORE[numCPUs];
  unsigned long int old_val_REF[numCPUs],new_val_REF[numCPUs];
  unsigned long int old_val_C3[numCPUs],new_val_C3[numCPUs];
  unsigned long int old_val_C6[numCPUs],new_val_C6[numCPUs];
  unsigned long int old_val_C1[numCPUs],new_val_C1[numCPUs];
  
  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];
  
  struct timezone tz;
  struct timeval tvstart[numCPUs], tvstop[numCPUs];
 
  struct timespec one_second_sleep;
  one_second_sleep.tv_sec=0;
  one_second_sleep.tv_nsec=999999999;// 1000msec
  
  
  unsigned long int IA32_MPERF = get_msr_value(CPU_NUM,231, 7,0);
  unsigned long int IA32_APERF = get_msr_value(CPU_NUM,232, 7,0);
  mvprintw(12,0,"Wait...\n"); refresh();
   nanosleep(&one_second_sleep,NULL);
   IA32_MPERF = get_msr_value(CPU_NUM,231, 7,0) - IA32_MPERF;	 
   IA32_APERF  = get_msr_value(CPU_NUM,232, 7,0) - IA32_APERF;
   
   //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);
  
  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs], C6_time[numCPUs];
  double _FREQ[numCPUs], _MULT[numCPUs];
  refresh();
    
  mvprintw(12,0,"Current Freqs\n");
  
  for (i=0;i<numCPUs;i++){
	 CPU_NUM=i;
	IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
	set_msr_value(CPU_NUM, IA32_PERF_GLOBAL_CTRL,0x700000003);
	IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);
	set_msr_value(CPU_NUM, IA32_FIXED_CTR_CTL,819);	  
	IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
	IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);

	old_val_CORE[i] = get_msr_value(CPU_NUM,778, 63,0);
	old_val_REF[i]    = get_msr_value(CPU_NUM,779, 63,0);
	old_val_C3[i]	= get_msr_value(CPU_NUM,1020, 63,0); 
	old_val_C6[i]	= get_msr_value(CPU_NUM,1021, 63,0); 
	old_TSC[i] = rdtsc();
 }     
	  
 
 
  for (;;){
	 nanosleep(&one_second_sleep,NULL);
	  mvprintw(14,0,"\tProcessor  :Actual Freq (Mult.)\tC0\% Halt(C1) \%\t  C3 \%\t C6 \%\n");
		
	  for (i=0;i<numCPUs;i++){
		CPU_NUM=i;
		new_val_CORE[i] = get_msr_value(CPU_NUM,778, 63,0);
		new_val_REF[i]    = get_msr_value(CPU_NUM,779, 63,0);
		new_val_C3[i]	 = get_msr_value(CPU_NUM,1020, 63,0); 
		new_val_C6[i]	 = get_msr_value(CPU_NUM,1021, 63,0); 
		//gettimeofday(&tvstop[i], &tz);
		new_TSC[i] = rdtsc();
		if (old_val_CORE[i]>new_val_CORE[i]){
			 CPU_CLK_UNHALTED_CORE=(3.40282366921e38-old_val_CORE[i]) +new_val_CORE[i];  	
		}else { 
			CPU_CLK_UNHALTED_CORE=new_val_CORE[i] - old_val_CORE[i];	  
		}
		
		//number of TSC cycles while its in halted state
		if ((new_TSC[i] - old_TSC[i]) <  CPU_CLK_UNHALTED_CORE)
			CPU_CLK_C1=0;
		else
			CPU_CLK_C1 = ((new_TSC[i] - old_TSC[i]) - CPU_CLK_UNHALTED_CORE);
		
		if(old_val_REF[i]>new_val_REF[i]){
			CPU_CLK_UNHALTED_REF=(3.40282366921e38-old_val_REF[i]) +new_val_REF[i];
		}else{
			CPU_CLK_UNHALTED_REF=new_val_REF[i] - old_val_REF[i];	   
		}
		
		if(old_val_C3[i]>new_val_C3[i]){
			CPU_CLK_C3=(3.40282366921e38-old_val_C3[i]) +new_val_C3[i];
		}else{
			CPU_CLK_C3=new_val_C3[i] - old_val_C3[i];	   
		}
		
		if(old_val_C6[i]>new_val_C6[i]){
			CPU_CLK_C6=(3.40282366921e38-old_val_C6[i]) +new_val_C6[i];
		}else{
			CPU_CLK_C6=new_val_C6[i] - old_val_C6[i];	   
		}
			
		//microseconds = ((tvstop[i].tv_sec-tvstart[i].tv_sec)*1000000) +	(tvstop[i].tv_usec-tvstart[i].tv_usec);
		//printf("elapsed secs %llu\n",microseconds);
		//printf("%u %u\n",CPU_CLK_UNHALTED_CORE,CPU_CLK_UNHALTED_REF);
		//printf("%llu\n",CPU_CLK_UNHALTED_CORE);
		//CPU_CLK_UNHALTED_REF = CPU_CLK_UNHALTED_REF;
		//CPU_CLK_UNHALTED_CORE = CPU_CLK_UNHALTED_CORE;
		//printf("%llu %llu %llu\n",CPU_CLK_UNHALTED_CORE,CPU_CLK_UNHALTED_REF,(new_TSC[i]-old_TSC[i]));
		_FREQ[i] =   estimate_MHz()*((long double)CPU_CLK_UNHALTED_CORE/(long double)CPU_CLK_UNHALTED_REF);  
		_MULT[i] = _FREQ[i]/BLCK;
		
		C0_time[i]=((long double)CPU_CLK_UNHALTED_REF/(long double)(new_TSC[i]-old_TSC[i]));
		C1_time[i]=((long double)CPU_CLK_C1/(long double)(new_TSC[i]-old_TSC[i]));
		C3_time[i]=((long double)CPU_CLK_C3/(long double)(new_TSC[i]-old_TSC[i]));
		C6_time[i]=((long double)CPU_CLK_C6/(long double)(new_TSC[i]-old_TSC[i]));
		if (C0_time[i]<1e-2)
			if (C0_time[i]>1e-4)
				C0_time[i]=0.01;
			else
				C0_time[i]=0;
		
		if (C1_time[i]<1e-2)
			if (C1_time[i]>1e-4)
				C1_time[i]=0.01;
			else
				C1_time[i]=0;
		
		if (C3_time[i]<1e-2)
			if (C3_time[i]>1e-4)
				C3_time[i]=0.01;
			else
				C3_time[i]=0;
		
		if (C6_time[i]<1e-2)
			if (C6_time[i]>1e-4)
				C6_time[i]=0.01;
			else
				C6_time[i]=0;
		
		//printf("\n --%llu %llu--\t \n", old_val_C3[i], new_val_C3[i]);			
		//printf("\n --%llu %llu--\t \n", old_val_C6[i], new_val_C6[i]);	
		//printf("\t %0.2Lg\t \n", C0_time, CPU_CLK_UNHALTED_REF, (new_TSC[i]-old_TSC[i]));
	  }
	  
	 for(i=0;i<4;i++)
		mvprintw(15+i,0,"\tProcessor %d:  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n",i,_FREQ[i], _MULT[i],C0_time[i]*100,C1_time[i]*100,C3_time[i]*100,C6_time[i]*100); //C0_time[i]*100+C1_time[i]*100 around 100
	  
	TRUE_CPU_FREQ=0;
	for(i=0;i<4;i++)
	    if(_FREQ[i]>  TRUE_CPU_FREQ)
		    TRUE_CPU_FREQ = _FREQ[i];
	 mvprintw(13,0,"True Frequency %0.2f MHz (Intel specifies largest of below to be running Freq)\n",TRUE_CPU_FREQ);
	   
	refresh();
	memcpy(old_val_CORE ,new_val_CORE,sizeof(unsigned long int)*4);
	memcpy(old_val_REF ,new_val_REF,sizeof(unsigned long int)*4);
	memcpy(old_val_C3 ,new_val_C3,sizeof(unsigned long int)*4);
	memcpy(old_val_C6 ,new_val_C6,sizeof(unsigned long int)*4);
	memcpy(tvstart,tvstop,sizeof(struct timeval)*4);
	memcpy(old_TSC,new_TSC,sizeof(unsigned long long int)*4);
	
  }
  exit(0);
}
