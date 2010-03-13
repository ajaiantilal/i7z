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


#include <QApplication>
#include <QPushButton>
#include <QProgressBar>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QThread>
#include <QTimer>
#include <QTime>

#ifndef UINT32_MAX
# define UINT32_MAX (4294967295U)
#endif
#include "../helper_functions.c"


bool global_in_i7z_main_thread = false;

class MyThread:public QThread
{
public:
  unsigned int numCPUs;
    MyThread ();
  void run ();
  double *FREQ, *MULT;
  long double *C0_TIME, *C1_TIME, *C3_TIME, *C6_TIME;
};

MyThread::MyThread ()
{
  FILE *tmp_file;
  char tmp_str[30];
  unsigned int numPhysicalCores;

  //figure out the number of physical cores from cpuinfo
  system ("grep \"core id\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numPhysical.txt");

  //Open the parsed cpufreq file and obtain the cpufreq from /proc/cpuinfo
  //Parse the numPhysical and numLogical file to obtain the number of physical cores
  tmp_file = fopen ("/tmp/numPhysical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numPhysicalCores = atoi (tmp_str);
  fclose (tmp_file);

  numCPUs = numPhysicalCores;

  //allocate space for the variables
  FREQ = (double *) malloc (sizeof (double) * numCPUs);
  MULT = (double *) malloc (sizeof (double) * numCPUs);
  C0_TIME = (long double *) malloc (sizeof (long double) * numCPUs);
  C1_TIME = (long double *) malloc (sizeof (long double) * numCPUs);
  C3_TIME = (long double *) malloc (sizeof (long double) * numCPUs);
  C6_TIME = (long double *) malloc (sizeof (long double) * numCPUs);

  int i;
  for (i = 0; i < (int)numCPUs; i++)
    {
      FREQ[i] = 0;
      MULT[i] = 0;
      C0_TIME[i] = 0;
      C1_TIME[i] = 0;
      C3_TIME[i] = 0;
      C6_TIME[i] = 0;
    }
}


void
MyThread::run ()
{
  int i;

  //MSR number and hi:low bit of that MSR
  //This msr contains a lot of stuff, per socket wise
  //one can pass any core number and then get in multiplier etc 
  int PLATFORM_INFO_MSR = 206;	//CE 15:8
  int PLATFORM_INFO_MSR_low = 8;
  int PLATFORM_INFO_MSR_high = 15;

  ////To find out if Turbo is enabled use the below msr and bit 38
  ////bit for TURBO is 38
  ////msr reading is now moved into tubo_status
  //int IA32_MISC_ENABLE = 416;
  //int TURBO_FLAG_low = 38;
  //int TURBO_FLAG_high = 38;


  //int MSR_TURBO_RATIO_LIMIT = 429;

  int CPU_NUM;
  int CPU_Multiplier;
  float BLCK;
  char TURBO_MODE;


  struct family_info proc_info;


  get_familyinformation (&proc_info);

  //printf("%x %x",proc_info.extended_model,proc_info.family);

  //check if its nehalem or exit
  //Info from page 641 of Intel Manual 3B
  //Extended model and Model can help determine the right cpu
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
	      exit (1);
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
			  exit (1);
    	  }
	}else{
      printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
      exit (1);
	}
  }else{
      printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
      exit (1);
  }


  printf("i7z DEBUG: msr = Model Specific Register\n");
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
		  exit (1);
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
		  exit (1);
      }
  }
  sleep (1);

  // 3B defines till Max 4 Core and the rest bit values from 32:63 were reserved.  
  // int MAX_TURBO_1C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 7, 0);
  // int MAX_TURBO_2C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 15, 8);
  // int MAX_TURBO_3C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 23, 16);
  // int MAX_TURBO_4C = get_msr_value (CPU_NUM, MSR_TURBO_RATIO_LIMIT, 31, 24);


  //CPUINFO is wrong for i7 but correct for the number of physical and logical cores present
  //If Hyperthreading is enabled then, multiple logical processors will share a common CORE ID
  //http://www.redhat.com/magazine/022aug06/departments/tips_tricks/
  system ("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > /tmp/cpufreq.txt");
  system ("grep \"core id\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numPhysical.txt");
  system ("grep \"processor\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numLogical.txt");


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


  //estimate the freq using the estimate_MHz() code that is almost mhz accurate
  cpu_freq_cpuinfo = estimate_MHz ();

  //We just need one CPU (we use Core-0) to figure out the multiplier and the bus clock freq.
  CPU_NUM = 0;
  CPU_Multiplier =
    get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,
		   PLATFORM_INFO_MSR_low);
  BLCK = cpu_freq_cpuinfo / CPU_Multiplier;
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
    {
      TRUE_CPU_FREQ = BLCK * ((double)CPU_Multiplier + 1);
    }
  else
    {
      TRUE_CPU_FREQ = BLCK * ((double)CPU_Multiplier);
    }


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
//  unsigned long int old_val_C1[numCPUs], new_val_C1[numCPUs];

  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];

  struct timeval tvstart[numCPUs], tvstop[numCPUs];

  struct timespec one_second_sleep;
  one_second_sleep.tv_sec = 0;
  one_second_sleep.tv_nsec = 999999999;	// 1000msec


  unsigned long int IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0);
  unsigned long int IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0);
