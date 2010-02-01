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

#include "i7z.h"

int main(int argc, char *argv[])
{
    int row,col;				/* to store the number of rows and *
					 * the number of colums of the screen */
   unsigned long long int microseconds; /* total time taken */

   struct family_info proc_info;
   char vendor_string[13];
   
	#ifdef x64_BIT 
   get_vendor(vendor_string);
   if (strcmp(vendor_string,"GenuineIntel")==0)
	printf("Found Intel Processor\n");
   else{
	printf("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
	exit(1);
   }    
   #endif

  get_familyinformation(&proc_info);	
  
  //check if its nehalem or exit
  // if (proc_info.family!=6 ||proc_info.processor_type!=0 || proc_info.extended_model!=1){
//	 printf("Not a nehalem (i7)");
	// exit(1);
// }
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
		mvprintw(15+i,0,"\tProcessor %d:  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n",i,_FREQ[i], _MULT[i],C0_time[i]*100,C1_time[i]*100-(C3_time[i]*100+C6_time[i]*100),C3_time[i]*100,C6_time[i]*100); //C0_time[i]*100+C1_time[i]*100 around 100
	  
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
