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
#include <math.h>
#include <ctype.h>
#include <pthread.h>

#include "perfmon-i7z.h"
//#include "CPUHeirarchy.h"

#define CPU_FREQUENCY 3291  //this is in mhz

int Single_Socket();
int Dual_Socket();

int numPhysicalCores, numLogicalCores;
double TRUE_CPU_FREQ;

#define IA32_THERM_STATUS 0x19C
#define IA32_TEMPERATURE_TARGET 0x1a2

//Info: I start from index 1 when i talk about cores on CPU

//Monitoring Version - 1
//4 events at most can be monitored
//put in the value in PERFEVTSEL and the counts are in PMC
#define IA32_PERFEVTSEL0 0x186
#define IA32_PERFEVTSEL1 0x187
#define IA32_PERFEVTSEL2 0x188
#define IA32_PERFEVTSEL3 0x189
#define IA32_PMC0 0x0C1
#define IA32_PMC1 0x0C2
#define IA32_PMC2 0x0C3
#define IA32_PMC3 0x0C4

//Monitoring Version - 2
//Another 3 events can be then monitored  in IA32_FIXED_CTR0 to CTR2
#define IA32_FIXED_CTR_CTL 0x38D
#define IA32_PERF_GLOBAL_CTRL 0x38F
#define IA32_FIXED_CTR0 0x309	//Instruction_Retired.Any
#define IA32_FIXED_CTR1 0x30A	//CPU_CLK_Unhalted.Core
#define IA32_FIXED_CTR2 0x30B	//CPU_CLK_Unhalted.Ref

//Performance event number and umask value
#define MEM_INST_RETIRED_LOADS_EVENT 0x0B
#define MEM_INST_RETIRED_LOADS_UMASK 0x01
#define MEM_INST_RETIRED_LOADS 0x010B

#define MEM_INST_RETIRED_STORES_EVENT 0x0B
#define MEM_INST_RETIRED_STORES_UMASK 0x02
#define MEM_INST_RETIRED_STORES 0x020B

#define FP_COMP_OPS_EXE_X87_EVENT 0x10
#define FP_COMP_OPS_EXE_X87_UMASK 0x01
#define FP_COMP_OPS_EXE_X87 0x0110

#define FP_COMP_OPS_EXE_SSE_FP_EVENT 0x10
#define FP_COMP_OPS_EXE_SSE_FP_UMASK 0x04
#define FP_COMP_OPS_EXE_SSE_FP 0x0410

#define MAX_PROCESSORS_SYSTEM 4

int error_indx;

struct therm_status_struct {
	int reading_valid;
	int resolution_deg_celcius;
	int digital_readout;
	int power_limit_notification_status;
	int power_limit_notification_log;
	int thermal_threshold_2_log;
	int thermal_threshold_2_status;
	int thermal_threshold_1_log;
	int thermal_threshold_1_status;
	int critical_temperature_log;
	int critical_temperature_status;
	int PROCHOT_log;
	int PROCHOT_event;
	int thermal_status_log;
	int thermal_status;

	int PROCHOT_temp;	//Tj
	int current_core_temp;	//PROCHOT_temp - digital_readout
};

int Get_Bits_Value(unsigned long val, int highbit, int lowbit)
{
	unsigned long data = val;
	int bits = highbit - lowbit + 1;
	if (bits < 64) {
		data >>= lowbit;
		data &= (1ULL << bits) - 1;
	}
	return (data);
}

void Print_Thermal_Status(struct therm_status_struct therm_status)
{
	printf("Reading Valid: %d\n", therm_status.reading_valid);
	printf("Deg Celcius: %d\n", therm_status.resolution_deg_celcius);
	printf("Digital Readout: %d\n", therm_status.digital_readout);
	printf("Tj: %d\n", therm_status.PROCHOT_temp);
	printf("Current Temp: %d\n", therm_status.current_core_temp);
}

