/* This  file is modified from source available at http://www.kernel.org/pub/linux/utils/cpu/msr-tools/
 for Model specific cpu registers
 Modified to take i7 into account by Abhishek Jaiantilal abhishek.jaiantilal@colorado.edu

// Information about i7's MSR in
// http://download.intel.com/design/processor/applnots/320354.pdf
// Appendix B of http://www.intel.com/Assets/PDF/manual/253669.pdf


#ident "$Id: rdmsr.c,v 1.4 2004/07/20 15:54:59 hpa Exp $"
 ----------------------------------------------------------------------- *
 *
 *   Copyright 2000 Transmeta Corporation - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
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
#include <assert.h>
#include "perfmon-i7z.h"

#define UINT32_MAX (0xffffffff)

//#define ULLONG_MAX 18446744073709551615

/////////////////////////////////////////READ TEMPERATURE////////////////////////////////////////////
#define IA32_THERM_STATUS 0x19C
#define IA32_TEMPERATURE_TARGET 0x1a2



/*int Get_Bits_Value(unsigned long val,int highbit, int lowbit){ 
	unsigned long data = val;
	int bits = highbit - lowbit + 1;
	if(bits<64){
	    data >>= lowbit;
	    data &= (1ULL<<bits) - 1;
	}
	return(data);
} 


int Read_Thermal_Status_CPU(int cpu_num){
	int error_indx;
	unsigned long val= get_msr_value(cpu_num,IA32_THERM_STATUS,63,0,&error_indx);
	int digital_readout = Get_Bits_Value(val,23,16);
    
	val= get_msr_value(cpu_num,IA32_TEMPERATURE_TARGET,63,0,&error_indx);
	int PROCHOT_temp = Get_Bits_Value(val,24,16);
    
	//temperature is prochot - digital readout
	return(PROCHOT_temp - digital_readout);
} */

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int num_msr()
{
        unsigned int b;
        asm volatile ("mov %1, %%eax;"
                      "cpuid;"
                      "mov %%eax, %0;"
                      :"=r" (b)
                      :"r" (0x0a)
                      :"%eax"
        );
        return( (b & 0xFF00) >> 8 );
}

int bit_width_PMCx()
{
        unsigned int b;
        asm volatile ("mov %1, %%eax;"
                      "cpuid;"
                      "mov %%eax, %0;"
                      :"=r" (b)
                      :"r" (0x0a)
                      :"%eax"
        );
        return( (b & 0xFF0000) >> 16 );
}





void
print_family_info (struct family_info *proc_info)
{
    //print CPU info
    printf ("i7z DEBUG:    Stepping %x\n", proc_info->stepping);
    printf ("i7z DEBUG:    Model %x\n", proc_info->model);
    printf ("i7z DEBUG:    Family %x\n", proc_info->family);
    printf ("i7z DEBUG:    Processor Type %x\n", proc_info->processor_type);
    printf ("i7z DEBUG:    Extended Model %x\n", proc_info->extended_model);
    //    printf("    Extended Family %x\n", (short int*)(&proc_info->extended_family));
    //    printf("    Extended Family %d\n", proc_info->extended_family);
}


#ifdef x64_BIT
void  get_vendor (char *vendor_string)
{
    //get vendor name
    unsigned int b, c, d, e;
    //  int i;
    asm volatile ("mov %4, %%eax; "	// 0 into eax
                  "cpuid;" "mov %%eax, %0;"	// eeax into b
                  "mov %%ebx, %1;"	// eebx into c
                  "mov %%edx, %2;"	// eeax into d
                  "mov %%ecx, %3;"	// eeax into e
              :"=r" (b), "=r" (c), "=r" (d), "=r" (e)	/* output */
                          :"r" (0)	/* input */
                          :"%eax", "%ebx", "%ecx", "%edx"	/* clobbered register, will be modifying inside the asm routine so dont use them */
                         );
    memcpy (vendor_string, &c, 4);
    memcpy (vendor_string + 4, &d, 4);
    memcpy (vendor_string + 8, &e, 4);
    vendor_string[12] = '\0';
    //        printf("Vendor %s\n",vendor_string);
}
#endif

