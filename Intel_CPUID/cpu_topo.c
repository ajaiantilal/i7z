/*		Copyright 2005-2008 Intel Corporation 
 *	cpu_topo.c 
 *  This is the main source file to demonstrate processor topology enumeration 
 *	algorithm in Intel(R) 64 and IA-32 platforms with hardware multi-threading capability.
 *	Hardware multithreading capabilities covered here include: 
 *	HyperThreading Technology, multi-core, and multi-processor platform. 
 *	The algorithms demonstrated here include: system topology enumeration, and 
 *	cache topology enumeration.
 *	System topology enumeration relates to mapping each logical processor in a MP system 
 *	with respect to the three-level topological hierarchy: SMT, processor core (Core), 
 *	physical package (Pkg). 
 *	Cache topology enumeration relates to mapping each logical processor with respect to a 
 *	cache level in the cache hierachy of the physical processor. 
 *	Both System topology enumeration and cache topology enumeration make use of raw data 
 *	reported by CPUID instruction from each logical processor in the system. 
 *	Support functions that use assembly sequence or use OS specific functions are provided  
 *	in separate source file.

 *	The reference code provided in this file is for demonstration purpose only. It assumes
 *	the hardware topology configuration within a coherent domain does not change during
 *	the life of an OS session. If an OS support advanced features that can change 
 *	hardware topology configurations, more sophisticated adaptation may be necessary
 *	to account for the hardware configuration change that might have added and reduced 
 *  the number of logical processors being managed by the OS.
 *
 *	User should also`be aware that the system topology enumeration algorithm presented here 
 *	relies on CPUID instruction providing raw data reflecting the native hardware 
 *	configuration. When an application runs inside a virtual machine hosted by a 
 *	Virtual Machine Monitor (VMM), any CPUID instructions issued by an app (or a guest OS) 
 *	are trapped by the VMM and it is the VMM's responsibility and decision to emulate  
 *	CPUID return data to the virtual machines. When deploying topology enumeration code based on 
 *	CPUID inside a VM environment, the user must consult with the VMM vendor on how an VMM
 *	will emulate CPUID instruction relating to topology enumeration.

 *  Specify constant /DBUILD_MAIN in the compiler option to compile for console executable, 
 *  otherwise main() will not be compiled.
 *	Adaptation of this sample code for re-entrant threading library is outside the scope of this
 *	reference code.
 *
 *  Two batch files are provided to assist compiling the source files in Windows OS
 *  Two shell script files are provided to assist compiling the source files in Linux OS
 *
 *	Written by Patrick Fay, Ronen Zohar and Shihjong Kuo
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

#ifdef __INTEL_COMPILER

#endif

#endif

#include <stdio.h>


int MaskToHexStringGenericAffinityMask(GenericAffinityMask *pAffinityMap, unsigned  len, char *str);

extern  void get_cpuid_info (CPUIDinfo *, const unsigned int, const unsigned int);


/*
 * _CPUID
 *
 * Returns the raw data reported in 4 registers by CPUID, support sub-leaf reporting
 *			The CPUID instrinsic in MSC does not support sub-leaf reporting
 * Arguments:
 *     info       Point to strucrture to get output from CPIUD instruction
 *     func       CPUID Leaf number
 *     subfunc    CPUID Subleaf number
 * Return:        None
 */
void __cdecl _CPUID(CPUIDinfo *info, const unsigned int func, const unsigned int subfunc)
{
	get_cpuid_info(info, func, subfunc);
}

// Simpler version for CPUID leaf that do not require sub-leaf reporting
static void CPUID(CPUIDinfo *info, const unsigned int func)
{
	_CPUID(info,func,0);
}



/*
 * getBitsFromDWORD
 *
 * Returns of bits [to:from] of DWORD
 *
 * Arguments:
 *     val        DWORD to extract bits from
 *     from       Low order bit
 *     to         High order bit
 * Return:        Specified bits from DWORD val
 */
unsigned long getBitsFromDWORD(const unsigned int val, const char from, const char to)
{
	unsigned long mask = (1<<(to+1)) - 1;
	if (to == 31)	return val >> from;
	
	return (val & mask) >> from;
}



/*
 * countBits
 *
 *  count the number of bits that are set to 1 from the input
 *
 * Arguments:
 *     x          Argument to count set bits in, no restriction on set bits distribution
 *
 * Return:        Number of bits set to 1
 */
int countBits(DWORD_PTR x)
{
	int res = 0, i;
	LNX_PTR2INT myll;
	myll = (LNX_PTR2INT)(x);
	for(i=0; i < (8*sizeof(myll)); i++) {
		if((myll & (LNX_MY1CON << i)) != 0) {
			res++;
		}
	}
	return res;
}

/*
 * myBitScanReverse
 *
 * Returns of bits [to:from] of DWORD
 * This c-emulation of the BSR instruction is shown here for tool portability  
 *
 * Arguments:
 *     index      bit offset of the most significant bit that's not 0 found in mask
 *     mask       input data to search the most significant bit
  * Return:        1 if a non-zero bit is found, otherwise 0
 */
unsigned char myBitScanReverse(unsigned  * index, unsigned long mask) 
{unsigned long i;

	for(i=(8*sizeof(unsigned long)); i > 0; i--) {
		if((mask & (LNX_MY1CON << (i-1))) != 0) {
			*index = (unsigned) (i-1);
			break;
		}
	}
	return (unsigned char)( mask != 0);
}



/* createMask
 *
 * Derive a bit mask and associated mask width (# of bits) such that 
 *	the bit mask is wide enough to select the specified number of 
 *	distinct values "numEntries" within the bit field defined by maskWidth.
 * Arguments:
 *     numEntries : The number of entries in the bit field for which a mask needs to be created
 *     maskWidth: Optional argument, pointer to argument that get the mask width (# of bits)
 *
 * Return:        Created mask of all 1's up to the maskWidth
 */
static unsigned  createMask(unsigned  numEntries, unsigned  *maskWidth)
{	unsigned  i;
	unsigned long k;

	// NearestPo2(numEntries) is the nearest power of 2 integer that is not less than numEntries
	// The most significant bit of (numEntries * 2 -1) matches the above definition

	k = (unsigned long)(numEntries) * 2 -1;

	if (myBitScanReverse(&i, k) == 0)
	{	// No bits set
		if (maskWidth) *maskWidth = 0;
		return 0;
	}

	if (maskWidth)		*maskWidth = i;
	
	if (i == 31)	return (unsigned ) -1;	

	return (1 << i) -1;
}



GLKTSN_T *glbl_ptr=NULL;


/* AllocateGenericAffinityMask
 *      
 * Allocate the memory needed for the bitmap of a generic affinity mask
 *	The memory buffer needed must be large enough to cover the specified maxcpu number
 *	Each cpu is represented by one bit in the bitmap
 *
 * Arguments:     
 *   GenericAffintyMask ptr to be filled in.
 *   maxcpu: max number of logical processors the bitmap of this generic affinity mask needs to  handle
 *
 * Return:        0 if no errors, -1 otherwise
 */
int AllocateGenericAffinityMask(GenericAffinityMask *pAffinityMap, unsigned  maxcpu)
{
	int bytes;
	// Allocate an unsigned char string long enough (cpus/8 + 1) bytes to have 1 bit per cpu.
	// cpu 0 is the lsb of byte0, cpu1 is byte[0]:1, cpu2 is byte[0]:2 etc

	bytes = (maxcpu >> 3) + 1;
	pAffinityMap->AffinityMask = (unsigned char *)malloc(bytes*sizeof(unsigned char));
	if(pAffinityMap->AffinityMask == NULL) {
		printf("Error: AllocateGenericAffinityMask 2nd malloc failed at %s %d. Bye\n", __FILE__, __LINE__);
		return -1;
	}
	pAffinityMap->maxByteLength = bytes;
	memset(pAffinityMap->AffinityMask, 0, bytes*sizeof(unsigned char));
	return 0;
}


/* FreeGenericAffinityMask
 *  free up the memory allocated to the bitmap of a generic affinity mask
 *
 * args: 
 *  pAffinityMap : pointer to a generic affinity mask
 *
 * returns none
 */

void FreeGenericAffinityMask(GenericAffinityMask *pAffinityMap)
{
	free(pAffinityMap->AffinityMask);
	pAffinityMap->maxByteLength = 0;
}

/* ClearGenericAffinityMask
 *  Clear all the affinity bits in the bitmap of a generic affinity mask
 *
 * args: 
 *  pAffinityMap : pointer to a generic affinity mask
 *
 * returns none
 */
void ClearGenericAffinityMask(GenericAffinityMask *pAffinityMap)
{
	memset(pAffinityMap->AffinityMask, 0, pAffinityMap->maxByteLength);
}

/* SetGenericAffinityBit
 *  Set the affinity bit corresponding to a specified logical processor .
 *	The bitmap in a generic affinity mask is 
 *
 * args: 
 *  pAffinityMap : pointer to a generic affinity mask
 *  cpu			: an ordinal number that reference a logical processor visible to the OS
 *	return none, abort if error occured
 */
void SetGenericAffinityBit(GenericAffinityMask *pAffinityMap, unsigned  cpu)
{
	if (cpu < (pAffinityMap->maxByteLength << 3)  ) { 
		pAffinityMap->AffinityMask[ cpu >> 3  ] |=  1 << (cpu % 8);
	} else {
		printf("Error: Wanted cpu %u but mask only supports up to %u cpus at %s %d. Bye\n", 
			cpu, (pAffinityMap->maxByteLength << 3), __FILE__, __LINE__);
		exit(2);
	}
}

/* CompareEqualGenericAffinity
 *  compare two generic affinity masks if the identical set of 
 *	logical processors are set in two generic affinity mask bitmaps. 
 *  Since each generic affinity mask is allocated by user and can be of different length,
 *	if the length of one mask is shorter then check the shorter part first. 
 *  If the second mask is longer than the first mask, check that the longer part is all zero.
 *
 * args: 
 *  pAffinityMap1 : pointer to a generic affinity mask
 *  pAffinityMap2 : pointer to another generic affinity mask
 *
 * returns 0 if equal, 1 otherwise
 */
int CompareEqualGenericAffinity(GenericAffinityMask *pAffinityMap1, GenericAffinityMask *pAffinityMap2)
{
	int  rc;
	unsigned  i, smaller;
	
	smaller = pAffinityMap1->maxByteLength;
	if(smaller > pAffinityMap2->maxByteLength) {
		smaller = pAffinityMap2->maxByteLength;
	}
	if( !smaller) return 1;

	rc = memcmp(pAffinityMap1->AffinityMask, pAffinityMap2->AffinityMask, smaller);
	if(rc != 0) {
		return 1;
	} else {
		if(pAffinityMap1->maxByteLength ==  pAffinityMap2->maxByteLength) {
			return 0;
		} else if(pAffinityMap1->maxByteLength >  pAffinityMap2->maxByteLength) {
			for(i=smaller; i < pAffinityMap1->maxByteLength; i++) {
				if(pAffinityMap1->AffinityMask[i] != 0) {
					return 1;
				}
			}
			return 0;
		} else {
			for(i=smaller; i < pAffinityMap2->maxByteLength; i++) {
				if(pAffinityMap2->AffinityMask[i] != 0) {
					return 1;
				}
			}
			return 0;
		}
	}
}

/* ClearGenericAffinityBit
 *  Clear (set to 0) the cpu'th bit in the generic affinity mask.
 *
 * args: generic affinty mask ptr
 *
 * returns 0 if no error, -1 otherise
 *         
 */
int ClearGenericAffinityBit(GenericAffinityMask *pAffinityMap, unsigned  cpu)
{
	if (cpu < (pAffinityMap->maxByteLength << 3)  ) { 
		pAffinityMap->AffinityMask[ cpu >> 3  ] ^=  1 << (cpu % 8);
		return 0;
	} else {
		printf("Error: Wanted cpu %d but mask only supports up to %d cpus at %s %d. Bye\n", 
			cpu, (pAffinityMap->maxByteLength << 3), __FILE__, __LINE__);
		return -1;
	}
}

/* TestGenericAffinityBit
 *  check the cpu'th bit of the affinity mask and return 1 if the bit is set and 0 if the bit is 0.
 *
 * args: 
 *   generic affinty mask ptr
 *   cpu cpu number. Signifies bit in the unsigned char affinity mask to be checked
 *
 * returns 0 if the cpu's bit is clear, returns 1 if the bit is set, -1 if an error
 *   The error condition makes it where you can't just check for '0' or 'not 0'
 *         
 */
unsigned char TestGenericAffinityBit(GenericAffinityMask *pAffinityMap, unsigned  cpu)
{
	
	if (cpu < (pAffinityMap->maxByteLength << 3)  ) { 
		if((pAffinityMap->AffinityMask[ cpu >> 3  ] & (1 << (cpu % 8)))) {
			return 1;
		} else {
			return 0;
		}
	} else {
		printf("Error: Wanted cpu %d but mask only supports up to %d cpus at %s %d. Bye\n", 
			cpu, (pAffinityMap->maxByteLength << 3), __FILE__, __LINE__);
		return 0xff;
	}
}

/* CompressHexMask
 *  Compress hexdecimal string representation  by trimming off leading and trailing zero
 *  Drop leading 0s from input hex string and replace trailing 0s with 'xY' where Y is the number of trailing 0s
 *  The replacement of trailing zeros is only done if it makes the string shorter.   
 *	E.g. 0000001F000H is compressed as 1Fz3, 300000H as 3z5
 *
 * args: 
 *   char *str: pointer to char string into which the hex string will be compressed in place
 *
 * returns the number of bytes in the string
 *         
 */
