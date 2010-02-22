//i7z.c
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2009 Abhishek Jaiantilal
 *
 *   Under GPL v2
 *
 * ----------------------------------------------------------------------- */

#include <memory.h>
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
#include <ncurses.h>

#include "i7z.h"

//these variables if put inside the main get corrupted somehow :(
//Doesn't do that right now after -O2 was removed from gcc
//Dont want to mess up, so not moving them
int numPhysicalCores, numLogicalCores;
double TRUE_CPU_FREQ;

//Info: I start from index 1 when i talk about cores on CPU

int main (int argc, char *argv[])
{
  int row, col;	/* to store the number of rows and    *
				 * the number of colums of the screen *
				 * for NCURSES                        */

  struct family_info proc_info;

#ifdef x64_BIT
  char vendor_string[13];
  get_vendor (vendor_string);
  if (strcmp (vendor_string, "GenuineIntel") == 0)
    printf ("i7z DEBUG: Found Intel Processor\n");
  else
    {
      printf
	("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
      exit (1);
    }
#endif

#ifndef x64_BIT
  //anecdotal evidence: get_vendor doesnt seem to work on 32-bit   
  printf
    ("I dont know the CPUID code to check on 32-bit OS, so i will assume that you have an Intel processor\n");
  printf ("Don't worry if i don't find a nehalem next, i'll quit anyways\n");
#endif

  get_familyinformation (&proc_info);
  print_family_info (&proc_info);

  //printf("%x %x",proc_info.extended_model,proc_info.family);

  //check if its nehalem or exit
  //Info from page 641 of Intel Manual 3B
  //Extended model and Model can help determine the right cpu
  printf("i7z DEBUG: msr = Model Specific Register\n");
  if (proc_info.family == 0x6)
  {
    if (proc_info.extended_model == 0x1)
	{
	  switch (proc_info.model)
	    {
	    case 0xA:
	      printf ("i7z DEBUG: Detected a nehalem (i7)\n");
	      break;
	    case 0xE:
	    case 0xF:
	      printf ("i7z DEBUG: Detected a nehalem (i7/i5)\n");
	      break;
	    default:
	      printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
	      return (1);
	    }
	}else if (proc_info.extended_model == 0x2)
	 	{
		  switch (proc_info.model)
    	  {
			case 0xE:
			  printf ("i7z DEBUG: Detected a nehalem (Xeon)\n");
			  break;
			case 0x5:
			case 0xC:
			  printf ("i7z DEBUG: Detected a nehalem (32nm Westmere)\n");
			  break;
			default:
			  printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
			  return (1);
    	  }
	}else{
      printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
      return (1);
	}
  }else{
      printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
      exit (1);
  }

  //iterator
  int i;

  //cpu multiplier
  int CPU_Multiplier;
  //current blck value
  float BLCK;
  //turbo_mode enabled/disabled flag
  char TURBO_MODE;


  //test if the msr file exists
  if (access ("/dev/cpu/0/msr", F_OK) == 0)
  {
      printf ("i7z DEBUG: msr device files exist /dev/cpu/*/msr\n");
      if (access ("/dev/cpu/0/msr", W_OK) == 0)
	  {
		  //a system mght have been set with msr allowable to be written
		  //by a normal user so...
		  //Do nothing.
		  printf ("i7z DEBUG: You have write permissions to msr device files\n");
	  }else{
		  printf ("i7z DEBUG: You DONOT have write permissions to msr device files\n");
		  printf ("i7z DEBUG: A solution is to run this program as root\n");
		  return (1);
	  }
  }else{
      printf ("i7z DEBUG: msr device files DONOT exist, trying out a makedev script\n");
      if (geteuid () == 0)
      {
		  //Try the Makedev script
		  system ("msr_major=202; \
						cpuid_major=203; \
						n=0; \
						while [ $n -lt 16 ]; do \
							mkdir -m 0755 -p /dev/cpu/$n; \
							mknod /dev/cpu/$n/msr -m 0600 c $msr_major $n; \
							mknod /dev/cpu/$n/cpuid -m 0444 c $cpuid_major $n; \
							n=`expr $n + 1`; \
						done; \
						");
		  printf ("i7z DEBUG: modprobbing for msr\n");
		  system ("modprobe msr");
	  }else{
		  printf ("i7z DEBUG: You DONOT have root privileges, mknod to create device entries won't work out\n");
		  printf ("i7z DEBUG: A solution is to run this program as root\n");
		  return (1);
      }
  }

  sleep (1);

  ////To find out if Turbo is enabled use the below msr and bit 38
  ////bit for TURBO is 38
  ////msr reading is now moved into tubo_status
  //int IA32_MISC_ENABLE = 416;
  //int TURBO_FLAG_low = 38;
  //int TURBO_FLAG_high = 38;



  //Use Core-1 as the one to check for the turbo limit
  //Core number shouldnt matter
  int CPU_NUM = 0;
  //bits from 0-63 in this store the various maximum turbo limits
  int MSR_TURBO_RATIO_LIMIT = 429;
  // 3B defines till Max 4 Core and the rest bit values from 32:63 were reserved.  
  //Bits:0-7  - core1
  int MAX_TURBO_1C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 7, 0);
  //Bits:15-8 - core2
  int MAX_TURBO_2C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 15, 8);
  //Bits:23-16 - core3
  int MAX_TURBO_3C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 23, 16);
  //Bits:31-24 - core4
  int MAX_TURBO_4C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 31, 24);

  //gulftown/Hexacore support
  //technically these should be the bits to get for core 5,6
  //Bits:39-32 - core4
  int MAX_TURBO_5C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 39, 32);
  //Bits:47-40 - core4
  int MAX_TURBO_6C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 47, 40);



  //CPUINFO is wrong for i7 but correct for the number of physical and logical cores present
  //If Hyperthreading is enabled then, multiple logical processors will share a common CORE ID
  //http://www.redhat.com/magazine/022aug06/departments/tips_tricks/
  system
    ("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > /tmp/cpufreq.txt");
  system
    ("grep \"core id\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numPhysical.txt");
  system
    ("grep \"processor\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numLogical.txt");
  //At this step, /tmp/numPhysical contains number of physical cores in machine and 
  //                      /tmp/numPhysical contains number of logical cores in machine 


  //Open the parsed cpufreq file and obtain the cpufreq from /proc/cpuinfo
  FILE *tmp_file;
  tmp_file = fopen ("/tmp/cpufreq.txt", "r");
  char tmp_str[30];
  fgets (tmp_str, 30, tmp_file);
  double cpu_freq_cpuinfo = atof (tmp_str);
  fclose (tmp_file);

  //Parse the numPhysical and numLogical file to obtain the number of physical and logical core
  tmp_file = fopen ("/tmp/numPhysical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numPhysicalCores = atoi (tmp_str);
  fclose (tmp_file);

  tmp_file = fopen ("/tmp/numLogical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numLogicalCores = atoi (tmp_str);
  fclose (tmp_file);
  //reading of the number of cores is done

  fflush (stdout);
  sleep (1);

  //Setup stuff for ncurses
  initscr ();			/* start the curses mode */
  start_color ();
  getmaxyx (stdscr, row, col);	/* get the number of rows and columns */
  refresh ();
  //Setup for ncurses completed

  //Print a slew of information on the ncurses window
  mvprintw (0, 0, "Cpu speed from cpuinfo %0.2fMhz\n", cpu_freq_cpuinfo);
  mvprintw (1, 0,
	    "cpuinfo might be wrong if cpufreq is enabled. To guess correctly try estimating via tsc\n");
  mvprintw (2, 0, "Linux's inbuilt cpu_khz code emulated now\n\n");


  //estimate the freq using the estimate_MHz() code that is almost mhz accurate
  cpu_freq_cpuinfo = estimate_MHz ();
  mvprintw (3, 0, "True Frequency (without accounting Turbo) %f\n",
	    cpu_freq_cpuinfo);


  //MSR number and hi:low bit of that MSR
  //This msr contains a lot of stuff, per socket wise
  //one can pass any core number and then get in multiplier etc 
  int PLATFORM_INFO_MSR = 206;	//CE 15:8
  int PLATFORM_INFO_MSR_low = 8;
  int PLATFORM_INFO_MSR_high = 15;

  //We just need one CPU (we use Core-1) to figure out the multiplier and the bus clock freq.
  //multiplier doesnt automatically include turbo
  //note turbo is not guaranteed, only promised
  //So this msr will only reflect the actual multiplier, rest has to be figured out
  CPU_NUM = 0;
  CPU_Multiplier =
    get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,
		   PLATFORM_INFO_MSR_low);

  //Blck is basically the true speed divided by the multiplier
  BLCK = cpu_freq_cpuinfo / CPU_Multiplier;
  mvprintw (4, 0,
	    "CPU Multiplier %dx || Bus clock frequency (BCLK) %0.3f MHz \n",
	    CPU_Multiplier, BLCK);

  //Get turbo mode status by reading msr within turbo_status
  TURBO_MODE = turbo_status ();	//get_msr_value(CPU_NUM,IA32_MISC_ENABLE, TURBO_FLAG_high,TURBO_FLAG_low);

  //to find how many cpus are enabled, we could have used sysconf but that will just give the logical numbers
  //if HT is enabled then the threads of the same core have the same C-state residency number so...
  //Its imperative to figure out the number of physical and number of logical cores.
  //sysconf(_SC_NPROCESSORS_ONLN);

  //number of CPUs is as told via cpuinfo 
  int numCPUs = numPhysicalCores;

  //Flags and other things about HT.
  int HT_ON;
  char HT_ON_str[30];

  //HT enabled if num logical > num physical cores
  if (numLogicalCores > numPhysicalCores)
    {
      //printf("Multiplier %d \n", CPU_Multiplier);
      strncpy (HT_ON_str, "Hyper Threading ON\0", 30);
      HT_ON = 1;
      //printf("Multiplier %d \n", CPU_Multiplier);
    }
  else
    {
      //printf("Multiplier %d \n", CPU_Multiplier);
      strncpy (HT_ON_str, "Hyper Threading OFF\0", 30);
      HT_ON = 0;
      //printf("Multiplier %d \n", CPU_Multiplier);
    }

  //printf("Multiplier %d \n", CPU_Multiplier);

  if (TURBO_MODE == 1)
    {				// && (CPU_Multiplier+1)==MAX_TURBO_2C){
      mvprintw (5, 0, "TURBO ENABLED on %d Cores, %s\n", numPhysicalCores,
		HT_ON_str);
      TRUE_CPU_FREQ = BLCK * ((double) CPU_Multiplier + 1);
      mvprintw (6, 0, "True Frequency %0.2f MHz (%0.2f x [%d]) \n",
		TRUE_CPU_FREQ, BLCK, CPU_Multiplier + 1);
    }
  else
    {
      mvprintw (5, 0, "TURBO DISABLED on %d Cores, %s\n",
		numPhysicalCores, HT_ON_str);
      TRUE_CPU_FREQ = BLCK * ((double) CPU_Multiplier);
      mvprintw (6, 0, "True Frequency %0.2f MHz (%0.2f x [%d]) \n",
		TRUE_CPU_FREQ, BLCK, CPU_Multiplier);
    }


  if (numCPUs >= 2)
     {mvprintw (7, 0,"  Max TURBO (if Enabled) with 1/2 Core  active %dx / %dx\n", MAX_TURBO_1C, MAX_TURBO_2C);}
  if (numCPUs >= 4)
     {mvprintw (8, 0,"  Max TURBO (if Enabled) with 3/4 Cores active %dx / %dx\n", MAX_TURBO_3C, MAX_TURBO_4C);}
  if (numCPUs >= 6)
     {mvprintw (9, 0,"  Max TURBO (if Enabled) with 5/6 Cores active %dx / %dx\n", MAX_TURBO_5C, MAX_TURBO_6C);}

  mvprintw (22, 0, "C0 = Processor running without halting");
  mvprintw (23, 0,
	    "C1 = Processor running with halts (States >C0 are power saver)");
  mvprintw (24, 0,
	    "C3 = Cores running with PLL turned off and core cache turned off");
  mvprintw (25, 0,
	    "C6 = Everything in C3 + core state saved to last level cache");
  mvprintw (26, 0,
	    "  Above values in table are in percentage over the last 1 sec");
  mvprintw (27, 0,
	    " Total Logical Cores: [%d], Total Physical Cores: [%d] \n",
	    numLogicalCores, numPhysicalCores);

  mvprintw (29, 0, "  Ctrl+C to exit");


  int IA32_PERF_GLOBAL_CTRL = 911;	//38F
  int IA32_PERF_GLOBAL_CTRL_Value =
    get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
  int IA32_FIXED_CTR_CTL = 909;	//38D
  int IA32_FIXED_CTR_CTL_Value =
    get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);

  //printf("IA32_PERF_GLOBAL_CTRL %d\n",IA32_PERF_GLOBAL_CTRL_Value);
  //printf("IA32_FIXED_CTR_CTL %d\n",IA32_FIXED_CTR_CTL_Value);

  unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF,
    CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;

  CPU_CLK_UNHALTED_CORE = get_msr_value (CPU_NUM, 778, 63, 0);
  CPU_CLK_UNHALTED_REF = get_msr_value (CPU_NUM, 779, 63, 0);

  unsigned long int old_val_CORE[numCPUs], new_val_CORE[numCPUs];
  unsigned long int old_val_REF[numCPUs], new_val_REF[numCPUs];
  unsigned long int old_val_C3[numCPUs], new_val_C3[numCPUs];
  unsigned long int old_val_C6[numCPUs], new_val_C6[numCPUs];