int turbo_status ()
{
    //turbo state flag
    unsigned int eax;
    //  int i;
    asm volatile ("mov %1, %%eax; "	// 0 into eax
                  "cpuid;" "mov %%eax, %0;"	// eeax into b
              :"=r" (eax)	/* output */
                          :"r" (6)	/* input */
                          :"%eax"		/* clobbered register, will be modifying inside the asm routine so dont use them */
                         );

    //printf("eax %d\n",(eax&0x2)>>1);

    return ((eax & 0x2) >> 1);
}

void get_familyinformation (struct family_info *proc_info)
{
    //get info about CPU
    unsigned int b;
    asm volatile ("mov %1, %%eax; "	// 0 into eax
                  "cpuid;" "mov %%eax, %0;"	// eeax into b
              :"=r" (b)	/* output */
                          :"r" (1)	/* input */
                          :"%eax"		/* clobbered register, will be modifying inside the asm routine so dont use them */
                         );
    //  printf ("eax %x\n", b);
    proc_info->stepping = b & 0x0000000F;	//bits 3:0
    proc_info->model = (b & 0x000000F0) >> 4;	//bits 7:4
    proc_info->family = (b & 0x00000F00) >> 8;	//bits 11:8
    proc_info->processor_type = (b & 0x00007000) >> 12;	//bits 13:12
    proc_info->extended_model = (b & 0x000F0000) >> 16;	//bits 19:16
    proc_info->extended_family = (b & 0x0FF00000) >> 20;	//bits 27:20
}

double estimate_MHz ()
{
    //copied blantantly from http://www.cs.helsinki.fi/linux/linux-kernel/2001-37/0256.html
    /*
    * $Id: MHz.c,v 1.4 2001/05/21 18:58:01 davej Exp $
    * This file is part of x86info.
    * (C) 2001 Dave Jones.
    *
    * Licensed under the terms of the GNU GPL License version 2.
    *
    * Estimate CPU MHz routine by Andrea Arcangeli <andrea@suse.de>
    * Small changes by David Sterba <sterd9am@ss1000.ms.mff.cuni.cz>
    *
    */
    struct timezone tz;
    struct timeval tvstart, tvstop;
    unsigned long long int cycles[2];	/* gotta be 64 bit */
    unsigned long long int microseconds;	/* total time taken */

    memset (&tz, 0, sizeof (tz));

    /* get this function in cached memory */
    gettimeofday (&tvstart, &tz);
    cycles[0] = rdtsc ();
    gettimeofday (&tvstart, &tz);

    /* we don't trust that this is any specific length of time */
    //1 sec will cause rdtsc to overlap multiple times perhaps. 100msecs is a good spot
    usleep (100000);

    cycles[1] = rdtsc ();
    gettimeofday (&tvstop, &tz);
    microseconds = ((tvstop.tv_sec - tvstart.tv_sec) * 1000000) +
                   (tvstop.tv_usec - tvstart.tv_usec);

    unsigned long long int elapsed = 0;
    if (cycles[1] < cycles[0])
    {
        //printf("c0 = %llu   c1 = %llu",cycles[0],cycles[1]);
        elapsed = UINT32_MAX - cycles[0];
        elapsed = elapsed + cycles[1];
        //printf("c0 = %llu  c1 = %llu max = %llu elapsed=%llu\n",cycles[0], cycles[1], UINT32_MAX,elapsed);
    }
    else
    {
        elapsed = cycles[1] - cycles[0];
        //printf("\nc0 = %llu  c1 = %llu elapsed=%llu\n",cycles[0], cycles[1],elapsed);
    }

    double mhz = elapsed / microseconds;


    //printf("%llg MHz processor (estimate).  diff cycles=%llu  microseconds=%llu \n", mhz, elapsed, microseconds);
    //printf("%g  elapsed %llu  microseconds %llu\n",mhz, elapsed, microseconds);
    return (mhz);
}

/* Number of decimal digits for a certain number of bits */
/* (int) ceil(log(2^n)/log(10)) */
int decdigits[] = {
    1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5,
    5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10,
    10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15,
    15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19,
    20
};

