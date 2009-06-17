/*		Copyright 2008 Intel Corporation 
 *	The source code contained or described herein and all documents related 
 *  to the source code ("Material") are owned by Intel Corporation or 
 *	its suppliers or licensors. Use of this material must comply with the 
 *	rights and restrictions set forth in the accompnied license terms set
 *  forth in file "license.rtf".
 *
 *	cputopology.h 
 *  This is the header file that contain type definitions 
 *  and prototypes of functions in the file cpu_topo.c
 *	The source files can be compiled under 32-bit and 64-bit Windows and Linux.
 *  
 *	Written by Patrick Fay and Shihjong Kuo
 */
#ifndef TOPOLOGY_H__
#define TOPOLOGY_H__

#ifdef __linux__
#ifndef __int64 
#define __int64 long long
#endif
#ifndef __int32 
#define __int32 int
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef DWORD_PTR
#define DWORD_PTR unsigned long *
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef BOOL
#define BOOL char
#endif

#ifdef __x86_64__
#define AFFINITY_MASK unsigned __int64
#else
#define AFFINITY_MASK unsigned __int32
#endif

#else
// MS specific
#if _MSC_VER < 1300
#ifndef DWORD_PTR
#define DWORD_PTR unsigned long *
#endif
#endif


#ifdef _M_IA64
typedef unsigned __int64 AFFINITY_MASK;
#else
typedef unsigned __int32 AFFINITY_MASK;
#endif

#endif

#define MAX_LOG_CPU (8*sizeof(DWORD_PTR))
#define MAX_PACKAGES MAX_LOG_CPU
#define MAX_CORES MAX_LOG_CPU


#define MAX_CPUS_ARRAY 64
#define MAX_LEAFS 30
#define MAX_CACHE_SUBLEAFS  6
#define MAX_LEAFS_EXT MAX_LEAFS

typedef struct dcache_str {
   int L1;         // L1Data size 
   int L1i;        // L1 instruction size
   int L1i_type;   // L1 instruction type (0 == size in bytes, 1= size in trace cache uops)
   int L2;         // L2 size
   int L3;         // L3 size
   int cl;         // cache line size
   int sectored;   // cache lines sectored
} leaf2_cache_struct;


#define ENUM_ALL (0xffffffff)

#define _MSGTYP_GENERAL_ERROR				0x80000000
#define _MSGTYP_ABORT						0xc0000000
#define _MSGTYP_CHECKBIOS_CPUIDMAXSETTING	0x88000000  
#define _MSGTYP_OSGT64T_PRESENT				0xC4000000		// The # of processors supported by current OS exceed 
//						those of legacy win32 (32 processors) and win64 API (64)
#define _MSGTYP_UNKNOWNERR_OS				0xC2000000		
#define _MSGTYP_USERAFFINITYERR				0xC1000000		
#define _MSGTYP_TOPOLOGY_NOTANALYZED		0xC0800000		
#define _MSGTYP_UNK_AFFINTY_OPERATION		0x84000000  

typedef struct 
{
	unsigned __int32 EAX,EBX,ECX,EDX;
} CPUIDinfo;

typedef struct 
{
	CPUIDinfo *subleaf[MAX_CACHE_SUBLEAFS];
	unsigned __int32 subleaf_max;
} CPUIDinfox;



typedef struct {
	unsigned  maxByteLength;
	unsigned char *AffinityMask;
} GenericAffinityMask;
// The width of affinity mask in legacy Windows API is 32 or 64, depending on 
// 32-bit or 64-bit OS.
// Linux abstract its equivalent bitmap cpumask_t from direct programmer access, 
// cpumask_t can support more than 64 cpu, but is configured at kernel 
// compile time by configuration parameter.
// To abstract the size difference of the bitmap used by different OSes 
// for topology analysis, we use an unsigned char buffer that can 
// map to different OS implementation's affinity mask data structure


 
typedef struct {
	char description[256];
	char descShort[64];
	unsigned level;  // start at 1
	unsigned type;   // cache type (instruction, data, combined)
	unsigned sizeKB; 
	unsigned how_many_threads_share_cache; 
	unsigned how_many_caches_share_level; 
} cacheDetail_str;

typedef struct {
	unsigned  dim[2]; // xdim and ydim
	unsigned  *data;  // data array to be malloc'd
} Dyn2Arr_str;

typedef struct {
	unsigned  dim[1]; // xdim 
	unsigned  *data;  // data array to be malloc'd
} Dyn1Arr_str;

typedef struct {
	int size;
	int used;
	char *buffer;
} DynCharBuf_str;



