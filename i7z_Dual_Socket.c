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
#include <pthread.h>
#include "i7z.h"

extern int numPhysicalCores, numLogicalCores;
extern double TRUE_CPU_FREQ;


struct print_socket_arg_data
{
  int threadid;
  int socketid;
  struct cpu_heirarchy_info chi;
};

void *print_Socket (void *threadarg);
void print_Common_Stuff ();
void from_cpu_heirarchy_info_get_information_about_socket_cores (struct	 cpu_heirarchy_info		 *chi,
				 int socket_num, int *core_array, int *core_arraysize_phy, int *core_arraysize_log);

int Dual_Socket (struct cpu_heirarchy_info *chi)
{
	  int row, col;			/* to store the number of rows and    *
					 * the number of colums of the screen *
					 * for NCURSES                        */

	  printf ("i7z DEBUG: In i7z Dual_Socket()\n");

	  Print_Information_Processor ();

	  Test_Or_Make_MSR_DEVICE_FILES ();

	  sleep (1);

	  //Setup stuff for ncurses
	  initscr ();			/* start the curses mode */
	  start_color ();
	  getmaxyx (stdscr, row, col);	/* get the number of rows and columns */
	  refresh ();
	  //Setup for ncurses completed

	  pthread_t threads[1];
	  int t;
	  int rc;

	  struct print_socket_arg_data thread_data[2];

	  for (t = 0; t < 2; t++)
	  {
		  thread_data[t].threadid = t;
		  thread_data[t].socketid = t;
		  memcpy ((void *) &thread_data[t].chi, (void *) chi, sizeof (struct cpu_heirarchy_info));
		  rc = pthread_create (&threads[t], NULL, print_Socket, (void *) &thread_data[t]);
   	      if (rc)
		  {
		    printf ("ERROR; return code from pthread_create() is %d\n", rc);
		    exit (-1);
		  }
	  }
	  pthread_exit (NULL);
	  exit (0);
	  return (1);
}