#define mo_hex  0x01
#define mo_dec  0x02
#define mo_oct  0x03
#define mo_raw  0x04
#define mo_uns  0x05
#define mo_chx  0x06
#define mo_mask 0x0f
#define mo_fill 0x40
#define mo_c    0x80

const char *program;


uint64_t get_msr_value (int cpu, uint32_t reg, unsigned int highbit,
                        unsigned int lowbit, int* error_indx)
{
    uint64_t data;
    int fd;
    //  char *pat;
    //  int width;
    char msr_file_name[64];
    int bits;
    *error_indx =0;

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_RDONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            //fprintf (stderr, "rdmsr: No CPU %d\n", cpu);
            *error_indx = 1;
            return 1;
        } else if (errno == EIO) {
            //fprintf (stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
            *error_indx = 1;
            return 1;
        } else {
            //perror ("rdmsr:open");
            *error_indx = 1;
            return 1;
            //exit (127);
        }
    }

    if (pread (fd, &data, sizeof data, reg) != sizeof data)
    {
        //perror ("rdmsr:pread");
        *error_indx = 1;
        return 1;
        //exit (127);
    }

    close (fd);

    bits = highbit - lowbit + 1;
    if (bits < 64)
    {
        /* Show only part of register */
        data >>= lowbit;
        data &= (1ULL << bits) - 1;
    }

    /* Make sure we get sign correct */
    if (data & (1ULL << (bits - 1)))
    {
        data &= ~(1ULL << (bits - 1));
        data = -data;
    }

    *error_indx = 0;
    return (data);
}

uint64_t set_msr_value (int cpu, uint32_t reg, uint64_t data, int* error_indx)
{
    int fd;
    char msr_file_name[64];

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_WRONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            //fprintf (stderr, "wrmsr: No CPU %d\n", cpu);
            //exit (2);
            *error_indx = 1;
            return 1;
        } else if (errno == EIO) {
            //fprintf (stderr, "wrmsr: CPU %d doesn't support MSRs\n", cpu);
            //exit (3);
            *error_indx = 1;
            return 1;
        } else {
            //perror ("wrmsr:open");
            //exit (127);
            *error_indx = 1;
            return 1;
        }
    }

    if (pwrite (fd, &data, sizeof data, reg) != sizeof data)
    {
        //perror ("wrmsr:pwrite");
        //exit (127);
        *error_indx = 1;
        return 1;
    }
    close(fd);
    return(1);
}


#ifdef USE_INTEL_CPUID
void get_CPUs_info (unsigned int *num_Logical_OS,
                    unsigned int *num_Logical_process,
                    unsigned int *num_Processor_Core,

                    unsigned int *num_Physical_Socket);

#endif


//Below code
/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Abhishek Jaiantilal
 *
 *   Under GPL v2
 *
 * ----------------------------------------------------------------------- */

void Print_Version_Information()
{
	printf ("i7z DEBUG: i7z version: %s\n",i7z_VERSION_INFO);
}


void Print_Information_Processor() 
{
    struct family_info proc_info;

#ifdef x64_BIT
    char vendor_string[13];
    get_vendor (vendor_string);
    if (strcmp (vendor_string, "GenuineIntel") == 0)
        printf ("i7z DEBUG: Found Intel Processor\n");
    else
    {
        printf
        ("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
        exit (1);
    }
#endif

#ifndef x64_BIT
    //anecdotal evidence: get_vendor doesnt seem to work on 32-bit
    printf
    ("I dont know the CPUID code to check on 32-bit OS, so i will assume that you have an Intel processor\n");
    printf ("Don't worry if i don't find a nehalem next, i'll quit anyways\n");
#endif

    get_familyinformation (&proc_info);
    print_family_info (&proc_info);

    //printf("%x %x",proc_info.extended_model,proc_info.family);

    //check if its nehalem or exit
    //Info from page 641 of Intel Manual 3B
    //Extended model and Model can help determine the right cpu
    printf("i7z DEBUG: msr = Model Specific Register\n");
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
        } else if (proc_info.extended_model == 0x2)
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
        } else {
            printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
            exit (1);
        }
    } else {
        printf ("i7z DEBUG: Unknown processor, not exactly based on Nehalem\n");
        exit (1);
    }

}