void Read_Thermal_Status(struct therm_status_struct *therm_status)
{
	unsigned long val =
	    get_msr_value(0, IA32_THERM_STATUS, 63, 0, &error_indx);
	therm_status->reading_valid = Get_Bits_Value(val, 32, 31);
	therm_status->resolution_deg_celcius = Get_Bits_Value(val, 31, 27);
	therm_status->digital_readout = Get_Bits_Value(val, 23, 16);

	val = get_msr_value(0, IA32_TEMPERATURE_TARGET, 63, 0, &error_indx);
	therm_status->PROCHOT_temp = Get_Bits_Value(val, 24, 16);

	therm_status->current_core_temp =
	    therm_status->PROCHOT_temp - therm_status->digital_readout;
}

unsigned long Read_instruction_retired_any(int cpu)
{
	return (get_msr_value(cpu, IA32_FIXED_CTR0, 63, 0, &error_indx));
}

unsigned long Read_cpu_clk_unhalted_core(int cpu)
{
	return (get_msr_value(cpu, IA32_FIXED_CTR1, 63, 0, &error_indx));
}

unsigned long Read_cpu_clk_unhalted_ref(int cpu)
{
	return (get_msr_value(cpu, IA32_FIXED_CTR2, 63, 0, &error_indx));
}

//////////////PERFORMANCE COUNTER VERSION-1/////////////////////////////////////////////////////
void Set_MV1_Counters(int cpu, int pmc1, int pmc2, int pmc3, int pmc4)
{
	int error_indx;
	//pmc consists of 1byte of umask and 1byte of event select
	//63 = 0110 0011 = enable counter,anythread,os mode and user mode
	set_msr_value(cpu, IA32_PERFEVTSEL0, 0x430000 + pmc1, &error_indx);
	set_msr_value(cpu, IA32_PERFEVTSEL1, 0x430000 + pmc2, &error_indx);
	set_msr_value(cpu, IA32_PERFEVTSEL2, 0x430000 + pmc3, &error_indx);
	set_msr_value(cpu, IA32_PERFEVTSEL3, 0x430000 + pmc4, &error_indx);
}

unsigned long Get_MV1_Counter0(int cpu)
{
	return (get_msr_value(cpu, IA32_PMC0, 63, 0, &error_indx));
}

unsigned long Get_MV1_Counter1(int cpu)
{
	return (get_msr_value(cpu, IA32_PMC1, 63, 0, &error_indx));
}

unsigned long Get_MV1_Counter2(int cpu)
{
	return (get_msr_value(cpu, IA32_PMC2, 63, 0, &error_indx));
}

unsigned long Get_MV1_Counter3(int cpu)
{
	return (get_msr_value(cpu, IA32_PMC3, 63, 0, &error_indx));
}

void sleep_ms(long long int msecs)
{
	struct timespec t_req, t_rem;
	int secs_to_sleep = floor((double)msecs / 1000);
	int ms_to_sleep = msecs - secs_to_sleep * 1000;
	t_req.tv_sec = secs_to_sleep;
	t_req.tv_nsec = ms_to_sleep * 1000000;
	nanosleep(&t_req, NULL);
}

long double ctime_long_double()
{
	struct timeval current_time;
	long double current_time_f;
	gettimeofday(&current_time, NULL);
	current_time_f =
	    (long double)current_time.tv_sec +
	    (long double)current_time.tv_usec / 1000000;
	return (current_time_f);
}

///////////////////////////////////////////////////////////////////////////////////////////////

struct cycle_information {
	unsigned long instruction_retired_any;
	unsigned long cpu_clk_unhalted_core;
	unsigned long cpu_clk_unhalted_ref;
	unsigned long mem_inst_retired_stores;
	unsigned long mem_inst_retired_loads;
	unsigned long fp_comp_ops_exe_sse_fp;
	unsigned long fp_comp_ops_exe_x87;
};

char *lvalue = NULL;
bool logging = false;


