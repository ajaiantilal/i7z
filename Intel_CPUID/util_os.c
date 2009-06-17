/*		Copyright 2008 Intel Corporation 
 *	util_os.c 
 *  This is the auxilary source file that contain wrapper routines of 
 *  OS-specific services called by functions in the file cpu_topo.c
 *	The source files can be compiled under 32-bit and 64-bit Windows and Linux.
 *  
 *	Written by Patrick Fay and Shihjong Kuo
 */
#include "cputopology.h"

#ifdef __linux__


#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <alloca.h>
#include <stdarg.h>


#define __cdecl 
#ifndef _alloca
#define _alloca alloca
#endif

#ifdef __x86_64__
#define LNX_PTR2INT unsigned long long
#define LNX_MY1CON 1LL
#else
#define LNX_PTR2INT unsigned int
#define LNX_MY1CON 1
#endif

#else

#include <windows.h>

#ifdef _M_IA64
#define LNX_PTR2INT unsigned long long
#define LNX_MY1CON 1LL
#else
#define LNX_PTR2INT unsigned int
#define LNX_MY1CON 1
#endif

#endif


extern  GLKTSN_T * glbl_ptr;
extern int countBits(DWORD_PTR x);


/*
 * BindContext
 * A wrapper function that can compile under two OS environments
 * The size of the bitmap that underlies cpu_set_t is configurable 
 * at Linux Kernel Compile time. Each distro may set limit on its own.
 * Some newer Linux distro may support 256 logical processors, 
 * For simplicity we don't show the check for range on the ordinal index 
 * of the target cpu in Linux, interested reader can check Linux kernel documentation.
 * Current Windows OS has size limit of 64 cpu in 64-bit mode, 32 in 32-bit mode
 * the size limit is checked.
 * Arguments:
 *		cpu	:	the ordinal index to reference a logical processor in the system
 * Return:        0 is no error
 */
unsigned BindContext(unsigned cpu)
{unsigned ret = -1;
#ifdef __linux__
	cpu_set_t currentCPU;	 
		// add check for size of cpumask_t.
		CPU_ZERO(&currentCPU);
		// turn on the equivalent bit inside the bitmap corresponding to affinitymask
		CPU_SET(cpu, &currentCPU);
		if ( !sched_setaffinity (0, sizeof(currentCPU), &currentCPU)  ) 
		{	ret = 0;
		}
#else
	DWORD_PTR affinity;
		if( cpu >= MAX_LOG_CPU ) return ret;
		// flip on the bit in the affinity mask corresponding to the input ordinal index
		affinity = (DWORD_PTR)(LNX_MY1CON << cpu);
		if ( SetThreadAffinityMask(GetCurrentThread(),  affinity) )
		{ ret = 0;}
#endif
	return ret;
}


/*
 * GetMaxCPUSupportedByOS
 * A wrapper function that calls OS specific system API to find out
 * how many logical processor the OS supports
 * Return:        a non-zero value
 */
unsigned  GetMaxCPUSupportedByOS()
{unsigned  lcl_OSProcessorCount = 0;
#ifdef __linux__

	lcl_OSProcessorCount = sysconf(_SC_NPROCESSORS_CONF); //This will tell us how many CPUs are currently enabled.	

#else

	SYSTEM_INFO si;

	GetSystemInfo(&si);
	lcl_OSProcessorCount = si.dwNumberOfProcessors;

#endif
	return lcl_OSProcessorCount;
}

/*
 * SetChkProcessAffinityConsistency
 * A wrapper function that calls OS specific system API to find out
 * the number of logical processor support by OS matches 
 * the same set of logical processors this process is allowed to run on
 * if the two set matches, set the corresponding bits in our
 * generic affinity mask construct.
 * if inconsistency is found, app-specific error code is set
 * Return:        none, 
 */
void  SetChkProcessAffinityConsistency(unsigned  lcl_OSProcessorCount)
{unsigned i;
#ifdef __linux__
	cpu_set_t allowedCPUs;	 

	sched_getaffinity(0, sizeof(allowedCPUs), &allowedCPUs);
	for (i = 0; i < lcl_OSProcessorCount; i++ )
	{
		if ( CPU_ISSET(i, &allowedCPUs) == 0 )
		{
			glbl_ptr->error |= _MSGTYP_USERAFFINITYERR;
		}
		else
		{
			SetGenericAffinityBit(&glbl_ptr->cpu_generic_processAffinity, i);
			SetGenericAffinityBit(&glbl_ptr->cpu_generic_systemAffinity, i);
		}
	}
	
#else
	DWORD_PTR processAffinity;
	DWORD_PTR systemAffinity;

	GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);
	if (lcl_OSProcessorCount > MAX_LOG_CPU)
	{
		glbl_ptr->error |= _MSGTYP_OSGT64T_PRESENT ;  // If the os supports more processors than existing win32 or win64 API, 
												// we need to know the new API interface in that OS
	}
	if ( lcl_OSProcessorCount != (unsigned long) countBits(processAffinity) ) 
	{
		//throw some exception here, no full affinity for the process
		glbl_ptr->error |= _MSGTYP_USERAFFINITYERR;
	}

	if (lcl_OSProcessorCount != (unsigned long) countBits(systemAffinity) ) 
	{
		//throw some exception here, no full system affinity
		glbl_ptr->error |= _MSGTYP_UNKNOWNERR_OS;
	}

	for (i = 0; i < lcl_OSProcessorCount; i++ )
	{
		// This logic assumes that looping over OSProcCount will let us inspect all the affinity bits
		// That is, we can't need more than OSProcessorCount bits in the affinityMask
		if (( (unsigned long) systemAffinity & (LNX_MY1CON << i)) == 0) {
			glbl_ptr->error |= _MSGTYP_USERAFFINITYERR;
		}
		else {
			SetGenericAffinityBit(&glbl_ptr->cpu_generic_systemAffinity, i);
		}
		if (((unsigned long)processAffinity & (LNX_MY1CON << i)) == 0) {
			glbl_ptr->error |= _MSGTYP_USERAFFINITYERR;
		}
		else {
			SetGenericAffinityBit(&glbl_ptr->cpu_generic_processAffinity, i);
		}
	}
#endif

}