void Test_Or_Make_MSR_DEVICE_FILES() 
{
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
        } else {
            printf ("i7z DEBUG: You DONOT have write permissions to msr device files\n");
            printf ("i7z DEBUG: A solution is to run this program as root\n");
            exit (1);
        }
    } else {
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
        } else {
            printf ("i7z DEBUG: You DONOT have root privileges, mknod to create device entries won't work out\n");
            printf ("i7z DEBUG: A solution is to run this program as root\n");
            exit (1);
        }
    }
}
double cpufreq_info()
{
    //CPUINFO is wrong for i7 but correct for the number of physical and logical cores present
    //If Hyperthreading is enabled then, multiple logical processors will share a common CORE ID
    //http://www.redhat.com/magazine/022aug06/departments/tips_tricks/
    system
    ("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > /tmp/cpufreq.txt");


    //Open the parsed cpufreq file and obtain the cpufreq from /proc/cpuinfo
    FILE *tmp_file;
    tmp_file = fopen ("/tmp/cpufreq.txt", "r");
    char tmp_str[30];
    fgets (tmp_str, 30, tmp_file);
    fclose (tmp_file);
    return atof(tmp_str);
}

int check_and_return_processor(char*strinfo)
{
    char *t1;
    if (strstr(strinfo,"processor") !=NULL) {
        strtok(strinfo,":");
        t1 = strtok(NULL, " ");
        return(atoi(t1));
    } else {
        return(-1);
    }
}

int check_and_return_physical_id(char*strinfo)
{
    char *t1;
    if (strstr(strinfo,"physical id") !=NULL) {
        strtok(strinfo,":");
        t1 = strtok(NULL, " ");
        return(atoi(t1));
    } else {
        return(-1);
    }
}

int check_and_return_core_id(char*strinfo)
{
    char *t1;
    if (strstr(strinfo,"core id") !=NULL) {
        strtok(strinfo,":");
        t1 = strtok(NULL, " ");
        return(atoi(t1));
    } else {
        return(-1);
    }
}

void construct_sibling_list(struct cpu_heirarchy_info* chi)
{
    int i,j,core_id,socket_id;
    for (i=0;i< chi->max_online_cpu ;i++) {
        assert(i < MAX_HI_PROCESSORS);
        chi->sibling_num[i]=-1;
    }

    chi->HT=false;
    for (i=0;i< chi->max_online_cpu ;i++) {
        assert(i < MAX_HI_PROCESSORS);
        core_id = chi->coreid_num[i];
        socket_id = chi->package_num[i];
        for (j=i+1;j< chi->max_online_cpu ;j++) {
            assert(j < MAX_HI_PROCESSORS);
            if (chi->coreid_num[j] == core_id && chi->package_num[j] == socket_id) {
                chi->sibling_num[j] = i;
                chi->sibling_num[i] = j;
                chi->display_cores[i] = 1;
                chi->display_cores[j] = -1;
                chi->HT=true;
                continue;
            }
        }
    }
    //for cores that donot have a sibling put in 1
    for (i=0;i< chi->max_online_cpu ;i++) {
        assert(i < MAX_HI_PROCESSORS);
        if (chi->sibling_num[i] ==-1)
            chi->display_cores[i] = 1;
    }
}