typedef struct {
	unsigned __int32 APICID;			// the full x2APIC ID or initial APIC ID of a logical 
							//	processor assigned by HW
	unsigned __int32 OrdIndexOAMsk;	// An ordinal index (zero-based) for each logical 
							//  processor in the system, 1:1 with "APICID" 
	// Next three members are the sub IDs for processor topology enumeration
	unsigned __int32 pkg_IDAPIC;		// Pkg_ID field, subset of APICID bits 
							//  to distinguish different packages
	unsigned __int32 Core_IDAPIC;		// Core_ID field, subset of APICID bits to 
							//  distinguish different cores in a package
	unsigned __int32 SMT_IDAPIC;		// SMT_ID field, subset of APICID bits to 
						//  distinguish different logical processors in a core
	// the next three members stores a numbering scheme of ordinal index 
	// for each level of the processor topology. 
	unsigned __int32 packageORD;		// a zero-based numbering scheme for each physical 
							//  package in the system
	unsigned __int32 coreORD;	// a zero-based numbering scheme for each core in the same package
	unsigned __int32 threadORD;	// a zero-based numbering scheme for each thread in the same core
	// Next two members are the sub IDs for cache topology enumeration
	unsigned __int32 EaCacheSMTIDAPIC[MAX_CACHE_SUBLEAFS];	// SMT_ID field, subset of APICID bits 
				// to distinguish different logical processors sharing the same cache level
	unsigned __int32 EaCacheIDAPIC[MAX_CACHE_SUBLEAFS];    // sub ID to enumerate different cache entities
				// of the cache level corresponding to the array index/cpuid leaf 4 subleaf index
	// the next three members stores a numbering scheme of ordinal index 
	// for enumerating different cache entities of a cache level, and enumerating 
	// logical processors sharing the same cache entity.
	unsigned __int32 EachCacheORD[MAX_CACHE_SUBLEAFS];       // a zero-based numbering scheme 
				// for each cache entity of the specified cache level in the system
	unsigned __int32 threadPerEaCacheORD[MAX_CACHE_SUBLEAFS];	// a zero-based numbering scheme 
				// for each logical processor sharing the same cache of the specified cache level 

} IdAffMskOrdMapping;


// we are going to put an assortment of global variable, 1D and 2D arrays into 
// a data structure. This is the declaration
typedef struct
{	// for each logical processor we need spaces to store APIC ID, 
	// sub IDs, affinity mappings, etc.
	IdAffMskOrdMapping *pApicAffOrdMapping; 

	// workspace for storing hierarchical counts of each level 
	Dyn1Arr_str		perPkg_detectedCoresCount;
	Dyn2Arr_str 	perCore_detectedThreadsCount;
	// workspace for storing hierarchical counts relative to the cache topology 
	// of the largest unified cache (may be shared by several cores)
	Dyn1Arr_str		perCache_detectedCoreCount;
	Dyn2Arr_str		perEachCache_detectedThreadCount;
	// we use an error code to indicate any abnoral situation
	unsigned 	error;
	// If CPUID full reporting capability has been restricted, we need to be aware of it.
	unsigned 	Alert_BiosCPUIDmaxLimitSetting;

	unsigned 	OSProcessorCount;  // how many logical processor the OS sees
	unsigned 	hasLeafB;			// flag to keep track of whether CPUID leaf 0BH is supported
	unsigned 	maxCacheSubleaf;  // highest CPUID leaf 4 subleaf index in a processor

	// the following global variables are the total counts in the system resulting from software enumeration
	unsigned 	EnumeratedPkgCount;
	unsigned 	EnumeratedCoreCount;
	unsigned 	EnumeratedThreadCount;
	// CPUID ID leaf 4 can report data for several cache levels, we'll keep track of each cache level
	unsigned 	EnumeratedEachCacheCount[MAX_CACHE_SUBLEAFS];
	// the following global variables are parameters related to 
	//	extracting sub IDs from an APIC ID, common to all processors in the system
	unsigned 	SMTSelectMask;
	unsigned 	PkgSelectMask;
	unsigned 	CoreSelectMask;
	unsigned 	PkgSelectMaskShift;
	unsigned 	SMTMaskWidth;
	// We'll do sub ID extractions using parameters from each cache level
	unsigned 	EachCacheSelectMask[MAX_CACHE_SUBLEAFS];
	unsigned 	EachCacheMaskWidth[MAX_CACHE_SUBLEAFS];

	// the following global variables are used for product capability identification
	unsigned 	HWMT_SMTperCore;
	unsigned 	HWMT_SMTperPkg;
	// a data structure that can store simple leaves and complex subleaves of all supported leaf indices of CPUID
	CPUIDinfox      *cpuid_values;
	// workspace of our generic affinitymask structure to allow iteration over each logical processors in the system
	GenericAffinityMask cpu_generic_processAffinity;
	GenericAffinityMask	cpu_generic_systemAffinity;
	// workspeace to assist text display of cache topology information
	cacheDetail_str cacheDetail[MAX_CACHE_SUBLEAFS];	

} GLKTSN_T;


unsigned long getBitsFromDWORD(const unsigned int val, const char from, const char to);
static unsigned  createMask(unsigned  numEntries, unsigned  *maskLength);
unsigned SlectOrdfromPkg(unsigned  package,unsigned  core, unsigned  logical);

unsigned  GetOSLogicalProcessorCount();
unsigned  GetSysProcessorPackageCount();
unsigned  GetProcessorCoreCount();
unsigned  GetLogicalProcessorCount();
unsigned  GetCoresPerPackageProcessorCount();
unsigned  GetProcessorPackageCount();
unsigned  GetLogicalPerCoreProcessorCount();
unsigned  getAPICID(unsigned  processor);

unsigned  GetCoreCount(unsigned long package_ordinal);
unsigned  GetThreadCount(unsigned long package_ordinal, unsigned long core_ordinal);
void InitCpuTopology();

unsigned BindContext(unsigned cpu);
void  SetChkProcessAffinityConsistency(unsigned  lcl_OSProcessorCount);
void SetGenericAffinityBit(GenericAffinityMask *pAffinityMap, unsigned  cpu);
unsigned  GetMaxCPUSupportedByOS();


void get_cpuid_info(CPUIDinfo * info, const unsigned int func, const unsigned int subfunc);
	
#endif