int CompressHexMask( char *str)
{
	int i, j, k, m;
	
	// now drop the leading zeroes... find the 1st nonzero, then start moving it over
	j = strlen(str);
	k=0;
	for(i = 0; i < j; i++) {
		if(str[i] != '0') {
			k = i;
			break;
		}
	}
	if(k > 0) {
		m = 0;
		for(i = k; i <= j; i++) {
			str[m++] = str[i];
		}
	}
	// now look for trailing zeroes.. Count how many we havethen start moving it over
	k=0;
	j = strlen(str);
	for(i = j-1; i >= 0; i--) {
		if(str[i] != '0') {
			break;
		} else {
			k++;
		}
	}
	{
		char tstr[32];
		int tlen;
		sprintf(tstr, "z%d", k);
		tlen = strlen(tstr);
		if(k > tlen) {
			strcpy(str+(j-k), tstr);
			j = strlen(str);
		}
	}
	return j;
}
	
/* FormatSingleBitMask
 *  prints the generic affinity mask in hex (print 2 hex bytes per 1 byte of affinity mask.
 *  Have to reverse the string so that the least signficant bit (representing cpu 0) 
 *  is the rightmost bit.
 *  Also the
 *
 * args: 
 *   cpu: ordinal number that specifies a logical processor within a generic affinity mask
 *   len: length of string to receive the output
 *   char *str: pointer to char string into which the hex string will be put
 *
 * returns -1 if the string is too short to hold the output, otherwise returns the number of bytes in the string
 *         
 */
int FormatSingleBitMask(unsigned cpu, unsigned  len, char *str)
{
	unsigned i, mod_8, how_many_zeroes;
	char tstr[32];
	
	how_many_zeroes = 2*(cpu/8);
	mod_8 = cpu % 8;
	if(mod_8 > 3) {
		how_many_zeroes++;
		mod_8 -= 4;
	}
	if(how_many_zeroes > 2) {
		sprintf(tstr, "%xz%d", (1 << mod_8), how_many_zeroes);
	} else if(how_many_zeroes == 2) {
		sprintf(tstr, "%x00", (1 << mod_8));
	} else if(how_many_zeroes == 1) {
		sprintf(tstr, "%x0", (1 << mod_8));
	} else {
		sprintf(tstr, "%x", (1 << mod_8));
	}
	i = strlen(tstr);
	if(i < len) {
		strcpy(str, tstr);
	} else {
		if(len > 0) {
			str[0] = 0;
		}
		return -1;
	}
	return i;
}


/*
 * GetApicID
 * Returns APIC ID from leaf B if it else from leaf 1
 * Arguments:     None
 * Return:        APIC ID
 */
static unsigned  GetApicID() {
	CPUIDinfo info;

	if ( glbl_ptr->hasLeafB) {
		CPUID(&info,0xB); // query subleaf 0 of leaf B
		return info.EDX;  //  x2APIC ID
	}
	CPUID(&info,1);
	return (BYTE)(getBitsFromDWORD(info.EBX,24,31));  // zero extend 8-bit initial APIC ID
}


// select the system-wide ordinal number of the first logical processor the is located 
// within the entity specified from the hierarchical ordinal number scheme
// package: ordinal number of a package; ENUM_ALL means any package
// core   : ordinal number of a core in the specified package; ENUM_ALL means any core
// logical: ordinal number of a logical processor within the specifed core; ; ENUM_ALL means any thread
// returns: the system wide ordinal number of the first logical processor that matches the 
//    specified (package, core, logical) triplet specification. 
unsigned SlectOrdfromPkg(unsigned  package,unsigned  core, unsigned  logical)
{
	unsigned  i;

	if (package != ENUM_ALL && package >= GetSysProcessorPackageCount())		return 0;

	for (i=0;i< GetOSLogicalProcessorCount();i++)
	{
		////
		if (!(package == ENUM_ALL ||  glbl_ptr->pApicAffOrdMapping[i].packageORD == package))
			continue;		
		
		if (!(core == ENUM_ALL || core == glbl_ptr->pApicAffOrdMapping[i].coreORD))
			continue;

		if (!(logical == ENUM_ALL || logical == glbl_ptr->pApicAffOrdMapping[i].threadORD))
			continue;

		//res = res | (AFFINITY_MASK)(pApicAffOrdMapping[i].AffinityMask);
		return i;
	}
	return 0;
}

// Derive bitmask extraction parameters used to extract/decompose x2APIC ID.
// The algorithm assumes CPUID feature symmetry across all physical packages. 
// Since CPUID reporting by each logical processor in a physical package are identical, we only execute CPUID 
// on one logical processor to derive these system-wide parameters
int CPUTopologyLeafBConstants()
{
	CPUIDinfo infoB;
	int wasCoreReported = 0;
	int wasThreadReported = 0;
	int subLeaf = 0, levelType, levelShift;
	unsigned long		coreplusSMT_Mask = 0;

	do
	{	// we already tested CPUID leaf 0BH contain valid sub-leaves
		_CPUID(&infoB,0xB,subLeaf);
		if (infoB.EBX == 0)  
		{	// if EBX ==0 then this subleaf is not valid, we can exit the loop
			break;
		}
		levelType = getBitsFromDWORD(infoB.ECX,8,15);
		levelShift = getBitsFromDWORD(infoB.EAX,0,4);
		switch (levelType) 
		{
		case 1:		//level type is SMT, so levelShift is the SMT_Mask_Width
			glbl_ptr->SMTSelectMask = ~((-1) << levelShift);  
			glbl_ptr->SMTMaskWidth = levelShift;
			wasThreadReported = 1;
			break;
		case 2:	//level type is Core, so levelShift is the CorePlsuSMT_Mask_Width
			coreplusSMT_Mask = ~((-1) << levelShift);
			glbl_ptr->PkgSelectMaskShift =  levelShift;
			glbl_ptr->PkgSelectMask = (-1) ^ coreplusSMT_Mask;
			wasCoreReported = 1;
			break;
		default:
			// handle in the future
			break;
		}				
		subLeaf++;
	} while (1);

	if (wasThreadReported && wasCoreReported)
	{
		glbl_ptr->CoreSelectMask = coreplusSMT_Mask ^ glbl_ptr->SMTSelectMask;
	}
	else if (!wasCoreReported && wasThreadReported)
	{
		glbl_ptr->CoreSelectMask = 0;
		glbl_ptr->PkgSelectMaskShift =  glbl_ptr->SMTMaskWidth;
		glbl_ptr->PkgSelectMask = (-1) ^ glbl_ptr->SMTSelectMask;
	}
	else //(case where !wasThreadReported)
	{
		// throw an error, this should not happen if hardware function normally
		glbl_ptr->error |= _MSGTYP_GENERAL_ERROR;
	}
	if( glbl_ptr->error)return -1;
	else return 0;
}

// Calculate parameters used to extract/decompose Initial APIC ID.
// The algorithm assumes CPUID feature symmetry across all physical packages. 
// Since CPUID reporting by each logical processor in a physical package are identical, we only execute CPUID 
// on one logical processor to derive these system-wide parameters
/*
 * CPUTopologyLegacyConstants
 * Derive bitmask extraction parameter using CPUID leaf 1 and leaf 4
 * Arguments:
 *     info       Point to strucrture containing CPIUD instruction leaf 1 data 
 *     maxCPUID   Maximum CPUID Leaf number supported by the processor
 * Return:        0 is no error
 */
int CPUTopologyLegacyConstants(	CPUIDinfo  *pinfo, DWORD maxCPUID)
{
	unsigned   corePlusSMTIDMaxCnt;
	unsigned 	coreIDMaxCnt =1;
	unsigned 	SMTIDPerCoreMaxCnt = 1;
	// CPUID.1:EBX[23:16] provides the max # IDs that can be enumerated under the CorePlusSMT_SelectMask
	
	corePlusSMTIDMaxCnt = getBitsFromDWORD(pinfo->EBX,16,23);
		// support CPUID 4?
	if (maxCPUID >= 4)
	{
			CPUIDinfo info4;
			_CPUID(&info4, 4, 0);
			// (CPUID.(EAX=4, ECX=00:EAX[31:26] +1 ) provides the max # Core_IDs that's allocated in a package, this is 
			coreIDMaxCnt = getBitsFromDWORD(info4.EAX,26,31)+1; // related to coreMaskWidth
			SMTIDPerCoreMaxCnt = corePlusSMTIDMaxCnt / coreIDMaxCnt;
	}
	else 
		// no support for CPUID leaf 4 but caller has verified  HT support
	{	if (!glbl_ptr->Alert_BiosCPUIDmaxLimitSetting) {
				coreIDMaxCnt = 1;
				SMTIDPerCoreMaxCnt = corePlusSMTIDMaxCnt / coreIDMaxCnt;
		}
		else { // we got here most likely because IA32_MISC_ENABLES[22] was set to 1 by BIOS
				glbl_ptr->error |= _MSGTYP_CHECKBIOS_CPUIDMAXSETTING;  // IA32_MISC_ENABLES[22] may have been set to 1, will cause inaccurate reporting
		}
	}
	
	glbl_ptr->SMTSelectMask = createMask(SMTIDPerCoreMaxCnt,&glbl_ptr->SMTMaskWidth);
	glbl_ptr->CoreSelectMask = createMask(coreIDMaxCnt,&glbl_ptr->PkgSelectMaskShift);
	glbl_ptr->PkgSelectMaskShift += glbl_ptr->SMTMaskWidth;
	glbl_ptr->CoreSelectMask <<= glbl_ptr->SMTMaskWidth;
	glbl_ptr->PkgSelectMask = (-1) ^ (glbl_ptr->CoreSelectMask | glbl_ptr->SMTSelectMask);
	return 0;
}


/*
 * GetCacheTotalLize
 *
 * Caluculates the total capacity (bytes) from the cache parameters reported by CPUID leaf 4
 *			the caller provides the raw data reported by the target sub leaf of cpuid leaf 4 
 * Arguments:
 *     maxCPUID   Maximum CPUID Leaf number supported by the processor, provided by parent
 *     cache_type       the cache type encoding recognized by CPIUD instruction leaf 4 for the target cache level
 *
 * Return:        the sub-leaf index corresponding to the largest cache of specified cache type
 *
 */
unsigned long GetCacheTotalLize(CPUIDinfo info)
{unsigned long	LnSz, SectorSz, WaySz, SetSz;
	LnSz = getBitsFromDWORD(info.EBX, 0, 11 ) + 1;
	SectorSz = getBitsFromDWORD(info.EBX, 12, 21 ) + 1;
	WaySz = getBitsFromDWORD(info.EBX, 22, 31 ) + 1;
	SetSz = getBitsFromDWORD(info.ECX, 0, 31 )  + 1;
	return (SetSz*WaySz*SectorSz*LnSz);
}


static char *cacheTypes[]={"Null, end of cache_type list", "Data cache", "Instruction cache", "Unified cache", "cache type undefined"};
static char *cacheTypesShort[]={"Null", "L%dD", "L%dI", "L%d", "L%d", "NA"};

/*
 * FindEachCacheIndex
 *
 * Find the subleaf index of CPUID leaf 4 corresponding to the input subleaf
 * Arguments:
 *     maxCPUID   Maximum CPUID Leaf number supported by the processor, provided by parent
 *     cache_subleaf    the cache subleaf encoding recognized by CPIUD instruction leaf 4 for the target cache level
 *
 * Return:        the sub-leaf index corresponding to the largest cache of specified cache type
 *
 */
int FindEachCacheIndex(DWORD maxCPUID, unsigned cache_subleaf)
{	unsigned  i,  type;
	unsigned long cap, lastlevcap;
	CPUIDinfo info4;

	int   target_index = -1;
	
	if(maxCPUID < 4) {
		// if we can't get detailed parameters from cpuid leaf 4, this is stub pointers for older processors
		if(maxCPUID >= 2) {
			if(!cache_subleaf) {
					glbl_ptr->cacheDetail[cache_subleaf].sizeKB = 0;
					sprintf(glbl_ptr->cacheDetail[cache_subleaf].descShort, "$");
					sprintf(glbl_ptr->cacheDetail[cache_subleaf].description, "described by cpuid leaf 2");
					return cache_subleaf;
			}
		}
		return -1;
	}
	
	
	_CPUID(&info4, 4, cache_subleaf);
	type = getBitsFromDWORD(info4.EAX,0,4);
	lastlevcap = cap = GetCacheTotalLize(info4);
	if(type > 0)
	{	
		glbl_ptr->cacheDetail[cache_subleaf].level  = getBitsFromDWORD(info4.EAX,5,7);
		glbl_ptr->cacheDetail[cache_subleaf].type   = type;
		for(i=0; i <= cache_subleaf; i++) {
			if(glbl_ptr->cacheDetail[i].type == type) {
				glbl_ptr->cacheDetail[i].how_many_caches_share_level++;
			}
		}
		glbl_ptr->cacheDetail[cache_subleaf].sizeKB = cap/1024;
		sprintf(glbl_ptr->cacheDetail[cache_subleaf].descShort, cacheTypesShort[type], glbl_ptr->cacheDetail[cache_subleaf].level,
			glbl_ptr->cacheDetail[cache_subleaf].sizeKB);
		sprintf(glbl_ptr->cacheDetail[cache_subleaf].description, "Level %d %s, size(KBytes)= %d",
			glbl_ptr->cacheDetail[cache_subleaf].level, 
			(type < 4 ? cacheTypes[type] : cacheTypes[4]), 
			glbl_ptr->cacheDetail[cache_subleaf].sizeKB);
		lastlevcap = cap;  
		target_index = cache_subleaf;
	}
	return target_index;
}

