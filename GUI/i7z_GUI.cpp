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
#include <math.h>

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

#define MAX_PROCESSORS_GUI 12

bool global_in_i7z_main_thread = false;
int socket_list[MAX_PROCESSORS_GUI];
int core_list[MAX_PROCESSORS_GUI];
struct cpu_heirarchy_info chi;
struct cpu_socket_info socket_0, socket_1;
unsigned int numCPUs;
struct program_options prog_options;

void Construct_Socket_Information_in_GUI(unsigned int *numCPUs) {
    int socket_0_num=0, socket_1_num=1;
    socket_0.max_cpu=0;
    socket_0.socket_num=0;
    int i;
    for(i=0;i < 8; i++)
        socket_0.processor_num[i]=-1;
    socket_1.max_cpu=0;
    socket_1.socket_num=1;
    
    for(i=0;i < 8; i++)
        socket_1.processor_num[i]=-1;

    construct_CPU_Heirarchy_info(&chi);
    construct_sibling_list(&chi);
//    print_CPU_Heirarchy(chi);
    construct_socket_information(&chi, &socket_0, &socket_1, socket_0_num, socket_1_num);
//    print_socket_information(&socket_0);
//    print_socket_information(&socket_1);
    *numCPUs = socket_0.num_physical_cores + socket_1.num_physical_cores;

    //// FOR DEBUGGING DUAL SOCKET CODE ON SINGLE SOCKET, UNCOMMENT BELOW 2 lines
//    memcpy(&socket_1, &socket_0, sizeof(struct cpu_socket_info));
//    socket_1.socket_num=0;


//    print_socket_information(&socket_0);
//    print_socket_information(&socket_1);
    *numCPUs = socket_0.num_physical_cores + socket_1.num_physical_cores;
//    printf("My Widget: Num Processors %d\n",*numCPUs);

    int k, ii;
    k=0;
    for (ii = 0; ii < socket_0.num_physical_cores ; ii++) {
        if ( socket_0.processor_num[ii] != -1) {
            core_list[k] = socket_0.processor_num[ii];
            socket_list[k] = 0;
            k++;
        }
    }
    for (ii = 0; ii < socket_1.num_physical_cores ; ii++) {
        if ( socket_1.processor_num[ii] != -1) {
            core_list[k] = socket_1.processor_num[ii];
            socket_list[k] = 1;
            k++;
        }
    }
}

class MyThread:public QThread
{
public:
    MyThread ();
    void run ();
    double *FREQ, *MULT;
    long double *C0_TIME, *C1_TIME, *C3_TIME, *C6_TIME;
};

MyThread::MyThread ()
{
    Construct_Socket_Information_in_GUI(&numCPUs);

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

	print_CPU_Heirarchy(chi);

    int i, ii;

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

    printf("i7z DEBUG: GUI VERSION DOESN'T SUPPORT CORE OFFLINING\n");
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
    numPhysicalCores = socket_0.num_physical_cores + socket_1.num_physical_cores;
    numLogicalCores  = socket_0.num_logical_cores + socket_1.num_logical_cores;
//    printf("My thread: Num Processors %d\n",numCPUs);

    int error_indx;

    //estimate the freq using the estimate_MHz() code that is almost mhz accurate
    cpu_freq_cpuinfo = estimate_MHz ();

    //We just need one CPU (we use Core-0) to figure out the multiplier and the bus clock freq.
    CPU_NUM = 0;
    CPU_Multiplier =
        get_msr_value (CPU_NUM, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,
                       PLATFORM_INFO_MSR_low, &error_indx);
    BLCK = cpu_freq_cpuinfo / CPU_Multiplier;
    TURBO_MODE = turbo_status ();	//get_msr_value(CPU_NUM,IA32_MISC_ENABLE, TURBO_FLAG_high,TURBO_FLAG_low);

    //to find how many cpus are enabled, we could have used sysconf but that will just give the logical numbers
    //if HT is enabled then the threads of the same core have the same C-state residency number so...
    //Its imperative to figure out the number of physical and number of logical cores.
    //sysconf(_SC_NPROCESSORS_ONLN);


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
        get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0,&error_indx);
    int IA32_FIXED_CTR_CTL = 909;	//38D
    int IA32_FIXED_CTR_CTL_Value =
        get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0,&error_indx);

    //printf("IA32_PERF_GLOBAL_CTRL %d\n",IA32_PERF_GLOBAL_CTRL_Value);
    //printf("IA32_FIXED_CTR_CTL %d\n",IA32_FIXED_CTR_CTL_Value);

    unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF,
    CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;

    CPU_CLK_UNHALTED_CORE = get_msr_value (CPU_NUM, 778, 63, 0,&error_indx);
    CPU_CLK_UNHALTED_REF = get_msr_value (CPU_NUM, 779, 63, 0,&error_indx);

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


    unsigned long int IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0,&error_indx);
    unsigned long int IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0,&error_indx);