//  unsigned long int old_val_C1[numCPUs], new_val_C1[numCPUs];

  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];

  struct timeval tvstart[numCPUs], tvstop[numCPUs];

  struct timespec one_second_sleep;
  one_second_sleep.tv_sec = 0;
  one_second_sleep.tv_nsec = 999999999;	// 1000msec


  unsigned long int IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0);
  unsigned long int IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0);
  mvprintw (11, 0, "Wait...\n");
  refresh ();
  nanosleep (&one_second_sleep, NULL);
  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0) - IA32_MPERF;
  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0) - IA32_APERF;

  //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);

  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs],
    C6_time[numCPUs];
  double _FREQ[numCPUs], _MULT[numCPUs];
  refresh ();

  mvprintw (11, 0, "Current Freqs\n");

  for (i = 0; i < numCPUs; i++)
  {
      //Set up the performance counters and then start reading from them
      CPU_NUM = i;
      IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003);

      IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);
      set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);

      IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);

      old_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0);
      old_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0);
      old_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0);
      old_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0);
      old_TSC[i] = rdtsc ();
  }

  for (;;)
    {
      nanosleep (&one_second_sleep, NULL);
      mvprintw (13, 0,
		"\tProcessor  :Actual Freq (Mult.)  C0%%   Halt(C1)%%  C3 %%   C6 %%\n");

      for (i = 0; i < numCPUs; i++)
		{
		  //read from the performance counters
		  //things like halted unhalted core cycles
		  CPU_NUM = i;
		  new_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0);
		  new_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0);
		  new_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0);
		  new_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0);
		  new_TSC[i] = rdtsc ();

		  if (old_val_CORE[i] > new_val_CORE[i])
	  	  {	  //handle overflow
			  CPU_CLK_UNHALTED_CORE = (3.40282366921e38 - old_val_CORE[i]) + new_val_CORE[i];
		  }else{
			  CPU_CLK_UNHALTED_CORE = new_val_CORE[i] - old_val_CORE[i];
		  }

		  //number of TSC cycles while its in halted state
		  if ((new_TSC[i] - old_TSC[i]) < CPU_CLK_UNHALTED_CORE)		 
				{CPU_CLK_C1 = 0;}
		  else	
				{CPU_CLK_C1 = ((new_TSC[i] - old_TSC[i]) - CPU_CLK_UNHALTED_CORE);}

		  if (old_val_REF[i] > new_val_REF[i])
	      {   //handle overflow
			  CPU_CLK_UNHALTED_REF = (3.40282366921e38 - old_val_REF[i]) + new_val_REF[i];
		  }else{
			  CPU_CLK_UNHALTED_REF = new_val_REF[i] - old_val_REF[i];
		  }

		  if (old_val_C3[i] > new_val_C3[i])
		  {   //handle overflow
			  CPU_CLK_C3 = (3.40282366921e38 - old_val_C3[i]) + new_val_C3[i];
		  }else{
			  CPU_CLK_C3 = new_val_C3[i] - old_val_C3[i];
		  }

		  if (old_val_C6[i] > new_val_C6[i])
		  {   //handle overflow
			  CPU_CLK_C6 = (3.40282366921e38 - old_val_C6[i]) + new_val_C6[i];
		  }else{
			  CPU_CLK_C6 = new_val_C6[i] - old_val_C6[i];
		  }

		  _FREQ[i] =
			estimate_MHz () * ((long double) CPU_CLK_UNHALTED_CORE /
					   (long double) CPU_CLK_UNHALTED_REF);
		  _MULT[i] = _FREQ[i] / BLCK;

		  C0_time[i] = ((long double) CPU_CLK_UNHALTED_REF /
					   (long double) (new_TSC[i] - old_TSC[i]));
		  C1_time[i] = ((long double) CPU_CLK_C1 /
				       (long double) (new_TSC[i] - old_TSC[i]));
		  C3_time[i] = ((long double) CPU_CLK_C3 /
					   (long double) (new_TSC[i] - old_TSC[i]));
		  C6_time[i] = ((long double) CPU_CLK_C6 /
				 	   (long double) (new_TSC[i] - old_TSC[i]));

		  if (C0_time[i] < 1e-2)
		  {
			if (C0_time[i] > 1e-4)	{C0_time[i] = 0.01;}
			else					{C0_time[i] = 0;}
		  }
		  if (C1_time[i] < 1e-2)
		  {
			if (C1_time[i] > 1e-4)	{C1_time[i] = 0.01;}
			else					{C1_time[i] = 0;}
		  }
		  if (C3_time[i] < 1e-2)
		  {
			if (C3_time[i] > 1e-4)	{C3_time[i] = 0.01;}
			else					{C3_time[i] = 0;}
		  }
		  if (C6_time[i] < 1e-2)
		  {
			if (C6_time[i] > 1e-4)	{C6_time[i] = 0.01;}
			else					{C6_time[i] = 0;}
		  }
	  }

      for (i = 0; i < numCPUs; i++)
		mvprintw (14 + i, 0, "\tProcessor %d:  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n", i + 1, _FREQ[i], _MULT[i], C0_time[i] * 100, C1_time[i] * 100 - (C3_time[i] * 100 + C6_time[i] * 100), C3_time[i] * 100, C6_time[i] * 100);	//C0_time[i]*100+C1_time[i]*100 around 100

	  TRUE_CPU_FREQ = 0;
	  for (i = 0; i < numCPUs; i++)
	  {
		  if (_FREQ[i] > TRUE_CPU_FREQ)
		  {
			  TRUE_CPU_FREQ = _FREQ[i];
			  mvprintw (12, 0,
				"True Frequency %0.2f MHz (Intel specifies largest of below to be running Freq)\n",
				TRUE_CPU_FREQ);
		  }
	  }

      refresh ();

      //shift the new values to the old counter values
      //so that the next time we use those to find the difference
      memcpy (old_val_CORE, new_val_CORE,
	      sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_REF, new_val_REF, sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_C3, new_val_C3, sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_C6, new_val_C6, sizeof (unsigned long int) * numCPUs);
      memcpy (tvstart, tvstop, sizeof (struct timeval) * numCPUs);
      memcpy (old_TSC, new_TSC, sizeof (unsigned long long int) * numCPUs);
  }   //ENDOF INFINITE FOR LOOP
  exit (0);
  return (1);
}