/*
 * InitStructuredLeafBuffers
 *
 * Allocate buffers to store cpuid leaf data including buffers for subleaves within leaf 4 and 11
 * 
 * Return:        none
 *
 */
void  InitStructuredLeafBuffers()
{
	unsigned j;
	unsigned maxCPUID;
	CPUIDinfo info;

	CPUID(&info,0);
	maxCPUID = info.EAX;
	glbl_ptr->cpuid_values[0].subleaf[0] = (CPUIDinfo *)malloc(sizeof(CPUIDinfo));
	memcpy(glbl_ptr->cpuid_values[0].subleaf[0], &info, 4*sizeof(unsigned int));
	// Mark this combo of cpu, leaf, subleaf is valid
	glbl_ptr->cpuid_values[0].subleaf_max = 1; 

	if(maxCPUID >= MAX_LEAFS) {
		printf("got maxCPUID= 0x%x but the array only handles up to 0x%x. Bye at %s %d\n",
							maxCPUID, MAX_LEAFS, __FILE__, __LINE__);
		exit(2);
	}

	for(j=1; j <= maxCPUID; j++) {
		CPUID(&info,j);
		glbl_ptr->cpuid_values[j].subleaf[0] = (CPUIDinfo *)malloc( sizeof(CPUIDinfo) );
		memcpy(glbl_ptr->cpuid_values[j].subleaf[0], &info, 4*sizeof(unsigned int));
		glbl_ptr->cpuid_values[j].subleaf_max = 1;

		if( (j == 0x4 || j == 0xb)) {
			int subleaf=1;
			unsigned int type=1;
			while (type && subleaf < MAX_CACHE_SUBLEAFS) {
				_CPUID(&info, j, subleaf);
				if(j == 0x4) {
								type = getBitsFromDWORD(info.EAX,0,4);
				} else {
								type = info.EBX;
				} 
				glbl_ptr->cpuid_values[j].subleaf[subleaf] = (CPUIDinfo *)malloc( sizeof(CPUIDinfo) );
				memcpy(glbl_ptr->cpuid_values[j].subleaf[subleaf], &info, 4*sizeof(unsigned int));
							//printf("read cpu= %d leaf= 0x%x subleaf= 0x%x 0x%x 0x%x 0x%x 0x%x at %s %d\n", 
							//	i, j, subleaf, info.EAX, info.EBX, info.ECX, info.EDX, __FILE__, __LINE__);
				subleaf++;
				glbl_ptr->cpuid_values[j].subleaf_max = subleaf;
			}
			if(subleaf == MAX_CACHE_SUBLEAFS) {
							printf("Need bigger MAX_CACHE_SUBLEAFS(%d) at %s %d\n", MAX_CACHE_SUBLEAFS, __FILE__, __LINE__);
							exit(2);
			}
		}
	}

}

/*
 * EachCacheTopologyParams
 *
 * Calculates the select mask that can be used to extract Cache_ID from an APIC ID
 *			the caller must specify which target cache level it wishes to extract Cache_IDs 
 * Arguments:
 *     targ_subleaf       the subleaf index to execute CPIUD instruction leaf 4 for the target cache level, provided by parent
 *     maxCPUID   Maximum CPUID Leaf number supported by the processor, provided by parent
 *
 * Return:        0 is no error
 *
 */
static  int EachCacheTopologyParams(unsigned  targ_subleaf, DWORD maxCPUID)
{	unsigned long SMTMaxCntPerEachCache;
	CPUIDinfo info;

		CPUID(&info,1);
		if (maxCPUID >= 4)
		{
			CPUIDinfo info4;
			_CPUID(&info4,4, targ_subleaf);
			
			SMTMaxCntPerEachCache = getBitsFromDWORD(info4.EAX,14,25)+1; 
		}
		else if( getBitsFromDWORD(info.EDX, 28, 28))
		// no support for CPUID leaf 4 but HT is supported
		{	SMTMaxCntPerEachCache = getBitsFromDWORD(info.EBX, 16, 23);
		}
		else  // no support for CPUID leaf 4 and no HT
		{	SMTMaxCntPerEachCache = 1;
		}
	//printf("got SMTMaxCntPerEachCache[%d]= %d at %s %s %d\n", 
	//		targ_subleaf, SMTMaxCntPerEachCache, __FUNCTION__, __FILE__, __LINE__);
	glbl_ptr->EachCacheSelectMask[targ_subleaf] = createMask(SMTMaxCntPerEachCache,&glbl_ptr->EachCacheMaskWidth[targ_subleaf]);
	return 0;
}

// Calculate parameters used to extract/decompose APIC ID for cache topology
// The algorithm assumes CPUID feature symmetry across all physical packages. 
// Since CPUID reporting by each logical processor in a physical package are identical, we only execute CPUID 
// on one logical processor to derive these system-wide parameters
// return 0 if successful, non-zero if error occurred
static  int CacheTopologyParams()
{
	DWORD maxCPUID;
	CPUIDinfo info;
	int targ_index;
	_CPUID(&info, 0, 0);
	maxCPUID = info.EAX;
	// Let's also examine cache topology.
	// As an example choose the largest unified cache as target level
	if (maxCPUID >= 4) {
		unsigned  subleaf;
		InitStructuredLeafBuffers();
		glbl_ptr->maxCacheSubleaf = 0;
		for(subleaf=0; subleaf < glbl_ptr->cpuid_values[4].subleaf_max; subleaf++) {
			targ_index = FindEachCacheIndex(maxCPUID, subleaf); // unified cache is type 3 under leaf 4
			if( targ_index >= 0 ) {
				glbl_ptr->maxCacheSubleaf = targ_index;
				EachCacheTopologyParams(targ_index, maxCPUID);
			}
			else
			{
				break;
			}
		}
	} else if (maxCPUID >= 2) {
		int subleaf;
		glbl_ptr->maxCacheSubleaf = 0;
		for(subleaf=0; subleaf < 4; subleaf++) {
			targ_index = FindEachCacheIndex(maxCPUID, subleaf); 
			if( targ_index >= 0 ) {
				glbl_ptr->maxCacheSubleaf = targ_index;
				EachCacheTopologyParams(targ_index, maxCPUID);
			}
			else
			{
				break;
			}
		}
	}
	if( glbl_ptr->error)return -1;
	else return 0;

}


// Derive parameters used to extract/decompose APIC ID for CPU topology
// The algorithm assumes CPUID feature symmetry across all physical packages. 
// Since CPUID reporting by each logical processor in a physical package are 
// identical, we only execute CPUID on one logical processor to derive these 
// system-wide parameters
// return 0 if successful, non-zero if error occurred
static  int CPUTopologyParams()
{
	DWORD maxCPUID; // highest CPUID leaf index this processor supports
	CPUIDinfo info; // data structure to store register data reported by CPUID
	
	_CPUID(&info, 0, 0);
	maxCPUID = info.EAX;

	// cpuid leaf B detection
	if (maxCPUID >= 0xB)
	{
		CPUIDinfo CPUInfoB;
		_CPUID(&CPUInfoB,0xB, 0);
		//glbl_ptr points to assortment of global data, workspace, etc
		glbl_ptr->hasLeafB = (CPUInfoB.EBX != 0); 
	}
	_CPUID(&info, 1, 0);

	// Use HWMT feature flag CPUID.01:EDX[28] to treat three configurations:
	if (getBitsFromDWORD(info.EDX,28,28)) 
	{		// #1, Processors that support CPUID leaf 0BH
		if (glbl_ptr->hasLeafB)
		{	// use CPUID leaf B to derive extraction parameters
			CPUTopologyLeafBConstants();
		} 
		else	//#2, Processors that support legacy parameters 
				//  using CPUID leaf 1 and leaf 4
		{
			CPUTopologyLegacyConstants(&info, maxCPUID);
		}
	}
	else	//#3, Prior to HT, there is only one logical processor in a physical package
	{
			glbl_ptr->CoreSelectMask = 0;
			glbl_ptr->SMTMaskWidth = 0;
			glbl_ptr->PkgSelectMask = (unsigned ) (-1);
			glbl_ptr->PkgSelectMaskShift = 0;
			glbl_ptr->SMTSelectMask = 0;
	} 

	if( glbl_ptr->error)return -1;
	else return 0;
}




/*
 * FreeArrays
 *
 * Free the dynamic arrays in glbl_ptr
 *			
 * Arguments: Filename
 *
 * Return:        0 is no error, -1 is error
 */
int FreeArrays(void)
{
	
	free(glbl_ptr->pApicAffOrdMapping);

	free(glbl_ptr->perPkg_detectedCoresCount.data);

	free(glbl_ptr->perCore_detectedThreadsCount.data);

	free(glbl_ptr->perCache_detectedCoreCount.data);

	free(glbl_ptr->perEachCache_detectedThreadCount.data);

	free(glbl_ptr->cpuid_values);

	return 0;
}

/*
 * AllocArrays
 *
 * allocate the dynamic arrays in the glbl_ptr containing various buffers for analyzing
 *	cpu topology and cache topology of a system with N logical processors
 *			
 * Arguments: number of logical processors
 *
 * Return:        0 is no error, -1 is error
 */
int AllocArrays(unsigned  cpus)
{
	unsigned  i;
	
	i = cpus+1;
	glbl_ptr->pApicAffOrdMapping = 
		(IdAffMskOrdMapping *)malloc(i*sizeof(IdAffMskOrdMapping));
	memset(glbl_ptr->pApicAffOrdMapping, 0, i*sizeof(IdAffMskOrdMapping));

	glbl_ptr->perPkg_detectedCoresCount.data = (unsigned  *)malloc(i*sizeof(unsigned ));
	memset(glbl_ptr->perPkg_detectedCoresCount.data, 0, i*sizeof(unsigned ));
	glbl_ptr->perPkg_detectedCoresCount.dim[0] = i;

	glbl_ptr->perCore_detectedThreadsCount.data = (unsigned  *)malloc(MAX_CORES*i*sizeof(unsigned ));
	memset(glbl_ptr->perCore_detectedThreadsCount.data, 0, MAX_CORES*i*sizeof(unsigned ));
	glbl_ptr->perCore_detectedThreadsCount.dim[0] = i;
	glbl_ptr->perCore_detectedThreadsCount.dim[1] = MAX_CORES;

	// workspace for storing hierarchical counts relative to the cache topology 
	// of the largest unified cache (may be shared by several cores)
	glbl_ptr->perCache_detectedCoreCount.data = (unsigned  *)malloc(i*sizeof(unsigned ));
	memset(glbl_ptr->perCache_detectedCoreCount.data, 0, i*sizeof(unsigned ));
	glbl_ptr->perCache_detectedCoreCount.dim[0] = i;

	
	glbl_ptr->perEachCache_detectedThreadCount.data = (unsigned  *)malloc(MAX_CACHE_SUBLEAFS*i*sizeof(unsigned ));
	memset(glbl_ptr->perEachCache_detectedThreadCount.data, 0, MAX_CACHE_SUBLEAFS*i*sizeof(unsigned ));
	glbl_ptr->perEachCache_detectedThreadCount.dim[0] = i;
	glbl_ptr->perEachCache_detectedThreadCount.dim[1] = MAX_CACHE_SUBLEAFS;

	glbl_ptr->cpuid_values     = (CPUIDinfox *)malloc(MAX_LEAFS     *i*sizeof(CPUIDinfox));
	memset(glbl_ptr->cpuid_values, 0, MAX_LEAFS     *i*sizeof(CPUIDinfox));


	return 0;
}


/*
 * ParseIDS4EachThread
 * after execution context has already bound to the target logical processor
 * Query the 32-bit x2APIC ID if the processor supports it, or 
 * Query the 8bit initial APIC ID for older processors
 * Apply various system-wide topology constant to parse the APIC ID into various sub IDs
 * Arguments:
 *		i	:	the ordinal index to reference a logical processor in the system
 * numMappings: running count ot how many processors we've parsed
 * Return:        0 is no error
 */
unsigned ParseIDS4EachThread(unsigned  i, unsigned numMappings)
{	unsigned  APICID;
	unsigned  subleaf;

	APICID = glbl_ptr->pApicAffOrdMapping[numMappings].APICID = GetApicID();
	glbl_ptr->pApicAffOrdMapping[numMappings].OrdIndexOAMsk   = i;  // this an ordinal number that can relate to generic affinitymask
	glbl_ptr->pApicAffOrdMapping[numMappings].pkg_IDAPIC      = ((APICID & glbl_ptr->PkgSelectMask) >> glbl_ptr->PkgSelectMaskShift);
	glbl_ptr->pApicAffOrdMapping[numMappings].Core_IDAPIC     = ((APICID & glbl_ptr->CoreSelectMask) >> glbl_ptr->SMTMaskWidth);
	glbl_ptr->pApicAffOrdMapping[numMappings].SMT_IDAPIC      =  (APICID & glbl_ptr->SMTSelectMask);
	if(glbl_ptr->maxCacheSubleaf != -1) {
		for(subleaf=0; subleaf <= glbl_ptr->maxCacheSubleaf; subleaf++) {
				glbl_ptr->pApicAffOrdMapping[numMappings].EaCacheSMTIDAPIC[subleaf]   =  (APICID & glbl_ptr->EachCacheSelectMask[subleaf]);
				glbl_ptr->pApicAffOrdMapping[numMappings].EaCacheIDAPIC[subleaf]      =  (APICID & (-1 ^ glbl_ptr->EachCacheSelectMask[subleaf]));
			}
	}
	return 0;
}