void *main_thread(void *filename)
{
        int i;
	struct cpu_heirarchy_info chi;
	struct cpu_socket_info socket_0;
	socket_0.max_cpu = 0, socket_0.socket_num = 0;
	for(i=0; i<8; i++)
	    socket_0.processor_num[i] = -1;
	    
	struct cpu_socket_info socket_1;
	socket_1.max_cpu = 0, socket_1.socket_num = 0;
	for(i=0; i<8; i++)
	    socket_1.processor_num[i] = -1;


	int c;
	opterr = 0;

	
	FILE *fp;
	printf("Logging enabled to %s\n", lvalue);

	construct_CPU_Heirarchy_info(&chi);
	construct_sibling_list(&chi);
	construct_socket_information(&chi, &socket_0, &socket_1);
	//print_CPU_Heirarchy(chi);
	//print_socket_information(&socket_0);


    /******************************************
    for(i=0; i<8 ;i++){
	printf("%d %d\n",chi.coreid_num[i],chi.sibling_num[i]);
    }

    struct therm_status_struct therm_status;
    printf("Number of MSR %d\n", num_msr());
    printf("PMCx width %d\n", bit_width_PMCx());

    printf("Sizeof long %d\n", sizeof(long));
    printf("value of long %ld\n", 0x7fffffffffffffff);
    ******************************************/

	unsigned long new_value_array[MAX_PROCESSORS_SYSTEM * 4],
	    old_value_array[MAX_PROCESSORS_SYSTEM * 4];

	unsigned long instruction_retired_any_old[MAX_PROCESSORS_SYSTEM],
	    instruction_retired_any_new[MAX_PROCESSORS_SYSTEM];
	unsigned long cpu_clk_unhalted_core_old[MAX_PROCESSORS_SYSTEM],
	    cpu_clk_unhalted_core_new[MAX_PROCESSORS_SYSTEM];
	unsigned long cpu_clk_unhalted_ref_old[MAX_PROCESSORS_SYSTEM],
	    cpu_clk_unhalted_ref_new[MAX_PROCESSORS_SYSTEM];
	unsigned long new_tsc[MAX_PROCESSORS_SYSTEM],
	    old_tsc[MAX_PROCESSORS_SYSTEM];

	memset(new_value_array, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(old_value_array, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(instruction_retired_any_old, 0,
	       MAX_PROCESSORS_SYSTEM * sizeof(unsigned long));
	memset(instruction_retired_any_new, 0,
	       MAX_PROCESSORS_SYSTEM * sizeof(unsigned long));
	memset(cpu_clk_unhalted_core_old, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(cpu_clk_unhalted_core_new, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(cpu_clk_unhalted_ref_old, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(cpu_clk_unhalted_ref_new, 0,
	       MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(new_tsc, 0, MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));
	memset(old_tsc, 0, MAX_PROCESSORS_SYSTEM * 4 * sizeof(unsigned long));

	unsigned long long tsc_overall, instruction_retired_any_overall,
	    cpu_clk_unhalted_core_overall, cpu_clk_unhalted_ref_overall,
	    value_array_overall[4];

/*	new_value = get_msr_value(0,0x186,63,0,&error_indx);
	printf("value %ld\n",new_value);
	new_value = get_msr_value(0,0x187,63,0,&error_indx);
	printf("value %ld\n",new_value);
	new_value = get_msr_value(0,0x188,63,0,&error_indx);
	printf("value %ld\n",new_value);*/

	int error_indx;
	for (i = 0; i < MAX_PROCESSORS_SYSTEM; i++) {
		//below is example of MV-1 for instruction_retired.any
		//set_msr_value(0,IA32_PERFEVTSEL0,0x63003c);
		Set_MV1_Counters(i, FP_COMP_OPS_EXE_SSE_FP, FP_COMP_OPS_EXE_X87,
				 MEM_INST_RETIRED_LOADS,
				 MEM_INST_RETIRED_STORES);
		//Needs to set this to enable Monitoring Version - 2
		set_msr_value(i, IA32_FIXED_CTR_CTL, 0xFFF, &error_indx);	//can be used to stop/start counter
		set_msr_value(i, IA32_PERF_GLOBAL_CTRL, 0x70000000FLLU, &error_indx);	//this controls both the MV-2 and MV-1 events
		//Thus need to enable it always!
	}

	bool init_thread = 0;

	printf
	    ("Core id\t\tCurr Time\tTSC diff \tCurrFREQ (Core/Ref)*CPU_Freq\tInst.Retired\tUnhalted(Core)\tUnhalted(Ref)\tMem.Loads\tMem.Stores\t\tSSE_FP\t\tX87\n");

	long long int iteration = 0;
	int num_online_physical_cores = 0;
	long double time_1, time_2, elapsed_time=0;
	long double time_3, time_4, time_in_sleep=0;
	//struct timeval current_time;
	//long double current_time_f;

	while (1) {
		time_1 = ctime_long_double();
		if (logging) {
			//printf("logging noow");
			fp = fopen(lvalue, "a+");
			//printf("fp=%d", fp);
		}
		//gettimeofday(&current_time, NULL);
		//current_time_f = (long double)current_time.tv_sec + (long double)current_time.tv_usec/1000000;
		//printf("sec %Lf\n", ctime_long_double());

		tsc_overall = 0, instruction_retired_any_overall = 0, 
		cpu_clk_unhalted_core_overall = 0, cpu_clk_unhalted_ref_overall = 0;
		memset(value_array_overall, 0, sizeof(unsigned long) * 4);
		num_online_physical_cores = 0;

		for (i = 0; i < MAX_PROCESSORS_SYSTEM; i++) {
			construct_CPU_Heirarchy_info(&chi);
			construct_sibling_list(&chi);
			construct_socket_information(&chi, &socket_0, &socket_1);

			old_value_array[i * 4 + 0] = new_value_array[i * 4 + 0];
			old_value_array[i * 4 + 1] = new_value_array[i * 4 + 1];
			old_value_array[i * 4 + 2] = new_value_array[i * 4 + 2];
			old_value_array[i * 4 + 3] = new_value_array[i * 4 + 3];

			old_tsc[i] = new_tsc[i];
			instruction_retired_any_old[i] = instruction_retired_any_new[i];
			cpu_clk_unhalted_core_old[i]   = cpu_clk_unhalted_core_new[i];
			cpu_clk_unhalted_ref_old[i]    = cpu_clk_unhalted_ref_new[i];

			new_tsc[i] = rdtsc();
			//new_value = get_msr_value(0,IA32_PMC0,63,0,&error_indx);
			new_value_array[i * 4 + 0] = Get_MV1_Counter0(i);
			new_value_array[i * 4 + 1] = Get_MV1_Counter1(i);
			new_value_array[i * 4 + 2] = Get_MV1_Counter2(i);
			new_value_array[i * 4 + 3] = Get_MV1_Counter3(i);

			instruction_retired_any_new[i] = Read_instruction_retired_any(i);
			cpu_clk_unhalted_core_new[i]   = Read_cpu_clk_unhalted_core(i);
			cpu_clk_unhalted_ref_new[i]    = Read_cpu_clk_unhalted_ref(i);

			if ((iteration > 0)
			    && (socket_0.processor_num[i] != -1)) {
				num_online_physical_cores++;
				if (iteration % 10 == 0) {printf("%10d\t%12.2Lf\t", socket_0.processor_num[i], ctime_long_double());}
				if (logging) {fprintf(fp, "%10d\t%12.2Lf\t", socket_0.processor_num[i], ctime_long_double());}
				
				if (iteration % 10 == 0)
					printf("%10llu\t%10f\t\t", (new_tsc[i] - old_tsc[i]),
					       CPU_FREQUENCY * ((double) (cpu_clk_unhalted_core_new[i] - cpu_clk_unhalted_core_old[i])) / ((double) (cpu_clk_unhalted_ref_new[i] - cpu_clk_unhalted_ref_old[i])));
				if (logging) {
					fprintf(fp, "%10llu\t%10f\t\t", (new_tsc[i] - old_tsc[i]), 
						   CPU_FREQUENCY * ((double) (cpu_clk_unhalted_core_new[i] - cpu_clk_unhalted_core_old[i])) / ((double)(cpu_clk_unhalted_ref_new[i] - cpu_clk_unhalted_ref_old[i])));
				}
				tsc_overall += (new_tsc[i] - old_tsc[i]);

				if (iteration % 10 == 0) {printf("%10lu\t", instruction_retired_any_new[i] - instruction_retired_any_old[i]);}
				if (logging) {fprintf(fp, "%10lu\t", instruction_retired_any_new[i] - instruction_retired_any_old[i]);}
				instruction_retired_any_overall +=    (instruction_retired_any_new[i] - instruction_retired_any_old[i]);

				if (iteration % 10 == 0) {printf("%10lu\t", cpu_clk_unhalted_core_new[i] - cpu_clk_unhalted_core_old[i]);}
				if (logging) {fprintf(fp, "%10lu\t", cpu_clk_unhalted_core_new[i] -	cpu_clk_unhalted_core_old[i]);}
				cpu_clk_unhalted_core_overall += (cpu_clk_unhalted_core_new[i] - cpu_clk_unhalted_core_old[i]);

				if (iteration % 10 == 0) {printf("%10lu\t", cpu_clk_unhalted_ref_new[i] - cpu_clk_unhalted_ref_old[i]);}
				if (logging) {fprintf(fp, "%10lu\t", cpu_clk_unhalted_ref_new[i] - cpu_clk_unhalted_ref_old[i]);}
				cpu_clk_unhalted_ref_overall += (cpu_clk_unhalted_ref_new[i] - cpu_clk_unhalted_ref_old[i]);

				if (iteration % 10 == 0) {printf("%10lu\t", new_value_array[i * 4 + 0] - old_value_array[i * 4 + 0]);}
				if (logging) {fprintf(fp, "%10lu\t", new_value_array[i * 4 + 0] - old_value_array[i * 4 + 0]);}
				value_array_overall[0] += (new_value_array[i * 4 + 0] - old_value_array[i * 4 + 0]);

				if (iteration % 10 == 0) {printf("%10lu\t", new_value_array[i * 4 + 1] - old_value_array[i * 4 + 1]);}
				if (logging) {fprintf(fp, "%10lu\t", new_value_array[i * 4 + 1] - old_value_array[i * 4 + 1]);}
				value_array_overall[1] += (new_value_array[i * 4 + 1] - old_value_array[i * 4 + 1]);

				if (iteration % 10 == 0) {printf("%10lu\t", new_value_array[i * 4 + 2] - old_value_array[i * 4 + 2]);}
				if (logging) {fprintf(fp, "%10lu\t", new_value_array[i * 4 + 2] - old_value_array[i * 4 + 2]);}
				value_array_overall[2] += (new_value_array[i * 4 + 2] - old_value_array[i * 4 + 2]);

				if (iteration % 10 == 0) {printf("%10lu\n", new_value_array[i * 4 + 3] - old_value_array[i * 4 + 3]);}
				if (logging) {fprintf(fp, "%10lu\n", new_value_array[i * 4 + 3] - old_value_array[i * 4 + 3]);}
				value_array_overall[3] += (new_value_array[i * 4 + 3] - old_value_array[i * 4 + 3]);
			}
			//Read_Thermal_Status(&therm_status);
			//Print_Thermal_Status(therm_status);
		}

		if ((iteration > 0) && (iteration % 10 == 0)) {
			printf("Overall (%d/%d)\t%12.2Lf\t",
			       num_online_physical_cores, 4,
			       ctime_long_double());
			printf("%10llu\t%10f\t\t", tsc_overall,
			       CPU_FREQUENCY * ((double)cpu_clk_unhalted_core_overall /
				      (double)cpu_clk_unhalted_ref_overall));
			printf("%10llu\t", instruction_retired_any_overall);
			printf("%10llu\t", cpu_clk_unhalted_core_overall);
			printf("%10llu\t", cpu_clk_unhalted_ref_overall);
			printf("%10llu\t", value_array_overall[0]);
			printf("%10llu\t", value_array_overall[1]);
			printf("%10llu\t", value_array_overall[2]);
			printf("%10llu\t", value_array_overall[3]);
			printf("\n");

			printf
			    ("--------------------------------------------------------------------------------------------");
			printf
			    ("--------------------------------------------------------------------------------------------\n");
		}
		if ((logging) && (iteration > 0)) {
			fprintf(fp, "Overall (%d/%d)\t%12.2Lf\t",
				num_online_physical_cores, 4,
				ctime_long_double());
			fprintf(fp, "%10llu\t%10f\t", tsc_overall,
				CPU_FREQUENCY * ((double)cpu_clk_unhalted_core_overall /
				       (double)cpu_clk_unhalted_ref_overall));
			fprintf(fp, "%10llu\t",
				instruction_retired_any_overall);
			fprintf(fp, "%10llu\t", cpu_clk_unhalted_core_overall);
			fprintf(fp, "%10llu\t", cpu_clk_unhalted_ref_overall);
			fprintf(fp, "%10llu\t", value_array_overall[0]);
			fprintf(fp, "%10llu\t", value_array_overall[1]);
			fprintf(fp, "%10llu\t", value_array_overall[2]);
			fprintf(fp, "%10llu\t", value_array_overall[3]);
			fprintf(fp, "\n");
		}
		//fflush(fp);
		fflush(stdout);

		iteration++;
		if (logging) {
			fclose(fp);
		}
		time_2 = ctime_long_double();
		
		elapsed_time += (time_2 - time_1);
		if (iteration % 10 == 0) {
			printf("Avg elapsed time %Lf\n", elapsed_time/iteration);
		}
		
		time_3 = ctime_long_double();
		//sleep(1);
		sleep_ms(800);
		time_4 = ctime_long_double();
		time_in_sleep += (time_4 - time_3);
		if (iteration % 10 == 0) {
			printf("Avg Sleep time %Lf\n", time_in_sleep/iteration);
		}
		//
	}

}

void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
	   printf("Pthread Policy is SCHED_FIFO\n");
	   break;
     case SCHED_OTHER:
	   printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
	   printf("Pthread Policy is SCHED_OTHER\n");
	   break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }
}
		
		
int main(int argc, char **argv)
{
	//sam siewert rocks! http://avatar.colorado.edu/resource/linux_examples/pthread.c
	pthread_attr_t rt_sched_attr;
	int rt_max_prio, rt_min_prio, rc;
	struct sched_param rt_param, nrt_param;

	print_scheduler();
	
	pthread_attr_init(&rt_sched_attr);
	pthread_attr_setinheritsched(&rt_sched_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&rt_sched_attr, SCHED_RR);
    
	rt_max_prio = sched_get_priority_max(SCHED_FIFO);
	rt_min_prio = sched_get_priority_min(SCHED_FIFO);
	
	rc=sched_getparam(getpid(), &nrt_param);
    rt_param.sched_priority = rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &rt_param);

    if (rc) {printf("ERROR; sched_setscheduler rc is %d\n", rc); perror(NULL); exit(-1);}
	print_scheduler();
	
	int c;
	while ((c = getopt(argc, argv, "l:")) != -1) {
		switch (c) {
		case 'l':
			logging = true;
			lvalue = optarg;
			break;
		case '?':
			if (optopt == 'l')
				fprintf(stderr,
					"Option -%c requires an argument\n",
					optopt);
		default:
			abort();
		}
	}

	pthread_t mainthread;
	int iret;
	
	iret = pthread_create(&mainthread, NULL, main_thread, NULL);
	
	pthread_join(mainthread, NULL);
	return (1);
}