//   mvprintw(12,0,"Wait...\n"); refresh();
    nanosleep (&one_second_sleep, NULL);
    IA32_MPERF = get_msr_value (CPU_NUM, 231, 7, 0, &error_indx) - IA32_MPERF;
    IA32_APERF = get_msr_value (CPU_NUM, 232, 7, 0, &error_indx) - IA32_APERF;

    //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);

    long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs],    C6_time[numCPUs];
    double _FREQ[numCPUs], _MULT[numCPUs];

//  mvprintw(12,0,"Current Freqs\n");

    int kk=11;
    double estimated_mhz;
    for (;;)
    {
        Construct_Socket_Information_in_GUI(&numCPUs);

        if (kk>10) {
            kk=0;
            for (ii = 0; ii < (int)numCPUs; ii++)
            {
                CPU_NUM = core_list[ii];
                IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
                set_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 0x700000003LLU);

                IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);
                set_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 819);

                IA32_PERF_GLOBAL_CTRL_Value =	get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);
                IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);

                old_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0,&error_indx);
                old_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0,&error_indx);
                old_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0,&error_indx);
                old_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0,&error_indx);
                old_TSC[ii] = rdtsc ();
            }
        }
        kk++;

        nanosleep (&one_second_sleep, NULL);

        estimated_mhz = estimate_MHz();

        for (i = 0; i < (int)numCPUs; i++)
        {
            CPU_NUM = core_list[i];
            new_val_CORE[i] = get_msr_value (CPU_NUM, 778, 63, 0,&error_indx);
            new_val_REF[i] = get_msr_value (CPU_NUM, 779, 63, 0,&error_indx);
            new_val_C3[i] = get_msr_value (CPU_NUM, 1020, 63, 0,&error_indx);
            new_val_C6[i] = get_msr_value (CPU_NUM, 1021, 63, 0,&error_indx);
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
                estimated_mhz * ((long double) CPU_CLK_UNHALTED_CORE /
                                 (long double) CPU_CLK_UNHALTED_REF);
            _MULT[i] = _FREQ[i] / BLCK;

            C0_time[i] =
                ((long double) CPU_CLK_UNHALTED_REF /
                 (long double) (new_TSC[i] - old_TSC[i]));
            long double c1_time =
                ((long double) CPU_CLK_C1 /
                 (long double) (new_TSC[i] - old_TSC[i]));
            C3_time[i] =
                ((long double) CPU_CLK_C3 /
                 (long double) (new_TSC[i] - old_TSC[i]));
            C6_time[i] =
                ((long double) CPU_CLK_C6 /
                 (long double) (new_TSC[i] - old_TSC[i]));

            //C1_time[i] -= C3_time[i] + C6_time[i];
			C1_time[i] = c1_time - (C3_time[i] + C6_time[i]) ;
            if (!isnan(c1_time) && !isinf(c1_time)) {
                if (C1_time[i] <= 0) {
                    C1_time[i]=0;
                }
            }

            if (C0_time[i] < 1e-2) {
                if (C0_time[i] > 1e-4) 	C0_time[i] = 0.01;
                else				    C0_time[i] = 0;
            }

            if (C1_time[i] < 1e-2) {
                if (C1_time[i] > 1e-4)  C1_time[i] = 0.01;
                else				    C1_time[i] = 0;
            }

            if (C3_time[i] < 1e-2) {
                if (C3_time[i] > 1e-4)  C3_time[i] = 0.01;
                else			        C3_time[i] = 0;
            }

            if (C6_time[i] < 1e-2) {
                if (C6_time[i] > 1e-4)  C6_time[i] = 0.01;
                else			        C6_time[i] = 0;
            }
        }