void construct_socket_information(struct cpu_heirarchy_info* chi,struct cpu_socket_info* socket_0,struct cpu_socket_info* socket_1)
{
    int i;

    socket_0->max_cpu=0;
    socket_0->num_physical_cores=0;
    socket_0->num_logical_cores=0;
    socket_1->max_cpu=0;
    socket_1->num_physical_cores=0;
    socket_1->num_logical_cores=0;


    for (i=0;i< chi->max_online_cpu ;i++) {
        assert(i < MAX_HI_PROCESSORS);
        if (chi->display_cores[i]!=-1) {
            if (chi->package_num[i]==0) {
                assert(socket_0->max_cpu < MAX_SK_PROCESSORS);
                socket_0->processor_num[socket_0->max_cpu]=chi->processor_num[i];
                socket_0->max_cpu++;
                socket_0->num_physical_cores++;
                socket_0->num_logical_cores++;
            }
            if (chi->package_num[i]==1) {
                assert(socket_1->max_cpu < MAX_SK_PROCESSORS);
                socket_1->processor_num[socket_1->max_cpu]=chi->processor_num[i];
                socket_1->max_cpu++;
                socket_1->num_physical_cores++;
                socket_1->num_logical_cores++;
            }
        } else {
            if (chi->package_num[i]==0) {
                socket_0->num_logical_cores++;
            }
            if (chi->package_num[i]==1) {
                socket_1->num_logical_cores++;
            }
        }
    }
}

void print_socket_information(struct cpu_socket_info* socket)
{
    int i;
    char socket_list[200]="";

    for (i=0;i< socket->max_cpu ;i++) {
        assert(i < MAX_SK_PROCESSORS);
        if (socket->processor_num[i]!=-1) {
            sprintf(socket_list,"%s%d,",socket_list,socket->processor_num[i]);
        }
    }
    printf("Socket-%d [num of cpus %d physical %d logical %d] %s\n",socket->socket_num,socket->max_cpu,socket->num_physical_cores,socket->num_logical_cores,socket_list);
}

void construct_CPU_Heirarchy_info(struct cpu_heirarchy_info* chi)
{
    FILE *fp = fopen("/proc/cpuinfo","r");
    char strinfo[200];

    int processor_num, physicalid_num, coreid_num;
    int it_processor_num=-1, it_physicalid_num=-1, it_coreid_num=-1;
    int tmp_processor_num, tmp_physicalid_num, tmp_coreid_num;
    int old_processor_num=-1;

    memset(chi, 0, sizeof(*chi));

    if (fp!=NULL) {
        while ( fgets(strinfo,200,fp) != NULL) {
            //		printf(strinfo);
            tmp_processor_num = check_and_return_processor(strinfo);
            tmp_physicalid_num = check_and_return_physical_id(strinfo);
            tmp_coreid_num = check_and_return_core_id(strinfo);


            if (tmp_processor_num != -1) {
                it_processor_num++;
                processor_num = tmp_processor_num;
                assert(it_processor_num < MAX_HI_PROCESSORS);
                chi->processor_num[it_processor_num] = processor_num;
            }
            if (tmp_physicalid_num != -1) {
                it_physicalid_num++;
                physicalid_num = tmp_physicalid_num;
                assert(it_physicalid_num < MAX_HI_PROCESSORS);
                chi->package_num[it_physicalid_num] = physicalid_num;
            }
            if (tmp_coreid_num != -1) {
                it_coreid_num++;
                coreid_num = tmp_coreid_num;
                assert(it_coreid_num < MAX_HI_PROCESSORS);
                chi->coreid_num[it_coreid_num] = coreid_num;
            }
            if (processor_num != old_processor_num) {
                old_processor_num = processor_num;
            }
        }
    }
    chi->max_online_cpu = it_processor_num+1;
    fclose(fp);
}

void print_CPU_Heirarchy(struct cpu_heirarchy_info chi)
{
    int i;
    printf("\n------------------------------\n--[core id]--- Other information\n-------------------------------------\n");
    for (i=0;i < chi.max_online_cpu;i++) {
        assert(i < MAX_HI_PROCESSORS);
        printf("--[%d] Processor number %d\n",i,chi.processor_num[i]);
        printf("--[%d] Socket number/Hyperthreaded Sibling number  %d,%d\n",i,chi.package_num[i],chi.sibling_num[i]);
        printf("--[%d] Core id number %d\n",i,chi.coreid_num[i]);
        printf("--[%d] Display core in i7z Tool: %s\n\n",i,(chi.display_cores[i]==1)?"Yes":"No");
    }
}

int in_core_list(int ii,int* core_list)
{
    int i;
    int in=0;
    for (i=0;i<8;i++) {
        if (ii == core_list[i]) {
            in=1;
            break;
        }
    }
    return(in);
}