/*
 * QueryParseSubIDs
 *
 * Use OS specific service to find out how many logical processors can be accessed 
 * by this application.
 * Querying CPUID on each logical processor requires using OS-specific API to 
 * bind current context to each logical processor first. 
 * After gathering the APIC ID's for each logical processor, 
 * we can parse APIC ID into sub IDs for each topological levels
 * The thread affnity API to bind the current context limits us 
 * in dealing with the limit of specific OS
 * The iteration loop to go through each logical processor managed by the OS can be done
 * in a manner that abstract the OS-specific affinity mask data structure. 
 * Here, we construct a generic affinity mask that can handle arbitrary number of logical processors. 
 * Return:        0 is no error
 */
int QueryParseSubIDs(void)
{	unsigned  i;
	//DWORD_PTR processAffinity;
	//DWORD_PTR systemAffinity;
	int numMappings = 0;
	unsigned 	 lcl_OSProcessorCount;
	// we already queried OS how many logical processor it sees.
	lcl_OSProcessorCount = glbl_ptr->OSProcessorCount;
	// we will use our generic affinity bitmap that can be generalized from 
	// OS specific affinity mask constructs or the bitmap representation of an OS
	AllocateGenericAffinityMask(&glbl_ptr->cpu_generic_processAffinity, lcl_OSProcessorCount); 
	AllocateGenericAffinityMask(&glbl_ptr->cpu_generic_systemAffinity, lcl_OSProcessorCount); 
	// Set the affinity bits of our generic affinity bitmap according to
	// the system affinity mask and process affinity mask
	SetChkProcessAffinityConsistency(lcl_OSProcessorCount);
	if (glbl_ptr->error) return -1;

	for (i=0; i < glbl_ptr->OSProcessorCount;i++) {
		// can't asume OS affinity bit mask is contiguous, 
		// but we are using our generic bitmap representation for affinity
		if(TestGenericAffinityBit(&glbl_ptr->cpu_generic_processAffinity, i) == 1) {
			// bind the execution context to the ith logical processor
			// using OS-specifi API
			if( BindContext(i) ) {
				glbl_ptr->error |= _MSGTYP_UNKNOWNERR_OS;
				break;
			}
			// now the execution context is on the i'th cpu, call the parsing routine
			ParseIDS4EachThread(i, numMappings);
			numMappings++;
		}
	}
	glbl_ptr->EnumeratedThreadCount = numMappings;
	if( glbl_ptr->error)return -1;
	else return numMappings;
}


/*
 * AnalyzeCPUHierarchy
 * Analyze the Pkg_ID, Core_ID to derive hierarchical ordinal numbering scheme
 * Arguments:
 *		numMappings:	the number of logical processors successfully queried with SMT_ID, Core_ID, Pkg_ID extracted
 * Return:        0 is no error
 */
static int AnalyzeCPUHierarchy(unsigned  numMappings)
{
	unsigned  i, ckDim, maxPackageDetetcted = 0;
	unsigned  APICID;
	unsigned  packageID, coreID, threadID;
	unsigned  *pDetectCoreIDsperPkg, *pDetectedPkgIDs;
	// allocate workspace to sort parents and siblings in the topology
	// starting from pkg_ID and work our ways down each inner level
	pDetectedPkgIDs = (unsigned  *)_alloca( numMappings * sizeof(unsigned ) ); 
	if(pDetectedPkgIDs == NULL) return -1;
	// we got a 1-D array to store unique Pkg_ID as we sort thru 
	// each logical processor
	memset(pDetectedPkgIDs, 0xff, numMappings*sizeof(unsigned ) );
	ckDim = numMappings * ( 1 << glbl_ptr->PkgSelectMaskShift);
	pDetectCoreIDsperPkg = (unsigned  *)_alloca( ckDim * sizeof(unsigned ) ); 
	if(pDetectCoreIDsperPkg == NULL) return -1;
	// we got a 2-D array to store unique Core_ID within each Pkg_ID,
	// as we sort thru each logical processor
	memset(pDetectCoreIDsperPkg, 0xff, ckDim * sizeof(unsigned ));

	if(numMappings >= glbl_ptr->perCore_detectedThreadsCount.dim[0]) {
		// consistency check on the dimensions of allocated buffer 
		printf("got too large 1st dimension %d which is bigger than %d at %s %d. Bye\n",
			(unsigned ) numMappings, glbl_ptr->perCore_detectedThreadsCount.dim[0], __FILE__, __LINE__);
		exit(2);
	}
	// iterate throught each logical processor in the system.
	// mark up each unique physical package with a zero-based numbering scheme
	// Within each distinct package, mark up distinct cores within that package 
	// with a zero-based numbering scheme
	for (i=0; i < numMappings;i++) {
		BOOL PkgMarked;
		unsigned  h;
		APICID = glbl_ptr->pApicAffOrdMapping[i].APICID;
		packageID = glbl_ptr->pApicAffOrdMapping[i].pkg_IDAPIC ;
		coreID = glbl_ptr->pApicAffOrdMapping[i].Core_IDAPIC ;
		threadID = glbl_ptr->pApicAffOrdMapping[i].SMT_IDAPIC;

		PkgMarked = FALSE;
		for (h=0;h<maxPackageDetetcted;h++)
		{
			if (pDetectedPkgIDs[h] == packageID)
			{
				BOOL foundCore = FALSE;
				unsigned  k;
				PkgMarked = TRUE;
				glbl_ptr->pApicAffOrdMapping[i].packageORD = h;

				if(glbl_ptr->perPkg_detectedCoresCount.data[h] >= glbl_ptr->perCore_detectedThreadsCount.dim[1]) {
					// just a sanity check on the dimensions
					printf("got too large 2nd dimension %d which is bigger than %d at %s %d. Bye\n",
						glbl_ptr->perPkg_detectedCoresCount.data[h], glbl_ptr->perCore_detectedThreadsCount.dim[1], __FILE__, __LINE__);
					exit(2);
				}

				// look for core in marked packages
				for (k=0;k<glbl_ptr->perPkg_detectedCoresCount.data[h];k++)
				{
					if (coreID == pDetectCoreIDsperPkg[h* numMappings +k])
					{
						foundCore = TRUE;
						// add thread - can't be that the thread already exists, breaks uniqe APICID spec
						glbl_ptr->pApicAffOrdMapping[i].coreORD = k;
						glbl_ptr->pApicAffOrdMapping[i].threadORD = glbl_ptr->perCore_detectedThreadsCount.data[h*MAX_CORES+k];
						glbl_ptr->perCore_detectedThreadsCount.data[h*MAX_CORES+k]++;
						break;
					}
				}
				if (!foundCore)
				{	// mark up the Core_ID of an unmarked core in a marked package
					unsigned  core = glbl_ptr->perPkg_detectedCoresCount.data[h];
					if( h* numMappings + core >= ckDim) {
						printf("got error. h* numMappings + core = %d and ckDim= %d at %s %d\n", 
							h* numMappings + core, ckDim, __FILE__, __LINE__);
						exit(2);
					}
					pDetectCoreIDsperPkg[h* numMappings + core] = coreID;
					// keep track of respective hierarchical counts
					glbl_ptr->perCore_detectedThreadsCount.data[h*MAX_CORES+core] = 1;
					glbl_ptr->perPkg_detectedCoresCount.data[h]++;
					// build a set of numbering system to iterate each topological hierarchy
					glbl_ptr->pApicAffOrdMapping[i].coreORD = core;
					glbl_ptr->pApicAffOrdMapping[i].threadORD = 0;
					glbl_ptr->EnumeratedCoreCount++;  // this is an unmarked core, increment system core count by 1
				}
				break;
			}
		}

		if (!PkgMarked)
		{	// mark up the pkg_ID and Core_ID of an unmarked package
			pDetectedPkgIDs[maxPackageDetetcted] = packageID;
			if( maxPackageDetetcted* numMappings + 0 >= ckDim) {
				printf("got error. maxPackageDetetcted= %d numMappings= %d maxPackageDetetcted* numMappings + 0 = %d and ckDim= %d at %s %d\n", 
					maxPackageDetetcted, numMappings, maxPackageDetetcted* numMappings + 0, ckDim, __FILE__, __LINE__);
				exit(2);
			}
			pDetectCoreIDsperPkg[maxPackageDetetcted* numMappings + 0] = coreID;
			// keep track of respective hierarchical counts
			if( maxPackageDetetcted >= glbl_ptr->perPkg_detectedCoresCount.dim[0]) {
				printf("got error. maxPackageDetetcted(%d) >= glbl_ptr->perPkg_detectedCoresCount.dim[0](%d) at %s %d. Bye\n", 
					maxPackageDetetcted, glbl_ptr->perPkg_detectedCoresCount.dim[0], __FILE__, __LINE__);
				exit(2);
			}
			glbl_ptr->perPkg_detectedCoresCount.data[maxPackageDetetcted] = 1;
			glbl_ptr->perCore_detectedThreadsCount.data[maxPackageDetetcted*MAX_CORES+0] = 1;
			// build a set of zero-based numbering acheme so that
			// each logical processor in the same core can be referenced by a zero-based index
			// each core in the same package can be referenced by another zero-based index
			// each package in the system can be referenced by a third zero-based index scheme.
			// each system wide index i can be mapped to a triplet of zero-based hierarchical indices
			glbl_ptr->pApicAffOrdMapping[i].packageORD = maxPackageDetetcted;
			glbl_ptr->pApicAffOrdMapping[i].coreORD = 0;
			glbl_ptr->pApicAffOrdMapping[i].threadORD = 0;
					
			maxPackageDetetcted++; // this is an unmarked pkg, increment pkg count by 1
			glbl_ptr->EnumeratedCoreCount++;  // there is at least one core in a package
		}
	}
	glbl_ptr->EnumeratedPkgCount = maxPackageDetetcted;
	return 0;
}

/*
 * AnalyzeEachCHierarchy
 *   this is an example illustrating cache topology analysis of the largest unified cache
 *   the largest unified cache may be shared by multiple cores
 *			parse APIC ID into sub IDs for each topological levels
 *	This example illustrates several mapping relationships:
 *		1. count distinct target level cache in the system;		2 count distinct cores sharing the same cache
 *		3. Establish a hierarchical numbering scheme (0-based, ordinal number) for each distinct entity within a hierarchical level
 * Arguments:
 *		numMappings:	the number of logical processors successfully queried with SMT_ID, Core_ID, Pkg_ID extracted
 *
 * Return:        0 is no error
 */
