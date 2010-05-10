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

extern int numPhysicalCores, numLogicalCores;
extern double TRUE_CPU_FREQ;

int Single_Socket(struct cpu_heirarchy_info *chi){
	  int row, col;	/* to store the number of rows and    *
					 * the number of colums of the screen *
					 * for NCURSES                        */

  	  printf ("i7z DEBUG: In i7z Single_Socket()\n"); 

 	  Print_Information_Processor();

	  Test_Or_Make_MSR_DEVICE_FILES();

	  sleep (1);

	  //iterator
	  int i;

	  int error_indx;
	  //cpu multiplier
	  int CPU_Multiplier;
	  //current blck value
	  float BLCK;
	  //turbo_mode enabled/disabled flag
	  char TURBO_MODE;

      int online_cpus[8];
      SET_ONLINE_ARRAY_PLUS1(online_cpus)

	  //Use Core-1 as the one to check for the turbo limit
	  //Core number shouldnt matter
	  int CPU_NUM = 0;
	  //bits from 0-63 in this store the various maximum turbo limits
	  int MSR_TURBO_RATIO_LIMIT = 429;
	  // 3B defines till Max 4 Core and the rest bit values from 32:63 were reserved.  
	  //Bits:0-7  - core1
	  int MAX_TURBO_1C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 7, 0, &error_indx);
	  //Bits:15-8 - core2
	  int MAX_TURBO_2C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 15, 8, &error_indx);
	  //Bits:23-16 - core3
	  int MAX_TURBO_3C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 23, 16, &error_indx);
	  //Bits:31-24 - core4
	  int MAX_TURBO_4C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 31, 24, &error_indx);

	  //gulftown/Hexacore support
	  //technically these should be the bits to get for core 5,6
	  //Bits:39-32 - core4
	  int MAX_TURBO_5C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 39, 32, &error_indx);
	  //Bits:47-40 - core4
	  int MAX_TURBO_6C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 47, 40, &error_indx);



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
	  mvprintw (3, 0, "True Frequency (without accounting Turbo) %0.0f MHz\n",
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
	  CPU_Multiplier = get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high, PLATFORM_INFO_MSR_low, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);

	  //Blck is basically the true speed divided by the multiplier
	  BLCK = cpu_freq_cpuinfo / CPU_Multiplier;
	  mvprintw (4, 0,
			"CPU Multiplier %dx || Bus clock frequency (BCLK) %0.2f MHz \n",
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
	  int IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);
	  int IA32_FIXED_CTR_CTL = 909;	//38D
	  int IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);

	  //printf("IA32_PERF_GLOBAL_CTRL %d\n",IA32_PERF_GLOBAL_CTRL_Value);
	  //printf("IA32_FIXED_CTR_CTL %d\n",IA32_FIXED_CTR_CTL_Value);

	  unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF,
		CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;

	  CPU_CLK_UNHALTED_CORE = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);
	  CPU_CLK_UNHALTED_REF = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);

	  unsigned long long int old_val_CORE[numCPUs], new_val_CORE[numCPUs];
	  unsigned long long int old_val_REF[numCPUs], new_val_REF[numCPUs];
	  unsigned long long int old_val_C3[numCPUs], new_val_C3[numCPUs];
	  unsigned long long int old_val_C6[numCPUs], new_val_C6[numCPUs];
	//  unsigned long int old_val_C1[numCPUs], new_val_C1[numCPUs];

	  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];

	  struct timeval tvstart[numCPUs], tvstop[numCPUs];

	  struct timespec one_second_sleep;
	  one_second_sleep.tv_sec = 0;
	  one_second_sleep.tv_nsec = 999999999;	// 1000msec


	  unsigned long int IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);
	  unsigned long int IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0, &error_indx);
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);
	  mvprintw (11, 0, "Wait...\n");
	  refresh ();
	  nanosleep (&one_second_sleep, NULL);
	  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0, &error_indx) - IA32_MPERF;
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);
	  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0, &error_indx) - IA32_APERF;
	  SET_IF_TRUE(error_indx,online_cpus[0],-1);

	  //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);

	  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs],
		C6_time[numCPUs];
	  double _FREQ[numCPUs], _MULT[numCPUs];
	  refresh ();

	  mvprintw (11, 0, "Current Freqs\n");

	  int kk=11, ii;
	  
	  for (;;)
		{
		      SET_ONLINE_ARRAY_PLUS1(online_cpus)
			  if (kk > 10){
				kk=0;
				for (ii = 0; ii < numCPUs; ii++)
				{
				  //Set up the performance counters and then start reading from them
				  CPU_NUM = ii;
				  IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003LLU);

				  IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);

				  IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);
				
                  IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  old_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);
				  
				  old_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  old_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  old_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[ii],-1);
				  CONTINUE_IF_TRUE(online_cpus[ii]==-1);

				  old_TSC[ii] = rdtsc ();
				}
			  }
			  kk++;
		  nanosleep (&one_second_sleep, NULL);
		  mvprintw (13, 0,
			"\tProcessor  :Actual Freq (Mult.)  C0%%   Halt(C1)%%  C3 %%   C6 %%\n");

		  for (i = 0; i < numCPUs; i++)
			{
			  //read from the performance counters
			  //things like halted unhalted core cycles
			  
			  CPU_NUM = i;
			  new_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);
			
              new_TSC[i] = rdtsc ();

			  if (old_val_CORE[i] > new_val_CORE[i])
		  	  {	  //handle overflow
				  CPU_CLK_UNHALTED_CORE = (UINT64_MAX - old_val_CORE[i]) + new_val_CORE[i];
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
				  CPU_CLK_UNHALTED_REF = (UINT64_MAX - old_val_REF[i]) + new_val_REF[i]; //3.40282366921e38
			  }else{
				  CPU_CLK_UNHALTED_REF = new_val_REF[i] - old_val_REF[i];
			  }

			  if (old_val_C3[i] > new_val_C3[i])
			  {   //handle overflow
				  CPU_CLK_C3 = (UINT64_MAX - old_val_C3[i]) + new_val_C3[i];
			  }else{
				  CPU_CLK_C3 = new_val_C3[i] - old_val_C3[i];
			  }

			  if (old_val_C6[i] > new_val_C6[i])
			  {   //handle overflow
				  CPU_CLK_C6 = (UINT64_MAX - old_val_C6[i]) + new_val_C6[i];
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
			  //printf("%lld  - %lld", CPU_CLK_C6, (new_TSC[i] - old_TSC[i]));
		  }

		  for (i = 0; i < numCPUs; i++){
			if(online_cpus[i]==-1){
				mvprintw (14 + i, 0, "\tProcessor %d:  OFFLINE\n",i + 1);
			}else{
				mvprintw (14 + i, 0, "\tProcessor %d:  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n", i + 1, _FREQ[i], _MULT[i],
				      C0_time[i] * 100 , C1_time[i]* 100  - (C3_time[i] + C6_time[i] )* 100, C3_time[i]* 100 , C6_time[i]* 100);	//C0_time[i]*100+C1_time[i]*100 around 100
			}
		  }


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