//   printf("Hello");
//   for(i=0;i<numCPUs;i++){
//      printf("%g %Lg %Lg %Lg %Lg %lld %llu\n",_FREQ[i],C0_time[i]*100,C1_time[i]*100,C3_time[i]*100,C6_time[i]*100,CPU_CLK_UNHALTED_REF,(new_TSC[i] - old_TSC[i]));
//      printf("%g %llu %llu %llu %llu %llu\n",_FREQ[i],CPU_CLK_UNHALTED_REF,CPU_CLK_C1,CPU_CLK_C3,CPU_CLK_C6,(new_TSC[i] - old_TSC[i]));
//      printf("%llu %llu  %lld\n",new_TSC[i], old_TSC[i],new_TSC[i]- old_TSC[i]);
//   }
        TRUE_CPU_FREQ = 0;
        for (i = 0; i < (int)numCPUs; i++)
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
    QProgressBar * C0_l[MAX_PROCESSORS_GUI], *C1_l[MAX_PROCESSORS_GUI], *C3_l[MAX_PROCESSORS_GUI], *C6_l[MAX_PROCESSORS_GUI];
    QLabel *C0, *C1, *C3, *C6;
    QLabel *Freq_[MAX_PROCESSORS_GUI];
    QLabel *StatusMessage0, *StatusMessage1, *Curr_Freq0, *Curr_Freq1;
    QLabel *ProcNames[MAX_PROCESSORS_GUI];
    MyWidget (QWidget * parent = 0);
    MyThread *mythread;
    int curr_numCPUs;

private slots:
    void UpdateWidget ();
};