static int AnalyzeEachCHierarchy(unsigned subleaf, unsigned  numMappings)
{
	unsigned  i;
	unsigned  maxCacheDetected = 0, maxThreadsDetected = 0;
	unsigned  APICID;
	unsigned  threadID ;
	unsigned  CacheID;
	unsigned  *pDetectThreadIDsperEachC, *pDetectedEachCIDs;
	unsigned  *pThreadIDsperEachC, *pEachCIDs;
	
	if( glbl_ptr->EachCacheMaskWidth[subleaf] == 0xffffffff) return -1;

	pEachCIDs= _alloca( numMappings * sizeof(unsigned ) ); 
	if(pEachCIDs == NULL) return -1;
	memset(pEachCIDs, 0xff, numMappings* sizeof(unsigned ));
	pThreadIDsperEachC = _alloca(    numMappings * sizeof(unsigned )); 
	if(pThreadIDsperEachC == NULL) return -1;
	memset(pThreadIDsperEachC, 0xff, numMappings * sizeof(unsigned ));
	// enumerate distinct caches of the same subleaf index to get counts of how many caches only 
	// mark up each unique cache associated with subleaf index based on the cache_ID
	maxCacheDetected = 0;
	for (i=0; i < numMappings;i++) {
		unsigned  j;
		for (j=0; j < maxCacheDetected; j++) {
			if(pEachCIDs[j] == glbl_ptr->pApicAffOrdMapping[i].EaCacheIDAPIC[subleaf]) {
				break;
			}
		}
		if(j >= maxCacheDetected) {
			pEachCIDs[maxCacheDetected++] = glbl_ptr->pApicAffOrdMapping[i].EaCacheIDAPIC[subleaf];
			//printf("got EaCacheIDAPIC[%d]= 0x%x at %s %d\n", subleaf, glbl_ptr->pApicAffOrdMapping[i].EaCacheIDAPIC[subleaf], __FILE__, __LINE__);
		}
	}
	// enumerate distinct SMT threads within a caches of the subleaf index only without relation to core topology
	// mark up the distinct logical processors sharing a distinct cache level associated subleaf index
	maxThreadsDetected = 0;
	for (i=0; i < numMappings;i++) {
		unsigned  j;
		for (j=0; j < maxThreadsDetected; j++) {
			if(pThreadIDsperEachC[j] == glbl_ptr->pApicAffOrdMapping[i].EaCacheSMTIDAPIC[subleaf]) {
				break;
			}
		}
		if(j >= maxThreadsDetected) {
			pThreadIDsperEachC[maxThreadsDetected++] = glbl_ptr->pApicAffOrdMapping[i].EaCacheSMTIDAPIC[subleaf];
		}
	}

	glbl_ptr->EnumeratedEachCacheCount[subleaf] = maxCacheDetected;


	memset(pEachCIDs, 0xff, numMappings* sizeof(unsigned ));
	memset(pThreadIDsperEachC, 0xff, numMappings * sizeof(unsigned ));

	pDetectedEachCIDs= _alloca( numMappings * sizeof(unsigned ) ); 
	if(pDetectedEachCIDs == NULL) return -1;
	memset(pDetectedEachCIDs, 0xff, numMappings* sizeof(unsigned ));
	pDetectThreadIDsperEachC = _alloca(    numMappings * maxCacheDetected * sizeof(unsigned )); 
	if(pDetectThreadIDsperEachC == NULL) return -1;
	memset(pDetectThreadIDsperEachC, 0xff, numMappings * maxCacheDetected * sizeof(unsigned ));

	// enumerate distinct SMT threads and cores relative to a cache level of the subleaf index 
	// the enumeration below gets the counts and establishes zero-based numbering scheme for cores and SMT threads under each cache
	maxCacheDetected = 0;

	for (i=0; i < numMappings;i++) {
		glbl_ptr->pApicAffOrdMapping[i].EachCacheORD[subleaf] = (unsigned ) -1;
	}

	// check 2nd dim
	if(subleaf >= glbl_ptr->perEachCache_detectedThreadCount.dim[1]) {
		printf("Error: Got subleaf(%d) >= glbl_ptr->perEachCache_detectedThreadCount.dim[1](%d) at %s %d. Bye\n",
			subleaf, glbl_ptr->perEachCache_detectedThreadCount.dim[1], __FILE__, __LINE__);
		exit(2);
	}

	for (i=0; i < numMappings;i++) {
		BOOL CacheMarked;
		unsigned  h;

		APICID = glbl_ptr->pApicAffOrdMapping[i].APICID;
		CacheID = glbl_ptr->pApicAffOrdMapping[i].EaCacheIDAPIC[subleaf] ; // sub ID to enumerate different caches in the system
		threadID = glbl_ptr->pApicAffOrdMapping[i].EaCacheSMTIDAPIC[subleaf] ;

		if(maxCacheDetected >= glbl_ptr->perEachCache_detectedThreadCount.dim[0]) {
			printf("Error: Got maxCacheDetected(%d) >= glbl_ptr->perEachCache_detectedThreadCount.dim[0](%d) at %s %d. Bye\n",
				maxCacheDetected, glbl_ptr->perEachCache_detectedThreadCount.dim[0], __FILE__, __LINE__);
			exit(2);
		}

		CacheMarked = FALSE;
		for (h=0;h<maxCacheDetected;h++)
		{
			//printf("i= %d, h= %d, EachCIDS[%d]= 0x%x, cacheid= 0x%x maxCache= %d\n", 
			//		i, h, h, pDetectedEachCIDs[h], CacheID, maxCacheDetected);
			if (pDetectedEachCIDs[h] == CacheID)
			{	BOOL foundThread= FALSE;
				unsigned  k;

				//printf("got match\n");
				CacheMarked = TRUE;
				glbl_ptr->pApicAffOrdMapping[i].EachCacheORD[subleaf] = h;

				// look for cores sharing the same target cache level
				for (k=0;k<glbl_ptr->perEachCache_detectedThreadCount.data[h*MAX_CACHE_SUBLEAFS+subleaf];k++)
				{
					if (threadID == pDetectThreadIDsperEachC[h* numMappings +k])
					{
						foundThread = TRUE;						
						glbl_ptr->pApicAffOrdMapping[i].threadPerEaCacheORD[subleaf] = k;					
						break;
					}
				}

				if (!foundThread)
				{	// mark up the thread_ID of an unmarked core in a marked package
					unsigned  thread = glbl_ptr->perEachCache_detectedThreadCount.data[h*MAX_CACHE_SUBLEAFS+subleaf];
					pDetectThreadIDsperEachC[h* numMappings + thread] = threadID;
					// keep track of respective hierarchical counts
					glbl_ptr->perEachCache_detectedThreadCount.data[h*MAX_CACHE_SUBLEAFS+subleaf]++;
					// build a set of numbering system to iterate the child hierarchy below the target cache
					glbl_ptr->pApicAffOrdMapping[i].threadPerEaCacheORD[subleaf] = thread;
				}
				break;
			}
		}
		if (!CacheMarked)
		{	// mark up the pkg_ID and Core_ID of an unmarked package
			pDetectedEachCIDs[maxCacheDetected] = CacheID;
			pDetectThreadIDsperEachC[maxCacheDetected* numMappings + 0] = threadID; 
			// keep track of respective hierarchical counts
			glbl_ptr->perEachCache_detectedThreadCount.data[maxCacheDetected*MAX_CACHE_SUBLEAFS+subleaf] = 1;
			// build a set of numbering system to iterate each topological hierarchy
			glbl_ptr->pApicAffOrdMapping[i].EachCacheORD[subleaf] = maxCacheDetected;
			glbl_ptr->pApicAffOrdMapping[i].threadPerEaCacheORD[subleaf] = 0;					
					
			//maxCacheDetected++; // this is an unmarked cache, increment cache count by 1
			maxCacheDetected++; // this is an unmarked cache, increment cache count by 1
		}
	}
	return 0;
}


/*
 * BuildSystemTopologyTables
 *
 * Construct the processor topology tables and values necessary to
 * support the external functions that display CPU topology and/or
 * cache topology derived from system topology enumeration.
 * Arguments:     None
 * Return:        None, sets glbl_ptr->error if tables or values can not be calculated.
 */
static void		BuildSystemTopologyTables()
{	unsigned  lcl_OSProcessorCount, subleaf;
	int 		numMappings = 0 ;
	// call OS-specific service to find out how many logical processors 
	// are supported by the OS
	glbl_ptr->OSProcessorCount = lcl_OSProcessorCount = GetMaxCPUSupportedByOS();

	// allocated the memory buffers within the global pointer
	AllocArrays(lcl_OSProcessorCount);

	// Gather all the system-wide constant parameters needed to derive topology information
	if (CPUTopologyParams() ) return ;
	if (CacheTopologyParams() ) return ;

	// For each logical processor, collect APIC ID and parse sub IDs for each APIC ID
	numMappings = QueryParseSubIDs();
	if ( numMappings < 0 ) return ;
	// Derived separate numbering schemes for each level of the cpu topology
	if( AnalyzeCPUHierarchy(numMappings) < 0 ) {
		glbl_ptr->error |= _MSGTYP_TOPOLOGY_NOTANALYZED;
	}
	// an example of building cache topology info for each cache level
	if(glbl_ptr->maxCacheSubleaf != -1) {
		for(subleaf=0; subleaf <= glbl_ptr->maxCacheSubleaf; subleaf++) {
			if( glbl_ptr->EachCacheMaskWidth[subleaf] != 0xffffffff) {   
				// ensure there is at least one core in the target level cache
				if (AnalyzeEachCHierarchy(subleaf, numMappings) < 0) {
					glbl_ptr->error |= _MSGTYP_TOPOLOGY_NOTANALYZED;
				}
			}
		}
	}
}

/*
 * LeafBHWMTConsts
 *
 * Query CPUID leaf B to report the processor's hardware multithreading capabilities
 *
 * Arguments:     None
 *     
 * Return:        None, sets glbl_ptr->error if tables or values can not be calculated.
 */
int LeafBHWMTConsts()
{
CPUIDinfo infoB;
int subLeaf = 0;
	do
	{
		int levelType;
		int ThreadCnt;
			
		_CPUID(&infoB,0xB,subLeaf);
		if (infoB.EBX == 0)  
		{	// we already tested leaf B provide valid info, if EBX[15:0] of this subleaf is not valid, we can exit the loop
			break;
		}

		levelType = getBitsFromDWORD(infoB.ECX,8,15);
		ThreadCnt = getBitsFromDWORD(infoB.EBX,0,16);
		switch (levelType) 
		{
		case 1:		//level type is SMT, so ThreadCnt is the # of logical processor within the parent level, i.e. a core
			
			glbl_ptr->HWMT_SMTperCore = ThreadCnt;
			break;
		case 2:	//level type is Core, so ThreadCnt is the # of logical processor within the parent level, i.e. a package		
			glbl_ptr->HWMT_SMTperPkg =  ThreadCnt;
			break;
		default:
			// handle in the future
			break;
		}				
		subLeaf++;

	} while (1);
	if( !glbl_ptr->HWMT_SMTperCore || !glbl_ptr->HWMT_SMTperPkg) return -1;
	return 0;
}

/*
 * LegacyHWMTConsts
 *
 * Query CPUID leaf 1 and leaf 4 to report the processor's hardware multithreading capabilities
 *
 * Arguments:
 *     info       Point to strucrture to pass in CPIUD instruction leaf 1 data provided by parent
 *     maxCPUID   Maximum CPUID Leaf number supported by the processor, provided by parent
 *
 * Return:        None, sets glbl_ptr->error if tables or values can not be calculated.
 */
int LegacyHWMTConsts(CPUIDinfo * pinfo, DWORD maxCPUID)
{
unsigned long	coreIDMaxCnt ;
//unsigned long	SMTIDPerCoreMaxCnt ;
	// CPUID.1:EBX[23:16] provides the max # IDs that can be enumerated under the CorePlusSMT_SelectMask
	// for older legacy processors that don't support leaf B, it is also the max # of logical processors in a package
		glbl_ptr->HWMT_SMTperPkg = getBitsFromDWORD(pinfo->EBX,16,23);
		// support CPUID 4?
		if (maxCPUID >= 4)
		{
			CPUIDinfo info4;
			CPUID(&info4,4);
			// (CPUID.(EAX=4, ECX=00:EAX[31:26] +1 ) provides the max # Core_IDs that can exist in a package 
			// for older legacy processors that don't support leaf B, this is also the max # of cores in a package
			coreIDMaxCnt = getBitsFromDWORD(info4.EAX,26,31)+1; 
			glbl_ptr->HWMT_SMTperCore = glbl_ptr->HWMT_SMTperPkg / coreIDMaxCnt;
		}
		else if (getBitsFromDWORD(pinfo->EDX,28,28)) 
		// no support for CPUID leaf 4 but HT supported
		{	if (!glbl_ptr->Alert_BiosCPUIDmaxLimitSetting) {
				glbl_ptr->HWMT_SMTperCore = glbl_ptr->HWMT_SMTperPkg ;
			}
			else {	// we got here most likely because IA32_MISC_ENABLES[22] was set to 1 by BIOS
				glbl_ptr->error |= _MSGTYP_CHECKBIOS_CPUIDMAXSETTING;  // IA32_MISC_ENABLES[22] may have been set to 1, will cause inaccurate reporting
			}
		}
		else 
		{
			glbl_ptr->HWMT_SMTperCore = glbl_ptr->HWMT_SMTperPkg  = 1;
		}
	return 0;
}


/*
 * GetHWMTConfig
 *
 * Bind the current context to a logical processor in a specified physical package, then query the 
 *		hardware multithreading capabilities using CPUID
 * Arguments:
 *     pkg       an ordinal number to specify a physical package in the system
 *
 * Return:        None, sets glbl_ptr->error if tables or values can not be calculated.
 */
int GetHWMTConfig(unsigned  pkg)
{
	DWORD maxCPUID;
	CPUIDinfo info;
	unsigned int which_cpu;
	// pick out a logical processor in the specified package
	which_cpu  = SlectOrdfromPkg(pkg,ENUM_ALL,ENUM_ALL );
	if (BindContext(which_cpu ) ) {
		glbl_ptr->error |= _MSGTYP_UNKNOWNERR_OS;
		return -1;
	}
	CPUID(&info,0);
	maxCPUID = info.EAX;

	// cpuid leaf B detection
	if (maxCPUID >= 0xB)
	{
		CPUIDinfo CPUInfoB;
		CPUID(&CPUInfoB,0xB);
		glbl_ptr->hasLeafB = (CPUInfoB.EBX != 0);
	}

	if (glbl_ptr->hasLeafB)
	{	// use CPUID leaf B to derive extraction parameters
		LeafBHWMTConsts();
	}
	else	 //legacy parameters use CPUID leaf 1 and leaf 4
	{
		CPUID(&info, 1); 
		LegacyHWMTConsts(&info, maxCPUID);
	}

	if( glbl_ptr->error)return -1;
	else return 0;
}


/*
 * InitCpuTopology
 *
 * Initialize the CPU topology structures if they have not already been initialized
 *
 * Arguments:     None
 *
 * Return:        None
 */
void InitCpuTopology() 
{
	if (!glbl_ptr->EnumeratedPkgCount)
		BuildSystemTopologyTables();
}


/*
 * getEnumerateAPICID
 *
 * Returns APIC ID for the specified processor from enumerated table
 *
 * Arguments:
 *     Processor  Os processor number of for which the the APIC ID is returned
 *
 * Return:        APIC ID for the specified processor
 */
unsigned  getEnumerateAPICID(unsigned  processor)
{
	if (!glbl_ptr->EnumeratedPkgCount)
	{
		InitCpuTopology();
	}
	if (glbl_ptr->error)		return 0xffffffff;
	
	if (processor >= glbl_ptr->OSProcessorCount)
		return 0xffffffff;		// allow caller to intercept error

	return glbl_ptr->pApicAffOrdMapping[processor].APICID;
}


/*
 * GetEnumeratedCoreCount
 *
 * Returns numbers of cores active on a given package, based on enumerated result
 *
 * Arguments:     package ordinal
 *
 * Return:        number of cores active on specified package, 0 if can not calulate
 */
unsigned  GetEnumeratedCoreCount(unsigned  package_ordinal)
{
	if (!glbl_ptr->EnumeratedPkgCount)		InitCpuTopology();
	
	if (glbl_ptr->error || package_ordinal >= glbl_ptr->EnumeratedPkgCount)		return 0;
	
	return glbl_ptr->perPkg_detectedCoresCount.data[package_ordinal];
}

/*
 * GetEnumeratedThreadCount
 *
 * Returns numbers of Threads active on a given package/core
 *
 * Arguments:     package ordinal and core ordinal
 *
 * Return:        number of threads active on specified package and core, 0 if can not calulate
 */