//   mvprintw(12,0,"Wait...\n"); refresh();
  nanosleep (&one_second_sleep, NULL);
  IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0) - IA32_MPERF;
  IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0) - IA32_APERF;

  //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);

  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs],
    C6_time[numCPUs];
  double _FREQ[numCPUs], _MULT[numCPUs];

//  mvprintw(12,0,"Current Freqs\n");

  for (i = 0; i < numCPUs; i++)
  {
      CPU_NUM = i;
      IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003LLU);

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

  int kk=0, ii;
  for (;;)
  {
  
  if(kk>10){
    kk=0;
    for (ii = 0; ii < numCPUs; ii++)
    {
      CPU_NUM = ii;
//      IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
      set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003LLU);

//      IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);
      set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);

//      IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0);
//      IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0);

      old_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0);
      old_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0);
      old_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0);
      old_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0);
      old_TSC[i] = rdtsc ();
    }
  }    
  kk=0;

    nanosleep (&one_second_sleep, NULL);

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

	  C1_time[i] -= C3_time[i] + C6_time[i];

	  if (C0_time[i] < 1e-2){
	    if (C0_time[i] > 1e-4) 	C0_time[i] = 0.01;
	    else				    C0_time[i] = 0;
	  }

	  if (C1_time[i] < 1e-2){
	    if (C1_time[i] > 1e-4)  C1_time[i] = 0.01;
	    else				    C1_time[i] = 0;
	  }

	  if (C3_time[i] < 1e-2){
	    if (C3_time[i] > 1e-4)  C3_time[i] = 0.01;
	    else			        C3_time[i] = 0;
	  }

	  if (C6_time[i] < 1e-2){
	    if (C6_time[i] > 1e-4)  C6_time[i] = 0.01;
	    else			        C6_time[i] = 0;
	  }
	}

    TRUE_CPU_FREQ = 0;
    for (i = 0; i < numCPUs; i++)
		if (_FREQ[i] > TRUE_CPU_FREQ)
		  TRUE_CPU_FREQ = _FREQ[i];

    memcpy (old_val_CORE, new_val_CORE, sizeof (unsigned long int) * numCPUs);
    memcpy (old_val_REF, new_val_REF, sizeof (unsigned long int) * numCPUs);
    memcpy (old_val_C3, new_val_C3, sizeof (unsigned long int) * numCPUs);
    memcpy (old_val_C6, new_val_C6, sizeof (unsigned long int) * numCPUs);
    memcpy (tvstart, tvstop, sizeof (struct timeval) * numCPUs);
    memcpy (old_TSC, new_TSC, sizeof (unsigned long long int) * numCPUs);

    memcpy (FREQ, _FREQ, sizeof (double) * numCPUs);
    memcpy (MULT, _MULT, sizeof (double) * numCPUs);
    memcpy (C0_TIME, C0_time, sizeof (long double) * numCPUs);
    memcpy (C1_TIME, C1_time, sizeof (long double) * numCPUs);
    memcpy (C3_TIME, C3_time, sizeof (long double) * numCPUs);
    memcpy (C6_TIME, C6_time, sizeof (long double) * numCPUs);
    global_in_i7z_main_thread = true;
  }

}

class MyWidget:public QWidget
{
Q_OBJECT public:
  QProgressBar * C0_l[6], *C1_l[6], *C3_l[6], *C6_l[6];
  QLabel *C0, *C1, *C3, *C6;
  QLabel *Freq_[6];
  QLabel *StatusMessage, *Curr_Freq;
  QLabel *ProcNames[6];
    MyWidget (QWidget * parent = 0);
  MyThread *mythread;
  unsigned int numCPUs;

  private slots:void UpdateWidget ();
};