MyWidget::MyWidget (QWidget * parent):QWidget (parent)
{

    Print_Version_Information();

    //
    //Print_Information_Processor ();
    bool cpuNehalem, cpuSandybridge, cpuIvybridge, cpuHaswell;
    Print_Information_Processor (&cpuNehalem, &cpuSandybridge, &cpuIvybridge, &cpuHaswell);

    Test_Or_Make_MSR_DEVICE_FILES ();

    Construct_Socket_Information_in_GUI(&numCPUs);

    char processor_str[100];
//    printf("MyWidget: Num Processors %d\n",numCPUs);
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

    curr_numCPUs = numCPUs;
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

    snprintf(processor_str, 100, "Core-1 [id:%d,socket:%d]",core_list[0],socket_list[0]);
    ProcNames[0] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-2 [id:%d,socket:%d]",core_list[1],socket_list[1]);
    ProcNames[1] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-3 [id:%d,socket:%d]",core_list[2],socket_list[2]);
    ProcNames[2] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-4 [id:%d,socket:%d]",core_list[3],socket_list[3]);
    ProcNames[3] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-5 [id:%d,socket:%d]",core_list[4],socket_list[4]);
    ProcNames[4] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-6 [id:%d,socket:%d]",core_list[5],socket_list[5]);
    ProcNames[5] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-7 [id:%d,socket:%d]",core_list[6],socket_list[6]);
    ProcNames[6] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-8 [id:%d,socket:%d]",core_list[7],socket_list[7]);
    ProcNames[7] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-9 [id:%d,socket:%d]",core_list[8],socket_list[8]);
    ProcNames[8] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-10 [id:%d,socket:%d]",core_list[9],socket_list[9]);
    ProcNames[9] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-11 [id:%d,socket:%d]",core_list[10],socket_list[10]);
    ProcNames[10] = new QLabel (tr (processor_str));

    snprintf(processor_str, 100, "Core-12 [id:%d,socket:%d]",core_list[11],socket_list[11]);
    ProcNames[11] = new QLabel (tr (processor_str));

    StatusMessage0 = new QLabel (tr ("Wait"));
    Curr_Freq0 = new QLabel (tr ("Wait"));

    if ( (socket_0.num_physical_cores > 0) && (socket_1.num_physical_cores > 0)) {
        StatusMessage1 = new QLabel (tr ("Wait"));
        Curr_Freq1 = new QLabel (tr ("Wait"));
    }

    for (i = 0; i < (int)numCPUs; i++) {
        layout1->addWidget (ProcNames[i], i + 1, 0);
    }

    layout1->addWidget (C0, 0, 1);
    layout1->addWidget (C1, 0, 2);
    layout1->addWidget (C3, 0, 3);
    layout1->addWidget (C6, 0, 4);

    for (i = 0; i < (int)numCPUs; i++)
        layout1->addWidget (Freq_[i], i + 1, 5);

    layout1->addWidget (StatusMessage0, numCPUs + 1, 4);
    layout1->addWidget (Curr_Freq0, numCPUs + 1, 5);
    if ( (socket_0.num_physical_cores > 0) && (socket_1.num_physical_cores > 0)) {
        layout1->addWidget (StatusMessage1, numCPUs + 2, 4);
        layout1->addWidget (Curr_Freq1, numCPUs + 2, 5);
    }
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
    char processor_str[100];
    snprintf(processor_str, 100, "Core-1 [id:%d,socket:%d]",core_list[0],socket_list[0]);
    ProcNames[0]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-2 [id:%d,socket:%d]",core_list[1],socket_list[1]);
    ProcNames[1]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-3 [id:%d,socket:%d]",core_list[2],socket_list[2]);
    ProcNames[2]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-4 [id:%d,socket:%d]",core_list[3],socket_list[3]);
    ProcNames[3]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-5 [id:%d,socket:%d]",core_list[4],socket_list[4]);
    ProcNames[4]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-6 [id:%d,socket:%d]",core_list[5],socket_list[5]);
    ProcNames[5]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-7 [id:%d,socket:%d]",core_list[6],socket_list[6]);
    ProcNames[6]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-8 [id:%d,socket:%d]",core_list[7],socket_list[7]);
    ProcNames[7]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-9 [id:%d,socket:%d]",core_list[8],socket_list[8]);
    ProcNames[8]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-10 [id:%d,socket:%d]",core_list[9],socket_list[9]);
    ProcNames[9]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-11 [id:%d,socket:%d]",core_list[10],socket_list[10]);
    ProcNames[10]->setText(tr (processor_str));
    snprintf(processor_str, 100, "Core-12 [id:%d,socket:%d]",core_list[11],socket_list[11]);
    ProcNames[11]->setText(tr (processor_str));

//Have to make sure that the constructor is being called correct.
//the below code logic is that if a core goes offline, call the constructor
//so as to replot all the widgets.
    /*    printf("Number of Cores %d\n",numCPUs);
    	if(numCPUs != curr_numCPUs){
    		curr_numCPUs = numCPUs;
    		printf("Number of Cores changed\n");
    		MyWidget (0);
    	}*/

//    printf("UpdateWidget: Num Processors %d\n",numCPUs);
    int i;
    char val2set[100];
    for (i = 0; i < (int)numCPUs; i++)
    {
        snprintf (val2set, 100, "%0.2f Mhz", mythread->FREQ[i]);
        Freq_[i]->setText (val2set);
    }

    for (i = 0; i < (int)numCPUs; i++)
    {
        C0_l[i]->setValue (mythread->C0_TIME[i] * 100);
        C1_l[i]->setValue (mythread->C1_TIME[i] * 100);
        C3_l[i]->setValue (mythread->C3_TIME[i] * 100);
        C6_l[i]->setValue (mythread->C6_TIME[i] * 100);
    }

    float Max_Freq_socket0 = 0;
    float Max_Freq_socket1 = 0;
    int num_socket0_cpus, num_socket1_cpus;

    for (i = 0; i < (int)numCPUs; i++)
    {
        if ( (mythread->FREQ[i] > Max_Freq_socket0) && (!isnan(mythread->FREQ[i])) &&
                (!isinf(mythread->FREQ[i]))  && (socket_list[i] == socket_0.socket_num) ) {
            Max_Freq_socket0 = mythread->FREQ[i];
            num_socket0_cpus++;
        }
        if ( (mythread->FREQ[i] > Max_Freq_socket1) && (!isnan(mythread->FREQ[i])) &&
                (!isinf(mythread->FREQ[i]))  && (socket_list[i] == socket_1.socket_num) ) {
            Max_Freq_socket1 = mythread->FREQ[i];
            num_socket1_cpus++;
        }
    }
    if (socket_0.num_physical_cores > 0) {
        StatusMessage0->setText ("Socket[0] Freq:");
        snprintf (val2set, 100, "%0.2f Mhz", Max_Freq_socket0);
        Curr_Freq0->setText (val2set);
    }
    if (socket_1.num_physical_cores > 0) {
        StatusMessage1->setText ("Socket[1] Freq:");
        snprintf (val2set, 100, "%0.2f Mhz", Max_Freq_socket1);
        Curr_Freq1->setText (val2set);
    }
}



int
main (int argc, char *argv[])
{
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
    
	QApplication app (argc, argv);
	
	char str_display[1050];
	snprintf(str_display, 1050, "i7z @ %s", hostname);
        MyWidget i7z_widget;
	i7z_widget.setWindowTitle(str_display);
	i7z_widget.show ();
	return app.exec ();
}

#include "i7z_GUI.moc"