unsigned  GetEnumeratedThreadCount(unsigned  package_ordinal, unsigned  core_ordinal)
{
	if (!glbl_ptr->EnumeratedPkgCount)
	{
		InitCpuTopology();
	}
	if (glbl_ptr->error || package_ordinal >= glbl_ptr->EnumeratedPkgCount)
	{
		return 0;
	}
	if (core_ordinal >= glbl_ptr->perPkg_detectedCoresCount.data[package_ordinal])
	{
		return 0;
	}
	return glbl_ptr->perCore_detectedThreadsCount.data[package_ordinal*MAX_CORES+core_ordinal];
}


/*
 * GetSysEachCacheCount
 *
 * Returns count of distinct target level cache in the system
 *
 * Arguments:     None
 *
 * Return:        Number of caches or 0 if number can not be calculated
 */

unsigned  GetSysEachCacheCount(unsigned  subleaf)
{
	if (!glbl_ptr->EnumeratedEachCacheCount[subleaf])		InitCpuTopology();
	
	if (glbl_ptr->error) return 0;
	
	return glbl_ptr->EnumeratedEachCacheCount[subleaf];
}



/*
 * GetOSLogicalProcessorCount
 *
 * Returns count of logical processors in the system as seen by OS
 *
 * Arguments:     None
 *
 * Return:        Number of logical processors or 0 if number can not be calculated
 */
extern unsigned  GetOSLogicalProcessorCount();
unsigned  GetOSLogicalProcessorCount()
{
	if (!glbl_ptr->OSProcessorCount)
	{
		InitCpuTopology();
	}
	if (glbl_ptr->error )	return 0;
	
	return glbl_ptr->OSProcessorCount;
}

/*
 * GetSysLogicalProcessorCount
 *
 * Returns count of logical processors in the system that were enumerated by this app
 *
 * Arguments:     None
 *
 * Return:        Number of logical processors or 0 if number can not be calculated
 */
extern unsigned  GetSysLogicalProcessorCount();
unsigned  GetSysLogicalProcessorCount()
{
	if (!glbl_ptr->OSProcessorCount)
	{
		InitCpuTopology();
	}
	if (glbl_ptr->error )	return 0;
	
	return glbl_ptr->EnumeratedThreadCount;
}

/*
 * GetSysProcessorCoreCount
 *
 * Returns count of processor cores in the system that were enumerated by this app
 *
 * Arguments:     None
 *
 * Return:        Number of physical processors or 0 if number can not be calculated
 */
extern unsigned  GetSysProcessorCoreCount();
unsigned  GetSysProcessorCoreCount()
{
	if (!glbl_ptr->EnumeratedCoreCount)		InitCpuTopology();
	
	if (glbl_ptr->error) return 0;
	
	return glbl_ptr->EnumeratedCoreCount;
}

/*
 * GetSysProcessorPackageCount
 *
 * Returns count of processor packages in the system that were enumerated by this app
 *
 * Arguments:     None
 *
 * Return:        Number of processor packages in the system or 0 if number can not be calculated
 */
extern unsigned  GetSysProcessorPackageCount();
unsigned  GetSysProcessorPackageCount()
{
	if (!glbl_ptr->EnumeratedPkgCount)
	{
		InitCpuTopology();
	}
	if (glbl_ptr->error )	return 0;
	
	return glbl_ptr->EnumeratedPkgCount;
}


/*
 * GetCoreCountPerEachCache
 *
 * Returns numbers of cores active sharing the target level cache
 *
 * Arguments:     Cache ordinal
 *
 * Return:        number of cores active on specified target level cache, 0 if can not calulate
 */
unsigned  GetCoreCountPerEachCache(unsigned  subleaf, unsigned  cache_ordinal)
{
	if (!glbl_ptr->EnumeratedEachCacheCount[subleaf])		InitCpuTopology();
	
	if (glbl_ptr->error || cache_ordinal >= glbl_ptr->EnumeratedEachCacheCount[subleaf])		return 0;
	
	return glbl_ptr->perEachCache_detectedThreadCount.data[cache_ordinal*MAX_CACHE_SUBLEAFS+subleaf];
}


// The utility function below are shown for tutorial purpose only

#ifdef BUILD_MAIN
static BOOL boxes_side_by_side = TRUE;


/*
 * DumpCPUIDArray
 *
 * Write cpuid array to file.
 *			
 * Arguments: Filename
 *
 * Return:        0 is no error, -1 is error
 */
int DumpCPUIDArray(char *filename)
{	
	//DWORD_PTR processAffinity;
	//DWORD_PTR systemAffinity;
	char long_str[1024], cres;
	unsigned  i;
	FILE *fd;
	
	if(strcmp(filename, "stdout") == 0) {
		fd = stdout;
	} else {
		fd = fopen(filename, "w");
		if(fd == NULL) {
			printf("failed to open file= %s \n", filename);
			exit(2);
		}
	}

	fprintf(fd, "OSProcessorCount\t%d\n", glbl_ptr->OSProcessorCount);
	if(sizeof(char *) == 4) {
		fprintf(fd, "ptr_size\t4\n");
	} else {
		fprintf(fd, "ptr_size\t%d\n", (int ) sizeof(char *));
	}
	MaskToHexStringGenericAffinityMask(&glbl_ptr->cpu_generic_processAffinity, sizeof(long_str), long_str);
	fprintf(fd, "processAffinity\t%s\n", long_str);
	MaskToHexStringGenericAffinityMask(&glbl_ptr->cpu_generic_systemAffinity, sizeof(long_str), long_str);
	fprintf(fd, "systemAffinity\t%s\n", long_str);
	
	fprintf(fd, "BeginCPUID\n");

	for (i=0; i < glbl_ptr->OSProcessorCount; i++) {

		// can't asume affinity bit mask is continuous....
		if( (cres = TestGenericAffinityBit(&glbl_ptr->cpu_generic_processAffinity, i)) == 1) {
			// Remove current processor from the mask
			unsigned  maxCPUID, j;
			CPUIDinfo info;
			if( !BindContext(i) ) 
			{	CPUID(&info, 0);
				maxCPUID = info.EAX;
				fprintf(fd, "%d\t0\t0\t%x %x %x %x\n", i, info.EAX, info.EBX, info.ECX, info.EDX);

				for(j=1; j <= maxCPUID; j++)
				{
					unsigned subleaf;
					CPUID(&info, j);
					fprintf(fd, "%d\t%x\t0\t%x %x %x %x\n", i, j, info.EAX, info.EBX, info.ECX, info.EDX);
					if(glbl_ptr->cpuid_values[i*MAX_LEAFS+j].subleaf_max > 1) {
						for(subleaf=1; subleaf < glbl_ptr->cpuid_values[i*MAX_LEAFS+j].subleaf_max; subleaf++) {
							_CPUID(&info, j, subleaf);
							fprintf(fd, "%d\t%x\t%x\t%x %x %x %x\n", i, j, subleaf, info.EAX, info.EBX, info.ECX, info.EDX);
						}
					}
				}

				CPUID(&info, 0x80000000);
				maxCPUID = info.EAX;
				fprintf(fd, "%d\t80000000\t0\t%x %x %x %x\n", i, info.EAX, info.EBX, info.ECX, info.EDX);

				for(j=0x80000000+1; j <= maxCPUID; j++)
				{
					
					CPUID(&info, j);
					fprintf(fd, "%d\t%x\t0\t%x %x %x %x\n", i, j, info.EAX, info.EBX, info.ECX, info.EDX);
				}
			}			
		}
		else if (cres == 0xff){
			glbl_ptr->error |= _MSGTYP_UNK_AFFINTY_OPERATION;
		}
	}
	if(strcmp(filename, "stdout") != 0) {
		fclose(fd);
	}
	if( glbl_ptr->error)return -1;
	else return 0;
}



/*
 * getPkgCoreThrdStr
 *
 * Returns various strings to be printed in 'box' 
 *
 * Arguments:
 *     which_mapping  an index into the glbl_ptr->pApicAffOrdMapping array
 *     line_type      which type of string to generate
 *     subleaf_in     which subleaf do you want
 *     which_entry    which box we are in the current level
 *     AffinityMask   the combined affinityMask of all the threads in the current cache
 *
 * Return:        formatted character string 
 */

char *getPkgCoreThrdStr(int which_mapping, int line_type, int subleaf_in, int which_entry, GenericAffinityMask *AffinityMask) {
	static char spacer[2], str[64], str2[64];
	char long_str[256];
	static unsigned first_time= TRUE, hasSMT=FALSE, max_len=0;
	unsigned  len, subleaf, j;

	spacer[0] = ' ';
	spacer[0] = 0;
	spacer[1] = 0;
	if(first_time == TRUE || which_mapping == -1) {
		// the 'which_mapping == -1' logic lets us reset the static values if we need to.
		// Maybe we are going to have more than system. 
		
		first_time = FALSE;
		for (j = 0; j < GetSysLogicalProcessorCount(); j++)	{
			if(glbl_ptr->pApicAffOrdMapping[j].threadORD > 0) {
				hasSMT = TRUE;
				break;
			}
		}
		for (j = 0; j < GetSysLogicalProcessorCount(); j++)	{
			if(hasSMT == TRUE) {
				// don't really need to put the package id in since I print it out at the top
				sprintf(str, "%sc%d_t%d", spacer, glbl_ptr->pApicAffOrdMapping[j].coreORD, glbl_ptr->pApicAffOrdMapping[j].threadORD);
			} else {
				sprintf(str, "%sc%d", spacer, glbl_ptr->pApicAffOrdMapping[j].coreORD);
			}
			len = strlen(str);
			if(len > max_len) {
				max_len = len;
			}
			sprintf(str, "%s%x", spacer, (LNX_MY1CON << glbl_ptr->pApicAffOrdMapping[j].OrdIndexOAMsk));
			len = strlen(str);
			if(len > max_len) {
				max_len = len;
			}
			sprintf(str, "%s%d", spacer, j); // OS cpu #
			len = strlen(str);
			if(len > max_len) {
				max_len = len;
			}
		}
		for(subleaf=0; subleaf <= glbl_ptr->maxCacheSubleaf; subleaf++) {
			len = strlen(glbl_ptr->cacheDetail[subleaf].descShort);
			if(len > max_len) {
				max_len = len;
			}
		}
		if(max_len == 3) {
			max_len++;
			spacer[0] = ' ';
			spacer[1] = 0;
		}
		if(max_len >= sizeof(str)) {
			printf("Error: got maxlen(%d) longer than sizeof(str)= %d at %s %d\n", 
				max_len, (int) sizeof(str), __FILE__, __LINE__);
			exit(2);
		}
		if(which_mapping == -1) {
			str[0] = 0;
			return str;
		}
	}

	if(line_type == 1) {
		if(which_entry == 0) {
			sprintf(str, "%s%s", spacer, glbl_ptr->cacheDetail[subleaf_in].descShort);
		} else {
			str[0] = 0;
		}
	} else if(line_type == 2) {
		if(which_entry == 0) {
			int sz, KorM;
			char *unitKM[]= {"K", "M"};
			sz = glbl_ptr->cacheDetail[subleaf_in].sizeKB;
			if(sz >= 1024) {
				sz /= 1024;
				KorM = 1;
			} else {
				KorM = 0;
			}
			sprintf(str, "%s%d%s", spacer, sz, unitKM[KorM]);
		} else {
			str[0] = 0;
		}
	} else if(line_type == 3) {
		sprintf(str, "%s%d", spacer, which_mapping);
	} else if(line_type == 4) {
		if(hasSMT == TRUE) {
			sprintf(str, "%sc%d_t%d", spacer, glbl_ptr->pApicAffOrdMapping[which_mapping].coreORD, 
					glbl_ptr->pApicAffOrdMapping[which_mapping].threadORD);
		} else {
			sprintf(str, "%sc%d", spacer, glbl_ptr->pApicAffOrdMapping[which_mapping].coreORD);
		}
	} else if(line_type == 5) {
		FormatSingleBitMask(glbl_ptr->pApicAffOrdMapping[which_mapping].OrdIndexOAMsk, sizeof(long_str), long_str);
		sprintf(str, "%s%s", spacer, long_str);
	} else if(line_type == 6) {
		if(which_entry == 0) {
			//cccc
			MaskToHexStringGenericAffinityMask(AffinityMask, sizeof(long_str), long_str);
			sprintf(str, "%s%s", spacer, long_str);
		} else {
			str[0] = 0;
		}
	} else {
		for(j=0; j < max_len; j++) {
			str[j] = '-';
		}
		str[max_len] = 0;
	}
	if(strlen(str) > sizeof(str)) {
		printf("Error: got strlen(%d) longer than sizeof(str)= %d at %s %d\n", 
			(int) strlen(str), (int) sizeof(str), __FILE__, __LINE__);
		exit(2);
	}
	sprintf(str2, "%*s", max_len, str);

	return str2;
}



/* MaskToHexStringGenericAffinityMask
 *  prints the affinity mask in hex (print 2 hex bytes per 1 byte of affinity mask.
 *  Have to reverse the string so that the least signficant bit (representing cpu 0) 
 *  is the rightmost bit.
 *  Also the
 *
 * args: 
 *   generic affinty mask ptr
 *   len: length of string to receive the output
 *   char *str: pointer to char string into which the hex string will be put
 *
 * returns -1 if the string is too short to hold the output, otherwise returns the number of bytes in the string
 *         
 */
