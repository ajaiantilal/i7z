/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2009 Abhishek Jaiantilal
 *
 *   Under GPL v2
 *
 * ----------------------------------------------------------------------- */

#include "i7z.h"

int
main (int argc, char *argv[])
{
  int row, col;			/* to store the number of rows and *
				 * the number of colums of the screen */
  unsigned long long int microseconds;	/* total time taken */

  struct family_info proc_info;
  char vendor_string[13];

#ifdef x64_BIT
  get_vendor (vendor_string);
  if (strcmp (vendor_string, "GenuineIntel") == 0)
    printf ("Found Intel Processor\n");
  else
    {
      printf
	("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
      exit (1);
    }
#endif

#ifndef x64_BIT
  printf
    ("I dont know the CPUID code to check on 32-bit OS, so i will assume that you have an Intel processor\n");
  printf ("Don't worry if i don't find a nehalem next, i'll quit anyways\n");
#endif

  get_familyinformation (&proc_info);

  //printf("%x %x",proc_info.extended_model,proc_info.family);

  //check if its nehalem or exit
  //Info from page 641 of Intel Manual 3B
  if (proc_info.family == 0x6)
    {
      if (proc_info.extended_model == 0x1)
	{
	  switch (proc_info.model)
	    {
	    case 0xA:
	      printf ("Detected a nehalem (i7)\n");
	      break;
	    case 0xE:
	    case 0xF:
	      printf ("Detected a nehalem (i7/i5)\n");
	      break;
	    default:
	      printf ("Unknown processor, not exactly based on Nehalem\n");
	      exit (1);
	    }
	}
      if (proc_info.extended_model == 0x2)
	{
	  switch (proc_info.model)
	    {
	    case 0xE:
	      printf ("Detected a nehalem (Xeon)\n");
	      break;
	    case 0x5:
	    case 0xC:
	      printf ("Detected a nehalem (32nm Westmere)\n");
	      break;
	    default:
	      printf ("Unknown processor, not exactly based on Nehalem\n");
	      exit (1);
	    }
	}
    }
  else
    {
      printf ("Unknown processor, not exactly based on Nehalem\n");
      exit (1);
    }
  int width = 1;
  int i, j;

  //MSR number and hi:low bit of that MSR
  int PLATFORM_INFO_MSR = 206;	//CE 15:8
  int PLATFORM_INFO_MSR_low = 8;
  int PLATFORM_INFO_MSR_high = 15;

  int IA32_MISC_ENABLE = 416;
  int TURBO_FLAG_low = 38;
  int TURBO_FLAG_high = 38;

  int MSR_TURBO_RATIO_LIMIT = 429;

  int CPU_NUM = 0;
  int CPU_Multiplier;
  float BLCK;
  char TURBO_MODE;

  printf ("modprobbing for msr");
  system ("modprobe msr");
  sleep (1);

  // 3B defines till Max 4 Core and the rest bit values from 32:63 were reserved.  
  int MAX_TURBO_1C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 7, 0);
  int MAX_TURBO_2C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 15, 8);
  int MAX_TURBO_3C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 23, 16);
  int MAX_TURBO_4C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 31, 24);


  //CPUINFO is wrong for i7 but correct for the number of physical and logical cores present
  //If Hyperthreading is enabled then, multiple logical processors will share a common CORE ID
  //http://www.redhat.com/magazine/022aug06/departments/tips_tricks/
  system
    ("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > /tmp/cpufreq.txt");
  system
    ("grep \"core id\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numPhysical.txt");
  system
    ("grep \"processor\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numLogical.txt");


  //Open the parsed cpufreq file and obtain the cpufreq from /proc/cpuinfo
  FILE *tmp_file;
  tmp_file = fopen ("/tmp/cpufreq.txt", "r");
  char tmp_str[30];
  fgets (tmp_str, 30, tmp_file);
  double cpu_freq_cpuinfo = atof (tmp_str);
  fclose (tmp_file);

  unsigned int numPhysicalCores, numLogicalCores;
  //Parse the numPhysical and numLogical file to obtain the number of physical and logical core
  tmp_file = fopen ("/tmp/numPhysical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numPhysicalCores = atoi (tmp_str);
  fclose (tmp_file);

  tmp_file = fopen ("/tmp/numLogical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numLogicalCores = atoi (tmp_str);
  fclose (tmp_file);

  //Setup stuff for ncurses
  initscr ();			/* start the curses mode */
  start_color ();
  getmaxyx (stdscr, row, col);	/* get the number of rows and columns */
  refresh ();
  mvprintw (0, 0, "Cpu speed from cpuinfo %0.2fMhz\n", cpu_freq_cpuinfo);
  mvprintw (1, 0,
	    "cpuinfo might be wrong if cpufreq is enabled. To guess correctly try estimating via tsc\n");
  mvprintw (2, 0, "Linux's inbuilt cpu_khz code emulated now\n\n");


  //estimate the freq using the estimate_MHz() code that is almost mhz accurate
  cpu_freq_cpuinfo = estimate_MHz ();
  mvprintw (3, 0, "True Frequency (without accounting Turbo) %f\n",
	    cpu_freq_cpuinfo);

  //We just need one CPU (we use Core-0) to figure out the multiplier and the bus clock freq.
  CPU_NUM = 0;
  CPU_Multiplier =
    get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,
		   PLATFORM_INFO_MSR_low);
  BLCK = cpu_freq_cpuinfo / CPU_Multiplier;
  mvprintw (4, 0,
	    "CPU Multiplier %dx || Bus clock frequency (BCLK) %f MHz \n",
	    CPU_Multiplier, BLCK);
  TURBO_MODE = turbo_status ();	//get_msr_value(CPU_NUM,IA32_MISC_ENABLE, TURBO_FLAG_high,TURBO_FLAG_low);

  //to find how many cpus are enabled, we could have used sysconf but that will just give the logical numbers
  //if HT is enabled then the threads of the same core have the same C-state residency number so...
  //Its imperative to figure out the number of physical and number of logical cores.
  //sysconf(_SC_NPROCESSORS_ONLN);

  int numCPUs = numPhysicalCores;


  bool HT_ON;
  char HT_ON_str[30];
  if (numLogicalCores > numPhysicalCores)
    {
      strcpy (HT_ON_str, "Hyper Threading ON");
      HT_ON = true;
    }
  else
    {
      strcpy (HT_ON_str, "Hyper Threading OFF");
      HT_ON = false;
    }

  float TRUE_CPU_FREQ;
  if (TURBO_MODE == 1)
    {				// && (CPU_Multiplier+1)==MAX_TURBO_2C){
      mvprintw (5, 0, "TURBO ENABLED on %d Cores, %s\n", numPhysicalCores,
		HT_ON_str);
      TRUE_CPU_FREQ = BLCK * (CPU_Multiplier + 1);
    }
  else
    {
      mvprintw (5, 0, "TURBO DISABLED on %d Cores, %s\n", numPhysicalCores,
		HT_ON_str);
      TRUE_CPU_FREQ = BLCK * (CPU_Multiplier);
    }

  mvprintw (6, 0, "True Frequency %0.2f MHz \n", TRUE_CPU_FREQ);
  mvprintw (7, 0, "  Max TURBO (if Enabled) with 1 Core  active %dx\n",
	    MAX_TURBO_1C);
  mvprintw (8, 0, "  Max TURBO (if Enabled) with 2 Cores active %dx\n",
	    MAX_TURBO_2C);
  mvprintw (9, 0, "  Max TURBO (if Enabled) with 3 Cores active %dx\n",
	    MAX_TURBO_3C);
  mvprintw (10, 0, "  Max TURBO (if Enabled) with 4 Cores active %dx\n",
	    MAX_TURBO_4C);
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


  int IA32_PERF_GLOBAL_CTRL = 911;	//3BF
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
  unsigned long int old_val_C1[numCPUs], new_val_C1[numCPUs];

  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];

  struct timezone tz;
  struct timeval tvstart[numCPUs], tvstop[numCPUs];

  struct timespec one_second_sleep;
  one_second_sleep.tv_sec = 0;
  one_second_sleep.tv_nsec = 999999999;	// 1000msec


  unsigned long int IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0);
  unsigned long int IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0);
  mvprintw (12, 0, "Wait...\n");
  refresh ();
  nanosleep (&one_second_sleep, NULL);
  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0) - IA32_MPERF;
  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0) - IA32_APERF;

  //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);

  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs],
    C6_time[numCPUs];
  double _FREQ[numCPUs], _MULT[numCPUs];
  refresh ();

  mvprintw (12, 0, "Current Freqs\n");

  for (i = 0; i < numCPUs; i++)
    {
      CPU_NUM = i;
      IA32_PERF_GLOBAL_CTRL_Value =
	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003);
      IA32_FIXED_CTR_CTL_Value =
	get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);
      set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);
      IA32_PERF_GLOBAL_CTRL_Value =
	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      IA32_FIXED_CTR_CTL_Value =
	get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);

      old_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0);
      old_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0);
      old_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0);
      old_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0);
      old_TSC[i] = rdtsc ();
    }

  for (;;)
    {
      nanosleep (&one_second_sleep, NULL);
      mvprintw (14, 0,
		"\tProcessor  :Actual Freq (Mult.)  C0%%   Halt(C1)%%  C3 %%   C6 %%\n");

      for (i = 0; i < numCPUs; i++)
	{
	  CPU_NUM = i;
	  new_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0);
	  new_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0);
	  new_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0);
	  new_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0);
	  new_TSC[i] = rdtsc ();
	  if (old_val_CORE[i] > new_val_CORE[i])
	    {
	      CPU_CLK_UNHALTED_CORE =
		(3.40282366921e38 - old_val_CORE[i]) + new_val_CORE[i];
	    }
	  else
	    {
	      CPU_CLK_UNHALTED_CORE = new_val_CORE[i] - old_val_CORE[i];
	    }

	  //number of TSC cycles while its in halted state
	  if ((new_TSC[i] - old_TSC[i]) < CPU_CLK_UNHALTED_CORE)
	    CPU_CLK_C1 = 0;
	  else
	    CPU_CLK_C1 = ((new_TSC[i] - old_TSC[i]) - CPU_CLK_UNHALTED_CORE);

	  if (old_val_REF[i] > new_val_REF[i])
	    {
	      CPU_CLK_UNHALTED_REF =
		(3.40282366921e38 - old_val_REF[i]) + new_val_REF[i];
	    }
	  else
	    {
	      CPU_CLK_UNHALTED_REF = new_val_REF[i] - old_val_REF[i];
	    }

	  if (old_val_C3[i] > new_val_C3[i])
	    {
	      CPU_CLK_C3 = (3.40282366921e38 - old_val_C3[i]) + new_val_C3[i];
	    }
	  else
	    {
	      CPU_CLK_C3 = new_val_C3[i] - old_val_C3[i];
	    }

	  if (old_val_C6[i] > new_val_C6[i])
	    {
	      CPU_CLK_C6 = (3.40282366921e38 - old_val_C6[i]) + new_val_C6[i];
	    }
	  else
	    {
	      CPU_CLK_C6 = new_val_C6[i] - old_val_C6[i];
	    }

	  _FREQ[i] =
	    estimate_MHz () * ((long double) CPU_CLK_UNHALTED_CORE /
			       (long double) CPU_CLK_UNHALTED_REF);
	  _MULT[i] = _FREQ[i] / BLCK;

	  C0_time[i] =
	    ((long double) CPU_CLK_UNHALTED_REF /
	     (long double) (new_TSC[i] - old_TSC[i]));
	  C1_time[i] =
	    ((long double) CPU_CLK_C1 /
	     (long double) (new_TSC[i] - old_TSC[i]));
	  C3_time[i] =
	    ((long double) CPU_CLK_C3 /
	     (long double) (new_TSC[i] - old_TSC[i]));
	  C6_time[i] =
	    ((long double) CPU_CLK_C6 /
	     (long double) (new_TSC[i] - old_TSC[i]));
	  if (C0_time[i] < 1e-2)
	    if (C0_time[i] > 1e-4)
	      C0_time[i] = 0.01;
	    else
	      C0_time[i] = 0;

	  if (C1_time[i] < 1e-2)
	    if (C1_time[i] > 1e-4)
	      C1_time[i] = 0.01;
	    else
	      C1_time[i] = 0;

	  if (C3_time[i] < 1e-2)
	    if (C3_time[i] > 1e-4)
	      C3_time[i] = 0.01;
	    else
	      C3_time[i] = 0;

	  if (C6_time[i] < 1e-2)
	    if (C6_time[i] > 1e-4)
	      C6_time[i] = 0.01;
	    else
	      C6_time[i] = 0;

	}

      for (i = 0; i < numCPUs; i++)
	mvprintw (15 + i, 0, "\tProcessor %d:  %0.2f (%.2fx)\t%4.3Lg\t%4.3Lg\t%4.3Lg\t%4.3Lg\n", i + 1, _FREQ[i], _MULT[i], C0_time[i] * 100, C1_time[i] * 100 - (C3_time[i] * 100 + C6_time[i] * 100), C3_time[i] * 100, C6_time[i] * 100);	//C0_time[i]*100+C1_time[i]*100 around 100

      TRUE_CPU_FREQ = 0;
      for (i = 0; i < numCPUs; i++)
	if (_FREQ[i] > TRUE_CPU_FREQ)
	  TRUE_CPU_FREQ = _FREQ[i];
      mvprintw (13, 0,
		"True Frequency %0.2f MHz (Intel specifies largest of below to be running Freq)\n",
		TRUE_CPU_FREQ);

      refresh ();
      memcpy (old_val_CORE, new_val_CORE,
	      sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_REF, new_val_REF, sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_C3, new_val_C3, sizeof (unsigned long int) * numCPUs);
      memcpy (old_val_C6, new_val_C6, sizeof (unsigned long int) * numCPUs);
      memcpy (tvstart, tvstop, sizeof (struct timeval) * numCPUs);
      memcpy (old_TSC, new_TSC, sizeof (unsigned long long int) * numCPUs);

    }
  exit (0);
}
