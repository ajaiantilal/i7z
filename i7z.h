//i7z.h
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2009 Abhishek Jaiantilal
 *
 *   Under GPL v2
 *
 * ----------------------------------------------------------------------- */

//structure to store the information about the processor
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

void get_familyinformation (struct family_info *proc_info);

double estimate_MHz ();

uint64_t get_msr_value (int cpu, uint32_t reg, unsigned int highbit,
			unsigned int lowbit);

uint64_t set_msr_value (int cpu, uint32_t reg, uint64_t data);


#ifdef USE_INTEL_CPUID
void get_CPUs_info (unsigned int *num_Logical_OS,
		    unsigned int *num_Logical_process,
		    unsigned int *num_Processor_Core,
		    unsigned int *num_Physical_Socket);

#endif