int MaskToHexStringGenericAffinityMask(GenericAffinityMask *pAffinityMap, unsigned  len, char *str)
{
	unsigned   j;
	long i;
	j=0;
	str[j] = 0;

	for(i = (long) (pAffinityMap->maxByteLength -1) ; i >= 0; i--) {
		if((pAffinityMap->AffinityMask[i] != 0 || j > 0) && j < len) {
			sprintf(str+j, "%.2x", pAffinityMap->AffinityMask[i]);
			j += 2;
		}
	}
	if(j < len) {
		str[j] = 0;
	} else {
		return -1;
	}
	// let's compress the hex representation of generic bitmask if there
	// are leading zero or trailing zero
	j = CompressHexMask( str);
	return j;
}

DynCharBuf_str	sv_printBuffer;
DynCharBuf_str	printBuffer[MAX_PACKAGES];
#define BOX_SIZE 4096


void	PrintBoxDiagramNotes(DWORD pkg)
{
	unsigned  sub_lf;
	printf("\n\nBox Description:\n");
	printf("Cache  is cache level designator\n");
	printf("Size   is cache size\n");
	printf("OScpu# is cpu # as seen by OS\n");
	printf("Core   is core#[_thread# if > 1 thread/core] inside socket\n");
	printf("AffMsk is AffinityMask(extended hex) for core and thread\n");
	printf("CmbMsk is Combined AffinityMask(extended hex) for hw threads sharing cache\n");
	printf("       CmbMsk will differ from AffMsk if > 1 hw_thread/cache\n");
	printf("Extended Hex replaces trailing zeroes with 'z#'\n");
	printf("       where # is number of zeroes (so '8z5' is '0x800000')\n");
	
	for(sub_lf=0; sub_lf <= (unsigned ) glbl_ptr->maxCacheSubleaf; sub_lf++) {
		// print a desc of this cache 
		if(boxes_side_by_side == FALSE || (boxes_side_by_side == TRUE && pkg==0)) {
			printf("%s is %s,  Cores/cache= %d, Caches/package= %d\n", 
			glbl_ptr->cacheDetail[sub_lf].descShort, 
			glbl_ptr->cacheDetail[sub_lf].description,
			GetCoreCountPerEachCache(sub_lf, 0), 
			GetSysEachCacheCount(sub_lf)/GetSysProcessorPackageCount());
		} 
	}
	if(boxes_side_by_side == TRUE && GetSysProcessorPackageCount() > 1) {
		//memcpy(&sv_printBuffer, &glbl_ptr->printBuffer, sizeof(DynCharBuf_str));
		for(pkg=0; pkg < GetSysProcessorPackageCount(); pkg++) {
			printBuffer[pkg].buffer = (char *)malloc(BOX_SIZE*sizeof(char));
			memset(printBuffer[pkg].buffer, 0, BOX_SIZE*sizeof(char));
			printBuffer[pkg].used = 0;
			printBuffer[pkg].size = BOX_SIZE;
		}
	}					

}

/*
 * ListVisibleCountsByEachCacheTopologyByPkgBox
 *
 * This is a utility that presents topology information (cache hierarchy, core, SMT) of 
 *	the specified physical package in a somewhat visually intuitive manner (box-diagrams)
 *	The core/SMT topology is presented for each level of the cache hierarchy
 *  For each level of the cache hierarchy, this routine presents the following 5 type of information:
 *		Cache level: L1 Data cache (L1D), L1 instruction cache (L1I), unified L2 (UL2), unified L3 (UL3) if present
 *		HW Threads sharing this cache level: the logical processors sharing this cache level is listed via ordinal numbered scheme
 *		Cores sharing this cache level: the cores sharing this cache level is listed via ordinal numbered scheme, each ordinal core index is aligned vertically with the hw thread listing to illustrate SMT threads sharing a core
 *		affinity mask: the generic affinity mask associated with each logical processor is listed using compressed hex representation
 *		Combined affinity mask: for all logical processors sharing this package, using compressed hex representation
 *		The distinct entities within each topological level is denoted by a separator in the form of a vertical bar
 *		Each type of information is printed to separate line of alpha-numeric symbol/numbers. 
 *		5 type of topological information + heading line => visual representation consists of six linetypes
 *
 * Arguments:
 *     which_packege  package ordinal number for which you want to print the cache and thread layout
 *
 * Return:        formatted character string 
 */

void ListVisibleCountsByEachCacheTopologyByPkgBox(DWORD pkg)
{
	unsigned  i, j, did_header, cur_entry, max_entries, subleaf, level, level_min, level_max;
	unsigned line_type, line_type_max, sub_lf; 
	GenericAffinityMask AffinityMask;
	GenericAffinityMask TotAffinityMask;
	char *line_type_sep[]={"+", "|"};
	//char *header_desc6[]={"      ", "Cache ", "OScpu#", "Core  ","AffMsk", "CmbMsk"};
	char *header_desc7[]={"      ", "Cache ", "Size  ", "OScpu#", "Core  ","AffMsk", "CmbMsk"};

	AllocateGenericAffinityMask(&AffinityMask, glbl_ptr->OSProcessorCount);
	AllocateGenericAffinityMask(&TotAffinityMask, glbl_ptr->OSProcessorCount);
	// for each package, print the cache and thread info by cache levels. 
	// First find the min and max levels
	level_min = glbl_ptr->cacheDetail[0].level;
	level_max = glbl_ptr->cacheDetail[0].level;
	if(glbl_ptr->maxCacheSubleaf != -1) {
		for(subleaf=1; subleaf <= glbl_ptr->maxCacheSubleaf; subleaf++) {
			if(glbl_ptr->cacheDetail[subleaf].level < level_min) {
			level_min = glbl_ptr->cacheDetail[subleaf].level;
			}
			if(glbl_ptr->cacheDetail[subleaf].level > level_max) {
			level_max = glbl_ptr->cacheDetail[subleaf].level;
			}
		}
	}

	// for each cache level, go over all the caches and print the the details
	line_type_max = 7;
	for(sub_lf=0; sub_lf <= glbl_ptr->maxCacheSubleaf; sub_lf++) {
		level = glbl_ptr->cacheDetail[sub_lf].level;

		// TotAffinity will be the OR of all the threads at this level
		ClearGenericAffinityMask(&TotAffinityMask);
		for(line_type=0; line_type <= line_type_max; line_type++) {
			int use_sep = 1;
			if(line_type == 0 || line_type == line_type_max) {
				// use_sep=0 means we want to draw a horizontal line
				use_sep = 0;
			}
			for(subleaf=0; subleaf <= glbl_ptr->maxCacheSubleaf; subleaf++) {
				// search all the subleafs for any at this level
				int cache_desc_not_available = strcmp(glbl_ptr->cacheDetail[subleaf].descShort, "$");
				did_header= FALSE;
				for (i = 0; i < GetSysEachCacheCount(subleaf); i ++){
					if(subleaf != sub_lf) {
						continue;
					}
					if ( GetCoreCountPerEachCache(subleaf, i) > 0) {
						// got any threads? Can't see how this would be 0 but... 
						max_entries= 0;
						ClearGenericAffinityMask(&AffinityMask);
						for (j = 0; j < GetSysLogicalProcessorCount(); j++)	{
							// search over all the threads
							if( glbl_ptr->pApicAffOrdMapping[j].packageORD == pkg  &&
								glbl_ptr->pApicAffOrdMapping[j].EachCacheORD[subleaf] == i) {
								// count how many threads at this level
								max_entries++;
								// AffinityMask is OR of all threads in this cache
								SetGenericAffinityBit(&AffinityMask, glbl_ptr->pApicAffOrdMapping[j].OrdIndexOAMsk);
								// TotAffinityMask is OR of all threads in this level
								SetGenericAffinityBit(&TotAffinityMask, glbl_ptr->pApicAffOrdMapping[j].OrdIndexOAMsk);
							}
						}
						cur_entry= 0;
						for (j = 0; j < GetSysLogicalProcessorCount(); j++)	{
							//bbbb
							if( glbl_ptr->pApicAffOrdMapping[j].packageORD == pkg  &&
								glbl_ptr->pApicAffOrdMapping[j].EachCacheORD[subleaf] == i) {
								
									// print 1st five attributes for subleaf = 0;
								if( (subleaf == 0 && line_type == 6 && GetCoreCountPerEachCache(sub_lf, 0) == 1) || 
									//  we want cache level designator and its size printed
									//for every level if we have that info, so line_type 2 and 3 are printed for each level
									// if we can query them using enumerated cpuid leaf 4 data. 
									// we only print the sixth attribute (cmbMsk) for the last level cache
									(subleaf > 0 &&  ( line_type == 0 || (line_type >= 3 && line_type <= 5) ||
									(line_type == 6 && GetCoreCountPerEachCache(sub_lf-1, 0) == GetCoreCountPerEachCache(sub_lf, 0)) )) ||
									// if cpuid.4 is not available, we skip printing cache designator and size info
									( !cache_desc_not_available  && (line_type == 1 || line_type == 2 ) )
									) 
								{
									;
								} else {
									if(did_header == FALSE) {   // if the first time at this level
										did_header = TRUE;
										{	// if first time
											if(use_sep == 0) {
												printf("%s", header_desc7[0]);
											} else {
												printf("%s", header_desc7[line_type]);
											} 
										} 
									}
									if(cur_entry == 0) {
										printf("%s", line_type_sep[use_sep]);  // print the left hand side of the box
									} else if(use_sep == 0) {
										printf("-"); // if doing a line of '-' but we have more than 1 thread/cache
									} else  {
										printf(" "); // if more than one thread in the cache separate text with ' '
									}
									// print the text (or the '-'s if doing a line)
									printf("%s", getPkgCoreThrdStr(j, line_type, subleaf, cur_entry, &AffinityMask));
								}
								cur_entry++; // count how many 
							}
						}
					}
				}
				if(did_header == TRUE) {
					// so we printed something
					int need_nl= TRUE;
					// check whether we need a \n. I want the L1i to on the same level as the L1d so 
					// don't put a newline after L1D 
					if(need_nl == TRUE) {
						printf("%s\n", line_type_sep[use_sep]);
					} else {
						printf("%s ", line_type_sep[use_sep]);
					} 
				}
			}
		}
		printf("\n");
		if(level == level_max && CompareEqualGenericAffinity(&TotAffinityMask, &AffinityMask) != 0) {
			char long_str[1024];
			// so we are on the last level of caches and there are 2 or more distinct caches (ala merom/conroe/clovertown/etc)
			// This printf gives the combined AffinityMask for all the threads on the socket.
			MaskToHexStringGenericAffinityMask(&TotAffinityMask, sizeof(long_str), long_str);
			printf("Combined socket AffinityMask= 0x%s\n", long_str);
		}
	}
	FreeGenericAffinityMask(&AffinityMask);
	FreeGenericAffinityMask(&TotAffinityMask);
}





/*
 * GetGenericAffinityMask
 *
 * Report the "affinity mask" for the specified logical proccesor(s) using   
 *	an ordinal (zero-based) numbering scheme to specify each level of the CPU hierarchy.
 *	"Affinity mask" is a data structure software can acces using windows API, 
 *	Linux implements an internal bitmap that is conceptually equivalent to Windows's affinity mask 
 *  To relate an affinity mask to Linux API, extract the position of each non-zero bit
 *	and use _CPU_SET macro to update the internal bitmap corresponding to affinity mask 

 *  A generalized affinity data structure is defined here so we can theoretically 
 *	handle an OS that support more than 64 logical processors.
 *	The bits that are set in a generic affinity are transformed using bex 
 *	representation ina string buffer of specified length. 
 *	We use a compressed hex string representation that wll strip
 *	leading zero and compress trailing zero to conserve string length of hex string
 *
 * Arguments:     
 *		package   Specify a package using zero-based numbering scheme or ENUM_ALL for every package
 *      core	  Specify a core in the selected package(s) using zero-based numbering scheme or ENUM_ALL for every package
 *      logical   Specify a logical processor in the selected core(s) using zero-based numbering scheme or ENUM_ALL for every package
 * ENUM_ALL in any of the arguments means to provide a mask for all processor at that level;
 *		slen	  Specify the max length of the string buffer to receive compressed hex represention of the corresponding generic affinity mask
 *		sbuf	  Specify pointer to the string buffer.
 *
 * Return:        0: encounter error; 1 if successful
 */
	
int GetGenericAffinityMask(unsigned  package,unsigned  core, unsigned  logical, unsigned int slen, char * sbuf)
{
	
	GenericAffinityMask genericAffinity;
	unsigned int i;
	memset(sbuf, 0, slen);
	if( AllocateGenericAffinityMask(&genericAffinity, GetOSLogicalProcessorCount())) return 0;

	if (package != ENUM_ALL && package >= GetSysProcessorPackageCount())		return 0;

	for (i=0;i< GetOSLogicalProcessorCount();i++)
	{
		////
		if (!(package == ENUM_ALL ||  glbl_ptr->pApicAffOrdMapping[i].packageORD == package))
			continue;		
		
		if (!(core == ENUM_ALL || core == glbl_ptr->pApicAffOrdMapping[i].coreORD))
			continue;

		if (!(logical == ENUM_ALL || logical == glbl_ptr->pApicAffOrdMapping[i].threadORD))
			continue;
		
		SetGenericAffinityBit(&genericAffinity, i);
	}
	MaskToHexStringGenericAffinityMask(&genericAffinity, slen, sbuf);
	FreeGenericAffinityMask(&genericAffinity);
	return 1;
}



/*
 * ListAffinityByCPUTopology
 *
 *		print each physical processor's hardware multithreading capability
 *			to screen
 * Return:      none
 */	