MyWidget::MyWidget (QWidget * parent):QWidget (parent)
{

  FILE * tmp_file;
  char tmp_str[30];

  //figure out the number of physical cores from cpuinfo
  system ("grep \"core id\" /proc/cpuinfo |sort -|uniq -|wc -l > /tmp/numPhysical.txt");

  //Open the parsed cpufreq file and obtain the cpufreq from /proc/cpuinfo
  unsigned int numPhysicalCores;
  //Parse the numPhysical and numLogical file to obtain the number of physical cores
  tmp_file = fopen ("/tmp/numPhysical.txt", "r");
  fgets (tmp_str, 30, tmp_file);
  numPhysicalCores = atoi (tmp_str);
  fclose (tmp_file);

  numCPUs = numPhysicalCores;

  int  i;

  for (i = 0; i < (int)numCPUs; i++)
  {
      C0_l[i] = new QProgressBar;
      C0_l[i]->setMaximum (99);
      C0_l[i]->setMinimum (0);
      C1_l[i] = new QProgressBar;
      C1_l[i]->setMaximum (99);
      C1_l[i]->setMinimum (0);
      C3_l[i] = new QProgressBar;
      C3_l[i]->setMaximum (99);
      C3_l[i]->setMinimum (0);
      C6_l[i] = new QProgressBar;
      C6_l[i]->setMaximum (99);
      C6_l[i]->setMinimum (0);
      Freq_[i] = new QLabel (tr (""));
  }

  QGridLayout * layout1 = new QGridLayout;


  for (i = 0; i < (int)numCPUs; i++)
  {
      layout1->addWidget (C0_l[i], i + 1, 1);
      layout1->addWidget (C1_l[i], i + 1, 2);
      layout1->addWidget (C3_l[i], i + 1, 3);
      layout1->addWidget (C6_l[i], i + 1, 4);
  }

  C0 = new QLabel (tr ("C0"));
  C0->setAlignment (Qt::AlignCenter);
  C1 = new QLabel (tr ("C1"));
  C1->setAlignment (Qt::AlignCenter);
  C3 = new QLabel (tr ("C3"));
  C3->setAlignment (Qt::AlignCenter);
  C6 = new QLabel (tr ("C6"));
  C6->setAlignment (Qt::AlignCenter);

  ProcNames[0] = new QLabel (tr ("Processor-1"));
  ProcNames[1] = new QLabel (tr ("Processor-2"));
  ProcNames[2] = new QLabel (tr ("Processor-3"));
  ProcNames[3] = new QLabel (tr ("Processor-4"));
  ProcNames[4] = new QLabel (tr ("Processor-5"));
  ProcNames[5] = new QLabel (tr ("Processor-6"));

  StatusMessage = new QLabel (tr ("Wait"));
  Curr_Freq = new QLabel (tr ("Wait"));

  for (i = 0; i < (int)numCPUs; i++)
      layout1->addWidget (ProcNames[i], i + 1, 0);

  layout1->addWidget (C0, 0, 1);
  layout1->addWidget (C1, 0, 2);
  layout1->addWidget (C3, 0, 3);
  layout1->addWidget (C6, 0, 4);

  for (i = 0; i < (int)numCPUs; i++)
      layout1->addWidget (Freq_[i], i + 1, 5);

  layout1->addWidget (StatusMessage, numCPUs + 1, 4);
  layout1->addWidget (Curr_Freq, numCPUs + 1, 5);

  QTimer *timer = new QTimer (this);
  connect (timer, SIGNAL (timeout ()), this, SLOT (UpdateWidget ()));
  timer->start (1000);

  mythread = new MyThread ();
  mythread->start ();

  setLayout (layout1);
}

void
MyWidget::UpdateWidget ()
{
  int i;
  char val2set[100];
  for (i = 0; i < (int)numCPUs; i++)
  {
      snprintf (val2set, 100, "%0.2f Ghz", mythread->FREQ[i]);
      Freq_[i]->setText (val2set);
  }

  for (i = 0; i < (int)numCPUs; i++)
  {
      C0_l[i]->setValue (mythread->C0_TIME[i] * 100);
      C1_l[i]->setValue (mythread->C1_TIME[i] * 100);
      C3_l[i]->setValue (mythread->C3_TIME[i] * 100);
      C6_l[i]->setValue (mythread->C6_TIME[i] * 100);
  }

  float Max_Freq = 0;
  for (i = 0; i < (int)numCPUs; i++)
  {
	if (mythread->FREQ[i] > Max_Freq)
		Max_Freq = mythread->FREQ[i];
  }
  StatusMessage->setText ("Current Freq:");
  snprintf (val2set, 100, "%0.2f Ghz", Max_Freq);
  Curr_Freq->setText (val2set);
}



int
main (int argc, char *argv[])
{
  QApplication app (argc, argv);

  MyWidget i7z_widget;
  i7z_widget.show ();
  return app.exec ();
}

#include "GUI_i7z.moc"