void *print_Socket (void *threadarg)
{
	  struct print_socket_arg_data *arg_data;
	  arg_data = (struct print_socket_arg_data *) threadarg;

	  int socket_num = arg_data->socketid;
	  int thread_num = arg_data->threadid;
	  struct cpu_heirarchy_info chi;
	  memcpy ((void *) &chi, &arg_data->chi, sizeof (struct cpu_heirarchy_info));

	  int printw_offset = (thread_num) * 14;

	  //Make an array size max 8 (to accomdate Nehalem-EXEX -lol) to store the core-num that are candidates for a given socket
	  int core_list[8], core_list_size_phy, core_list_size_log;
	  construct_cpu_hierarchy (&chi);
	  from_cpu_heirarchy_info_get_information_about_socket_cores (&chi, socket_num, core_list,
									  &core_list_size_phy, &core_list_size_log);
	  //printf("socket_num %d  core_list_size %d, %d,  core-0 %d\n",socket_num,core_list_size_phy,core_list_size_log, core_list[0]);          


	  numPhysicalCores = core_list_size_phy;
	  numLogicalCores = core_list_size_log;
	  //reading of the number of cores is done


	  //iterator
	  int i;

	  //cpu multiplier
	  int CPU_Multiplier;
	  //current blck value
	  float BLCK;

	  //Use Core-1 as the one to check for the turbo limit
	  //Core number shouldnt matter
	  int CPU_NUM;

	  //turbo_mode enabled/disabled flag
	  char TURBO_MODE;

	  double cpu_freq_cpuinfo;

	  cpu_freq_cpuinfo = cpufreq_info ();
	  //estimate the freq using the estimate_MHz() code that is almost mhz accurate
	  cpu_freq_cpuinfo = estimate_MHz ();

	  if (thread_num == 0)
	  {
		  //Print a slew of information on the ncurses window
		  mvprintw (0, 0, "Cpu speed from cpuinfo %0.2fMhz\n", cpu_freq_cpuinfo);
		  mvprintw (1, 0,
			"True Frequency (without accounting Turbo) %0.0f MHz\n",
			cpu_freq_cpuinfo);
	  }
	  //MSR number and hi:low bit of that MSR
	  //This msr contains a lot of stuff, per socket wise
	  //one can pass any core number and then get in multiplier etc 
	  int PLATFORM_INFO_MSR = 206;	//CE 15:8
	  int PLATFORM_INFO_MSR_low = 8;
	  int PLATFORM_INFO_MSR_high = 15;


	  unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF,
		CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;


	  int numCPUs_max = 8;
	  unsigned long long int old_val_CORE[numCPUs_max], new_val_CORE[numCPUs_max];
	  unsigned long long int old_val_REF[numCPUs_max], new_val_REF[numCPUs_max];
	  unsigned long long int old_val_C3[numCPUs_max], new_val_C3[numCPUs_max];
	  unsigned long long int old_val_C6[numCPUs_max], new_val_C6[numCPUs_max];

	  unsigned long long int old_TSC[numCPUs_max], new_TSC[numCPUs_max];
	  long double C0_time[numCPUs_max], C1_time[numCPUs_max],
		C3_time[numCPUs_max], C6_time[numCPUs_max];
	  double _FREQ[numCPUs_max], _MULT[numCPUs_max];
	  struct timeval tvstart[numCPUs_max], tvstop[numCPUs_max];

	  struct timespec one_second_sleep;
	  one_second_sleep.tv_sec = 0;
	  one_second_sleep.tv_nsec = 999999999;	// 1000msec


	  unsigned long int IA32_MPERF;
	  unsigned long int IA32_APERF;

	  //Get turbo mode status by reading msr within turbo_status
	  TURBO_MODE = turbo_status ();

	  //Flags and other things about HT.
	  int HT_ON;
	  char HT_ON_str[30];

	  //HT enabled if num logical > num physical cores
	  if (numLogicalCores > numPhysicalCores)
	  {
		  strncpy (HT_ON_str, "Hyper Threading ON\0", 30);
		  HT_ON = 1;
	  }
	  else
	  {
		  strncpy (HT_ON_str, "Hyper Threading OFF\0", 30);
		  HT_ON = 0;
	  }

	  int kk = 11, ii;

      int online_cpus[8];
      int error_indx;

	  for (;;)
	  {
   		  SET_ONLINE_ARRAY_PLUS1(online_cpus)

		  //We just need one CPU (we use Core-1) to figure out the multiplier and the bus clock freq.
		  //multiplier doesnt automatically include turbo
		  //note turbo is not guaranteed, only promised
		  //So this msr will only reflect the actual multiplier, rest has to be figured out
		  CPU_NUM = core_list[0];

		  if (CPU_NUM == -1)
		  {
		    sleep (1);		//sleep for a bit hoping that the offline socket becomes online
		    continue;
		  }
		  //number of CPUs is as told via cpuinfo 
		  int numCPUs = numPhysicalCores;

		  CPU_Multiplier = get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high, PLATFORM_INFO_MSR_low, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);

		  //Blck is basically the true speed divided by the multiplier
		  BLCK = cpu_freq_cpuinfo / CPU_Multiplier;

		  //Use Core-1 as the one to check for the turbo limit
		  //Core number shouldnt matter

		  //bits from 0-63 in this store the various maximum turbo limits
		  int MSR_TURBO_RATIO_LIMIT = 429;
		  // 3B defines till Max 4 Core and the rest bit values from 32:63 were reserved.  
		  //Bits:0-7  - core1
		  int MAX_TURBO_1C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 7, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  //Bits:15-8 - core2
		  int MAX_TURBO_2C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 15, 8, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  //Bits:23-16 - core3
		  int MAX_TURBO_3C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 23, 16, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  //Bits:31-24 - core4
		  int MAX_TURBO_4C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 31, 24, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);

		  //gulftown/Hexacore support
		  //technically these should be the bits to get for core 5,6
		  //Bits:39-32 - core4
		  int MAX_TURBO_5C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 39, 32, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  //Bits:47-40 - core4
		  int MAX_TURBO_6C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 47, 40, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);

		  fflush (stdout);
		  sleep (1);

		  char string_ptr1[200], string_ptr2[200];

		  int IA32_PERF_GLOBAL_CTRL = 911;	//38F
		  int IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);
		  
          int IA32_FIXED_CTR_CTL = 909;	//38D
		  int IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);


		  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  CPU_CLK_UNHALTED_CORE = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  CPU_CLK_UNHALTED_REF = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  refresh ();
		  nanosleep (&one_second_sleep, NULL);
		  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0, &error_indx) - IA32_MPERF;
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0, &error_indx) - IA32_APERF;
		  SET_IF_TRUE(error_indx,online_cpus[0],-1);
		  CONTINUE_IF_TRUE(online_cpus[0]==-1);

		  refresh ();
		  mvprintw (4 + printw_offset, 0,
			"  CPU Multiplier %dx || Bus clock frequency (BCLK) %0.2f MHz \n",
			CPU_Multiplier, BLCK);

		  if (numCPUs >= 2 && numCPUs < 4)
		  {
		    sprintf (string_ptr1, "  Max TURBO (if Enabled) with 1/2");
		    sprintf (string_ptr2, " %dx / %dx ", MAX_TURBO_1C, MAX_TURBO_2C);
		  }
		  if (numCPUs >= 2 && numCPUs < 6)
		  {
		    sprintf (string_ptr1, "  Max TURBO (if Enabled) with 1/2/3/4");
		    sprintf (string_ptr2, " %dx / %dx / %dx / %dx ", MAX_TURBO_1C, MAX_TURBO_2C, MAX_TURBO_3C, MAX_TURBO_4C);
		  }
		  if (numCPUs >= 2 && numCPUs >= 6)	// Gulftown 6-cores
		  {
		    sprintf (string_ptr1, "  Max TURBO (if Enabled) with 1/2/3/4/5/6");
		    sprintf (string_ptr2, " %dx / %dx / %dx / %dx / %dx / %dx", MAX_TURBO_1C, MAX_TURBO_2C, MAX_TURBO_3C, MAX_TURBO_4C,
						   MAX_TURBO_5C, MAX_TURBO_6C);
		  }


		  if (thread_num == 0)
		  {
		    mvprintw (31, 0, "C0 = Processor running without halting");
		    mvprintw (32, 0,
				"C1 = Processor running with halts (States >C0 are power saver)");
		    mvprintw (33, 0,
				"C3 = Cores running with PLL turned off and core cache turned off");
		    mvprintw (34, 0,
				"C6 = Everything in C3 + core state saved to last level cache");
		    mvprintw (35, 0,
				"  Above values in table are in percentage over the last 1 sec");
 		    mvprintw (36, 0, "  Ctrl+C to exit");
		  }

		  from_cpu_heirarchy_info_get_information_about_socket_cores (&chi,
					  socket_num, core_list, &core_list_size_phy, &core_list_size_log);

		  numCPUs = core_list_size_phy;
		  numPhysicalCores = core_list_size_phy;
		  numLogicalCores = core_list_size_log;
		  mvprintw (3 + printw_offset, 0,
			"Socket [%d] - [physical cores=%d, logical cores=%d] \n",
			socket_num, numPhysicalCores, numLogicalCores);

		  mvprintw (7 + printw_offset, 0, "%s %s\n", string_ptr1, string_ptr2);

		  if (TURBO_MODE == 1)
		  {
		    mvprintw (5 + printw_offset, 0, "  TURBO ENABLED on %d Cores, %s\n", numPhysicalCores, HT_ON_str);
		    TRUE_CPU_FREQ = BLCK * ((double) CPU_Multiplier + 1);
		    mvprintw (6 + printw_offset, 0, "  True Frequency %0.2f MHz (%0.2f x [%d]) \n", TRUE_CPU_FREQ, BLCK, CPU_Multiplier + 1);
		  }
		  else
		  {
		    mvprintw (5 + printw_offset, 0, "  TURBO DISABLED on %d Cores, %s\n", numPhysicalCores, HT_ON_str);
 		    TRUE_CPU_FREQ = BLCK * ((double) CPU_Multiplier);
		    mvprintw (6 + printw_offset, 0,	"  True Frequency %0.2f MHz (%0.2f x [%d]) \n", TRUE_CPU_FREQ, BLCK, CPU_Multiplier);
		  }

		  if (kk > 10)
		  {
			  kk = 0;
			  for (i = 0; i < numCPUs; i++)
				{
				  //Set up the performance counters and then start reading from them
				  CPU_NUM = core_list[i];
				  ii = core_list[i];
				  IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003LLU);

				  IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);

				  IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  old_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  old_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  old_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  old_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
				  SET_IF_TRUE(error_indx,online_cpus[i],-1);
				  CONTINUE_IF_TRUE(online_cpus[i]==-1);

				  old_TSC[ii] = rdtsc ();
				}
		  }
		  kk++;
		  nanosleep (&one_second_sleep, NULL);
		  mvprintw (9 + printw_offset, 0, "\tProcessor [core-id]  :Actual Freq (Mult.)\t  C0%%   Halt(C1)%%  C3 %%   C6 %%\n");

		  for (i = 0; i < numCPUs; i++)
		  {
			  //read from the performance counters
			  //things like halted unhalted core cycles

			  CPU_NUM = core_list[i];
			  ii = core_list[i];
			  new_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

  		      new_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
			  SET_IF_TRUE(error_indx,online_cpus[i],-1);
			  CONTINUE_IF_TRUE(online_cpus[i]==-1);

			  new_TSC[ii] = rdtsc ();

			  if (old_val_CORE[ii] > new_val_CORE[ii])
			  {			//handle overflow
				  CPU_CLK_UNHALTED_CORE = (UINT64_MAX - old_val_CORE[ii]) + new_val_CORE[ii];
			  }else{
				  CPU_CLK_UNHALTED_CORE = new_val_CORE[ii] - old_val_CORE[ii];
			  }

			  //number of TSC cycles while its in halted state
			  if ((new_TSC[ii] - old_TSC[ii]) < CPU_CLK_UNHALTED_CORE)
		      {
				  CPU_CLK_C1 = 0;
			  }else{
				  CPU_CLK_C1 = ((new_TSC[ii] - old_TSC[ii]) - CPU_CLK_UNHALTED_CORE);
			  }

			  if (old_val_REF[ii] > new_val_REF[ii])
			  {			//handle overflow
				  CPU_CLK_UNHALTED_REF = (UINT64_MAX - old_val_REF[ii]) + new_val_REF[ii];	//3.40282366921e38
			  }else{
				  CPU_CLK_UNHALTED_REF = new_val_REF[ii] - old_val_REF[ii];
			  }

			  if (old_val_C3[ii] > new_val_C3[ii])
			  {			//handle overflow
				  CPU_CLK_C3 = (UINT64_MAX - old_val_C3[ii]) + new_val_C3[ii];
			  }else{
				  CPU_CLK_C3 = new_val_C3[ii] - old_val_C3[ii];
			  }

			  if (old_val_C6[ii] > new_val_C6[ii])
			  {			//handle overflow
				  CPU_CLK_C6 = (UINT64_MAX - old_val_C6[ii]) + new_val_C6[ii];
			  }else{
				  CPU_CLK_C6 = new_val_C6[ii] - old_val_C6[ii];
			  }

			  _FREQ[ii] =
				estimate_MHz () * ((long double) CPU_CLK_UNHALTED_CORE /
						   (long double) CPU_CLK_UNHALTED_REF);
			  _MULT[ii] = _FREQ[ii] / BLCK;

			  C0_time[ii] = ((long double) CPU_CLK_UNHALTED_REF /
					 (long double) (new_TSC[ii] - old_TSC[ii]));
			  C1_time[ii] = ((long double) CPU_CLK_C1 /
					 (long double) (new_TSC[ii] - old_TSC[ii]));
			  C3_time[ii] = ((long double) CPU_CLK_C3 /
					 (long double) (new_TSC[ii] - old_TSC[ii]));
			  C6_time[ii] = ((long double) CPU_CLK_C6 /
					 (long double) (new_TSC[ii] - old_TSC[ii]));

			  if (C0_time[ii] < 1e-2)
			  {
				if (C0_time[ii] > 1e-4)
				{
				  C0_time[ii] = 0.01;
				}else{
				  C0_time[ii] = 0;
				}
			  }
			  if (C1_time[ii] < 1e-2)
			  {
				if (C1_time[ii] > 1e-4)
				{
				  C1_time[ii] = 0.01;
				}else{
				  C1_time[ii] = 0;
				}
			  }
			  if (C3_time[ii] < 1e-2)
			  {
				if (C3_time[ii] > 1e-4)
				{
				  C3_time[ii] = 0.01;
				}else{
				  C3_time[ii] = 0;
				}
			  }
			  if (C6_time[ii] < 1e-2)
			  {
				if (C6_time[ii] > 1e-4)
				{
				  C6_time[ii] = 0.01;
				}else{
				  C6_time[ii] = 0;
				}
			  }
			}

		  for (ii = 0; ii < numCPUs; ii++)
		  {
		    i = core_list[ii];
			if(online_cpus[ii]==-1){
				mvprintw (10 + ii + printw_offset, 0, "\tProcessor %d:  OFFLINE\n",i + 1);
			}else{
		    	mvprintw (10 + ii + printw_offset, 0, "\tProcessor %d [%d]:\t  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n", i + 1, core_list[ii], _FREQ[i], _MULT[i], C0_time[i] * 100, C1_time[i] * 100 - (C3_time[i] + C6_time[i]) * 100, C3_time[i] * 100, C6_time[i] * 100);	//C0_time[i]*100+C1_time[i]*100 around 100
			}
		  }


		  TRUE_CPU_FREQ = 0;
		  for (ii = 0; ii < numCPUs; ii++)
		  {
		    i = core_list[ii];
		    if (_FREQ[i] > TRUE_CPU_FREQ)
			{
			  TRUE_CPU_FREQ = _FREQ[i];
			}
		  }
		  mvprintw (8 + printw_offset, 0,
			"  True Frequency %0.2f MHz \n", socket_num,
			numPhysicalCores, numLogicalCores, TRUE_CPU_FREQ);

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
		}				//ENDOF INFINITE FOR LOOP

}