void ListAffinityByCPUTopology()
{unsigned  k, i, j, lcl_maxcpu =  GetOSLogicalProcessorCount();
unsigned  len; 
char sbuf[256]; // string buffer large enough to hold a generic affinity mask of 1024 bits.
	len = sizeof(DWORD_PTR) << 1;
	if(  lcl_maxcpu> MAX_LOG_CPU) 
	{ len  += ((lcl_maxcpu - MAX_LOG_CPU) >> 2) + 1;}
	for (k=0;k<GetSysProcessorPackageCount();k++)	{
		for (i=0;i<GetEnumeratedCoreCount(k);i++)		{
			printf("Individual:\n");
			for (j=0; j<GetEnumeratedThreadCount(k,i); j++) 		{
				if( GetGenericAffinityMask (k,i,j, len, sbuf) )
				{ printf("\tP:%d, C:%d, T:%d --> %s\n",k,i,j,&sbuf[0]); }
			}
			if( GetGenericAffinityMask (k,i,ENUM_ALL, len, sbuf) )
			{ printf("\nCore-aggregated:\n\tP:%d, C:%d --> %s\n",k,i,&sbuf[0]); }
		}
		if( GetGenericAffinityMask (k,ENUM_ALL,ENUM_ALL, len, sbuf) )
		{ printf("\nPkg-aggregated:\n\tP:%d --> %s\n",k,&sbuf[0]); }
	}
}

/*
 * ListVisibleCountsByCPUTopology
 *
 *		print the enumerated count of each topological level: how many packages, how many cores, how many threads.
 *			to screen
 * Return:      none
 */	
void ListVisibleCountsByCPUTopology()
{unsigned  i, j;
	for (i = 0; i < GetSysProcessorPackageCount(); i ++){
		if (GetEnumeratedCoreCount(i) > 0 ) {
			printf(" # of cores in package %2d visible to this process: %d .\n", i, GetEnumeratedCoreCount(i));
			if( glbl_ptr->EnumeratedCoreCount < glbl_ptr->OSProcessorCount && 
					glbl_ptr->perCore_detectedThreadsCount.data[i*MAX_CORES+0] > 1) {			
				printf("\t # of logical processors in Core 0 visible to this process: %d .\n", 
						glbl_ptr->perCore_detectedThreadsCount.data[i*MAX_CORES+0]);						
			}
			for (j = 1; j < GetEnumeratedCoreCount(i); j ++){
				if( glbl_ptr->EnumeratedCoreCount < glbl_ptr->OSProcessorCount && 
						glbl_ptr->perCore_detectedThreadsCount.data[i*MAX_CORES+j] > 1) {	
					printf("\t # of logical processors in Core %2d visible to this process: %d .\n", j, 
							glbl_ptr->perCore_detectedThreadsCount.data[i*MAX_CORES+j]);
				}
			}
		}
	}
}


/*
 * ListProductHWMTCapability
 *
 *		print each physical processor's hardware multithreading capability
 *			to screen
 * Return:      none
 */	
void  ListProductHWMTCapability()
{unsigned   i;
	printf("\n\tHardware multithreading capability per physical processor: \n");
	for (i = 0; i < GetSysProcessorPackageCount(); i ++){
		if( !GetHWMTConfig(i) && glbl_ptr->HWMT_SMTperCore && glbl_ptr->HWMT_SMTperPkg) {
		printf(" Physical processor %2d is capable of supporting %2d threads per core, %2d threads per package.\n", i, glbl_ptr->HWMT_SMTperCore, glbl_ptr->HWMT_SMTperPkg);
		}
	}
}

/*
 * printCPUIDInfo
 *
 *		print the raw data of specified CPUID leaf number, sub-leaf index
 *			to screen
 * Return:      none
 */	

void printCPUIDInfo(int func, int subfunc, CPUIDinfo *info)
{

	_CPUID(info,func,subfunc);
	printf("func: %d, subfunc %d EAX:%x EBX:%x ECX:%x EDX:%x\n",func,subfunc,info->EAX,info->EBX,info->ECX,info->EDX);
}

/*
 *	The reference code provided in this file is for demonstration purpose only. It assumes
 *	the hardware topology configuration within a coherent domain does not change during
 *	the life of an OS session. If an OS support advanced features that can change 
 *	hardware topology configurations, more sophisticated adaptation may be necessary
 *	to account for the hardware configuration change that might have added and reduced 
 *  the number of logical processors being managed by the OS.
 *
 *	User should also`be aware that the system topology enumeration algorithm presented here 
 *	relies on CPUID instruction providing raw data reflecting the native hardware 
 *	configuration. When an application runs inside a virtual machine hosted by a 
 *	Virtual Machine Monitor (VMM), any CPUID instructions issued by an app (or a guest OS) 
 *	are trapped by the VMM and it is the VMM's responsibility and decision to emulate  
 *	CPUID return data to the virtual machines. When deploying topology enumeration code based on 
 *	CPUID inside a VM environment, the user must consult with the VMM vendor on how an VMM
 *	will emulate CPUID instruction relating to topology enumeration.
 */
void printAdvisory()
{
 /*printf("\n\n\tAdvisory to Users on system topology enumeration\n");
 printf("\nThis utility is for demonstration purpose only. It assumes the hardware topology");
 printf("\nconfiguration within a coherent domain does not change during the life of an OS");
 printf("\nsession. If an OS support advanced features that can change hardware topology ");
 printf("\nconfigurations, more sophisticated adaptation may be necessary to account for ");
 printf("\nthe hardware configuration change that might have added and reduced the number ");
 printf("\nof logical processors being managed by the OS.");
 printf("\n\nUser should also`be aware that the system topology enumeration algorithm is ");
 printf("\nbased on the assumption that CPUID instruction will return raw data reflecting ");
 printf("\nthe native hardware configuration. When an application runs inside a virtual ");
 printf("\nmachine hosted by a Virtual Machine Monitor (VMM), any CPUID instructions ");
 printf("\nissued by an app (or a guest OS) are trapped by the VMM and it is the VMM's ");
 printf("\nresponsibility and decision to emulate/supply CPUID return data to the virtual ");
 printf("\nmachines. When deploying topology enumeration code based on querying CPUID ");
 printf("\ninside a VM environment, the user must consult with the VMM vendor on how an VMM ");
 printf("\nwill emulate CPUID instruction relating to topology enumeration.\n");
*/
}

/*
 * main
 *
 * Proceses user input from command line parameters and Dispatch 
 *		the desired topology analysis subroutines or utility rountines 
 *	Four main types of utililties are demonstrated in the dispatcher
 *		Default (command line option 's'): Enumerate system topology and 
 *			cache topology and report enumerated results
 *		Product Capability Identification (command line option 'm'):
 *			Reports the hardware multithreading capability of each physical processor
 *		CPUID raw data to screen (command line option 'c'): prints several 
 *			leaves of raw CPUID data of a logical processor to screen
 *		CPUID raw data to file (command line option 'w'): Dump all 
 *			leaves of raw CPUID data of all logical processors to specified file
 * Return:        0 if normal completion;			otherwise, aborted with error.
 */	

void get_CPUs_info(unsigned int* num_Logical_OS, unsigned int* num_Logical_process,unsigned int* num_Processor_Core,
	unsigned int* num_Physical_Socket)
{

	CPUIDinfo info;
    unsigned  i;
	unsigned  maxleaf, max8xleaf, FM_EX;
	char ch = '\0';
	//if( argn > 1) ch = (char ) toupper(argv[1][0]);

	if(glbl_ptr == NULL) {
		glbl_ptr = (GLKTSN_T *)malloc(sizeof(GLKTSN_T));
	}
	if(glbl_ptr == NULL) {
		printf("error: malloc failed at %s %d. Bye\n", __FILE__, __LINE__);
		exit(2);
	}
	memset(glbl_ptr, 0, sizeof(GLKTSN_T));
	// check CPUID leaf reporting capability is intact
	CPUID(&info, 0);	
	maxleaf = info.EAX;
	CPUID(&info, 1);	
	FM_EX = info.EAX & 0x0fff0ff0;  // retain cpuid signatures of extended family, extended model, family and model encoding
	CPUID(&info, 0x80000000);	
	max8xleaf = info.EAX;
	// Earlier Pentium 4 and Intel Xeon processor (prior to 90nm Intel Pentium 4 processor) 
	// support extended with max extended leaf index 0x80000004,
	// 90nm Intel Pentium 4 processor and later processors supports higher extended leaf index greater than 0x80000004.
	// There is one case of exception, a newer SOC CPU with cpuid signature 0x10650 is based on a core that do not support
	// CPUID leaf 4, but it did add support for leaf 0x80000008.
	if ((maxleaf <= 4 && max8xleaf > 0x80000004) && (FM_EX != 0x10650)
		) glbl_ptr->Alert_BiosCPUIDmaxLimitSetting = 1;	switch(ch) {
		//case 'H': printf("\n\n\t%s [h|s|c|m|w filename]\n", argv[0]);
		//	break;
		case 'M': 
			if (glbl_ptr->Alert_BiosCPUIDmaxLimitSetting) {
				printf("\n\n\n\tAlert: cpu_topology has detected the following symptoms: \n");
				printf("\t\t 1. This processor likely supports CPUID leaf reporting for leaf index >=4 \n");
				printf("\t\t 2. CPUID leaf reporting for leaf index >=4 may have been disabled by BIOS setting\n");
				printf("\t\t Topology Enumeration can not produce reliable results given the above symptom # 2\n");
				printf("\t\t Please correct the problem and verify by executing ""cpu_topology c"" to ensure proper CPUID leaf reporting\n");
				exit(1);
			}
			printf("\n\n\n\tHardware Capabilities : \n");
			ListProductHWMTCapability();
			break;
		case 'C': printf("\n\n\n\tRaw Data from Selected CPUID leaves : \n");
			printCPUIDInfo(0,0,&info);
			maxleaf = info.EAX;
			printCPUIDInfo(1,0,&info);
			printCPUIDInfo(2,0,&info);
			if (maxleaf >= 4) {
				info.EAX = 1;	
				for (i = 0; (info.EAX & 0xF) != 0; i++)  printCPUIDInfo(4,i,&info);                
			}

	        if (maxleaf >= 0xB) {
			    info.EBX = 1;	
                for (i = 0; info.EBX != 0; i++)  printCPUIDInfo(0xb,i,&info);                
		    }
			
			printf("Max Leaf %x, %x \n",maxleaf, max8xleaf);
			break;
		case 'W': 
			/*if(argn < 2) {
				printf("If you specify 'w' (write file) option, you have to enter name of CPUID dump file too.\n");
				exit(2);
			}*/
			//printf("Write CPUID info to file '%s'\n\n\n", argv[2]);
			//DumpCPUIDArray(argv[2]);
			printf("Write complete\n");
			break;
		case 'S': 
		default: 
			if (glbl_ptr->Alert_BiosCPUIDmaxLimitSetting) {
				printf("\n\n\n\tAlert: cpu_topology has detected the following symptoms: \n");
				printf("\t\t 1. This processor likely supports CPUID leaf reporting for leaf index >=4 \n");
				printf("\t\t 2. CPUID leaf reporting for leaf index >=4 may have been disabled by BIOS setting\n");
				printf("\t\t Topology Enumeration can not produce reliable results given the above symptom # 2\n");
				printf("\t\t Please correct the problem and verify by executing ""cpu_topology c"" to ensure proper CPUID leaf reporting\n");
				exit(1);
			}
			//printAdvisory();
			//printf("\n\n\n\tSoftware visible enumeration in the system: \n");
			if (GetOSLogicalProcessorCount()) {
				*num_Logical_OS = GetOSLogicalProcessorCount();
			//	printf("Number of logical processors visible to the OS: %d \n",GetOSLogicalProcessorCount());
			}
			if (GetSysLogicalProcessorCount()) {
				*num_Logical_process=GetSysLogicalProcessorCount();
			//	printf("Number of logical processors visible to this process: %d \n",GetSysLogicalProcessorCount());
			}
			if (GetSysProcessorCoreCount()) {
				*num_Processor_Core=GetSysProcessorCoreCount();
			//	printf("Number of processor cores visible to this process: %d \n",GetSysProcessorCoreCount());
			}
			if (GetSysProcessorPackageCount()) {
				*num_Physical_Socket=GetSysProcessorPackageCount();
			//	printf("Number of physical packages visible to this process: %d \n",GetSysProcessorPackageCount());
			}
			
			/*printf("\n\n\tHierarchical counts by levels of processor topology: \n");
			ListVisibleCountsByCPUTopology();
			printf("\n\n\tAffinity masks per SMT thread, per core, per package: \n");
			ListAffinityByCPUTopology();
			printf("\n\n\tAPIC ID listings from affinity masks\n");
			for (i = 0;i<GetSysLogicalProcessorCount();i++)	{
				if(getEnumerateAPICID(i) != 0xffffffff) {
					printf("Affinity mask %08x - apic id %x\n", 1<<i, getEnumerateAPICID(i));				
				}
			}
			if(glbl_ptr->maxCacheSubleaf != -1) {
				unsigned pkg;
				
				// initialize some static fields in the routine
				getPkgCoreThrdStr(-1, -1, -1, -1, (GenericAffinityMask *)NULL);
				for(pkg=0; pkg < GetSysProcessorPackageCount(); pkg++) {
					printf("\n\nPackage %d Cache and Thread details\n", pkg);
					PrintBoxDiagramNotes(pkg);
					ListVisibleCountsByEachCacheTopologyByPkgBox(pkg);
					
				}
			}*/
			break;
	}
	FreeArrays();
	return 0;
}

#endif

