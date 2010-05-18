//i7z.h
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2009 Abhishek Jaiantilal
 *
 *   Under GPL v2
 *
 * ----------------------------------------------------------------------- */

//structure to store the information about the processor
#define proccpuinfo "/proc/cpuinfo"

#ifndef bool
#define bool int
#endif
#define false 0
#define true 1


struct cpu_heirarchy_info{
    int max_online_cpu;
	int num_sockets;
    int sibling_num[32];
	int processor_num[32];
    int package_num[32];
    int coreid_num[32];
	int display_cores[32];
	bool HT;
};

struct cpu_socket_info{
	int max_cpu;
	int socket_num;
	int processor_num[8];
	int num_physical_cores;
	int num_logical_cores;
};

struct family_info
{
  char stepping;
  char model;
  char family;
  char processor_type;
  char extended_model;
  int extended_family;
};

//read TSC() code for 32 and 64-bit
//http://www.mcs.anl.gov/~kazutomo/rdtsc.html

#ifndef x64_BIT
	//code for 32 bit
	static __inline__ unsigned long long int
	rdtsc ()
	{
	  unsigned long long int x;
	  __asm__ volatile (".byte 0x0f, 0x31":"=A" (x));
	  return x;
	}
#endif

#ifdef x64_BIT
	//code for 32 bit
	static __inline__ unsigned long long
	rdtsc (void)
	{
	  unsigned hi, lo;
	  __asm__ __volatile__ ("rdtsc":"=a" (lo), "=d" (hi));
	  return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
	}
#endif


void print_family_info (struct family_info *proc_info);

#ifdef x64_BIT
	void get_vendor (char *vendor_string);
#endif

int turbo_status ();
double cpufreq_info();
void get_familyinformation (struct family_info *proc_info);

double estimate_MHz ();

uint64_t get_msr_value (int cpu, uint32_t reg, unsigned int highbit,
			unsigned int lowbit, int* error_indx);

uint64_t set_msr_value (int cpu, uint32_t reg, uint64_t data);


#ifdef USE_INTEL_CPUID
void get_CPUs_info (unsigned int *num_Logical_OS,
		    unsigned int *num_Logical_process,
		    unsigned int *num_Processor_Core,
		    unsigned int *num_Physical_Socket);

#endif

int get_number_of_present_cpu();
void get_candidate_cores(struct cpu_heirarchy_info* chi);
void get_online_cpus(struct cpu_heirarchy_info* chi);
void get_siblings_list(struct cpu_heirarchy_info* chi);
void get_package_ids(struct cpu_heirarchy_info* chi);
void print_cpu_list(struct cpu_heirarchy_info chi);
void construct_cpu_hierarchy(struct cpu_heirarchy_info *chi);
void Print_Information_Processor();
void Test_Or_Make_MSR_DEVICE_FILES();


int check_and_return_processor(char*strinfo);
int check_and_return_physical_id(char*strinfo);
void construct_sibling_list(struct cpu_heirarchy_info* chi);
void construct_socket_information(struct cpu_heirarchy_info* chi,struct cpu_socket_info* socket_0,struct cpu_socket_info* socket_1);
void print_socket_information(struct cpu_socket_info* socket);
void construct_CPU_Heirarchy_info(struct cpu_heirarchy_info* chi);
void print_CPU_Heirarchy(struct cpu_heirarchy_info chi);
int in_core_list(int ii,int* core_list);


#define SET_ONLINE_ARRAY_MINUS1(online_cpus) {for(i=0;i<32;i++) online_cpus[i]=-1;}
#define SET_ONLINE_ARRAY_PLUS1(online_cpus) {for(i=0;i<32;i++) online_cpus[i]=1;}

#define SET_IF_TRUE(error_indx,a,b) if(error_indx) a=b;
#define CONTINUE_IF_TRUE(cond) if(cond) continue;
#define RETURN_IF_TRUE(cond) if(cond) return;

//due to the fact that sometimes 100.0>100,  the below macro checks till 101
#define THRESHOLD_BETWEEN_0_100(cond) (cond>=0 && cond <=101)? cond: __builtin_inf()

//due to the fact that sometimes 100.0>100,  the below macro checks till 101
#define IS_THIS_BETWEEN_0_100(cond) (cond>=0 && cond <=101 && !isinf(cond) && !isnan(cond))? 1: 0

#define THRESHOLD_BETWEEN_0_6000(cond) (cond>=0 && cond <=6000)? cond: __builtin_inf()
