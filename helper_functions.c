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
#include "i7z.h"

//#define ULLONG_MAX 18446744073709551615

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
void
get_vendor (char *vendor_string)
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

int
turbo_status ()
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

void
get_familyinformation (struct family_info *proc_info)
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

double
estimate_MHz ()
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


uint64_t
get_msr_value (int cpu, uint32_t reg, unsigned int highbit,
	       unsigned int lowbit, int* error_indx)
{
  uint64_t data;
  int fd;
//  char *pat;
//  int width;
  char msr_file_name[64];
  int bits;


  sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
  fd = open (msr_file_name, O_RDONLY);
  if (fd < 0)
  {
    if (errno == ENXIO)
	{
	  //fprintf (stderr, "rdmsr: No CPU %d\n", cpu);
	  *error_indx = 1;
	  return 1;
	}else if (errno == EIO){
	  //fprintf (stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
	  *error_indx = 1;
	  return 1;
	}else{
	  //perror ("rdmsr:open");
	  *error_indx = 1;
	  return 1;
	  //exit (127);
	}
  }

  if (pread (fd, &data, sizeof data, reg) != sizeof data)
  {
      perror ("rdmsr:pread");
      exit (127);
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

uint64_t
set_msr_value (int cpu, uint32_t reg, uint64_t data)
{
  int fd;
  char msr_file_name[64];

  sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
  fd = open (msr_file_name, O_WRONLY);
  if (fd < 0)
  {
    if (errno == ENXIO)
	{
	  fprintf (stderr, "wrmsr: No CPU %d\n", cpu);
	  exit (2);
	}else if (errno == EIO){
	  fprintf (stderr, "wrmsr: CPU %d doesn't support MSRs\n", cpu);
	  exit (3);
	}else{
	  perror ("wrmsr:open");
	  exit (127);
	}
  }

  if (pwrite (fd, &data, sizeof data, reg) != sizeof data)
  {
      perror ("wrmsr:pwrite");
      exit (127);
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

void Print_Information_Processor(){
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

}

void Test_Or_Make_MSR_DEVICE_FILES(){
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

int get_number_of_present_cpu()
{
    FILE *fp;
    char tmp_str[200];
    char command_str[200];
    sprintf(command_str,"cat /sys/devices/system/cpu/present > /tmp/CPUspresent.txt");
    system(command_str);
    fp = fopen("/tmp/CPUspresent.txt","r");
    fgets (tmp_str, 200, fp);
    //printf("%s\n",tmp_str);
    char* cpu_present = strtok(tmp_str,",");
    int start_core, end_core;
    sscanf(cpu_present,"%d-%d", &start_core, &end_core);
    //printf("%d --- %d\n", start_core, end_core);
    fclose(fp);
    return(end_core+1);
}

void get_online_cpus(struct cpu_heirarchy_info* chi)
{
    FILE *fp;
    int i;
    char tmp_str[200];
    char command_string[200];
    sprintf(command_string,"cat /sys/devices/system/cpu/online > /tmp/CPUspresent.txt");

    system(command_string);
    fp = fopen("/tmp/CPUspresent.txt","r");
    fgets (tmp_str, 200, fp);
    fclose (fp);
    
    char* cpu_present;
    cpu_present = strtok(tmp_str,",");
    int start_core, end_core;

	chi->max_online_cpu=0;
    while(cpu_present != NULL){
        //int num_cores = atoi (tmp_str);
        //printf("number of Cores present %s\n",cpu_present);
        sscanf(cpu_present,"%d-%d", &start_core, &end_core);
        for (i=start_core;i<=end_core;i++){
    	    chi->core_state[i]=1;
			chi->max_online_cpu++;
		}
        //printf("%d --- %d\n", start_core, end_core);
        cpu_present = strtok(NULL, ",");
    }
}

void get_siblings_list(struct cpu_heirarchy_info* chi)
{
	int i;
	int	sib_1, sib_2;
	FILE *fp;
	char command_string[200];
	char tmp_str[200], *sib_list;
	sprintf(command_string,"cat /sys/devices/system/cpu/cpu*/topology/thread_siblings_list > /tmp/SIBLINGSlist.txt");

	system(command_string);
	fp = fopen("/tmp/SIBLINGSlist.txt","r");


	for(i=0; i < chi->max_online_cpu ; i++){
   		fgets(tmp_str, 200, fp);
//		printf("%s\n",tmp_str);
	    sib_list = strtok(tmp_str,",");
		
		sib_1 = atoi(sib_list);
		sib_2 = sib_1;
		while(sib_list != NULL){
			sib_list = strtok(NULL,",");
			if (sib_list != NULL)
				sib_2 = atoi(sib_list);
		}
//	    printf("-%d, %d\n", sib_1, sib_2);
		chi->sibling_num[sib_1] = sib_2;
		chi->sibling_num[sib_2] = sib_1;
	}
	fclose(fp);
}

void get_package_ids(struct cpu_heirarchy_info* chi)
{
	int i;
	FILE *fp;
	char command_string[200];
	char tmp_str[200];

	chi->num_sockets=0;
	for(i=0; i < chi->max_present_cpu ; i++){
		if(chi->core_state[i]==1){
			sprintf(command_string,"cat /sys/devices/system/cpu/cpu%d/topology/physical_package_id > /tmp/cpu_phy_id.txt",i);
			system(command_string);
		    fp = fopen("/tmp/cpu_phy_id.txt","r");
		    fgets (tmp_str, 200, fp);
			chi->package_num[i] = atoi(tmp_str);
			if (chi->num_sockets < chi->package_num[i])
				chi->num_sockets = chi->package_num[i];
		}
	}
	chi->num_sockets = chi->num_sockets+1;
	fclose(fp);
}

void get_candidate_cores(struct cpu_heirarchy_info* chi)
{
	int i;
	char tmp_str[200];
	for(i=0; i < chi->max_present_cpu ; i++){	
			chi->candidate_cores[i]=-1;
			sprintf(tmp_str,"/dev/cpu/%d/msr",i);
			if (access (tmp_str, F_OK) == 0){
				chi->candidate_cores[i]=1;
			}
	}
}

/*void get_candidate_cores(struct cpu_heirarchy_info* chi)
{
	int i;
	for(i=0; i < chi->max_present_cpu ; i++){
		if (chi->core_state[i]==1)
            //Browse the core list and put -1 to siblings and +1 to the core id
			//do it only if it is not preset i.e do only if it was 0
			if (chi->candidate_cores[i] == 0 ){
				if (chi->sibling_num[i] != i ){ //the sibling of this core is online
					chi->candidate_cores[chi->sibling_num[i]]=-1;
					chi->candidate_cores[i]=1;
				}else{ // the sibling of the core is offline and thus the sibling of the core is core itself
					   // in that case just set it to 1 and leave
					chi->candidate_cores[i]=1;
				}
			}
	}	
}*/

void print_cpu_list(struct cpu_heirarchy_info chi)
{
    int i;
    for( i=0; i<chi.max_present_cpu; i++){
		if(chi.core_state[i]==0){
		    printf("CPU %d: offline, Package %d\n", i, chi.package_num[i]);
		}else if(chi.core_state[i]==1){
			if (i==chi.sibling_num[i]){
				printf("CPU %d: online, Package %d, [%d],  sibling offline\n", i, chi.package_num[i], chi.candidate_cores[i]);				
			}else{
				printf("CPU %d: online, Package %d, [%d],  sibling %d\n", i, chi.package_num[i], chi.candidate_cores[i], chi.sibling_num[i]);
			}
		}
    }
}



void construct_cpu_hierarchy(struct cpu_heirarchy_info *chi)
{ 

  chi->max_present_cpu = get_number_of_present_cpu();
  chi->core_state  = (int*)calloc(chi->max_present_cpu,sizeof(int));
  chi->package_num = (int*)calloc(chi->max_present_cpu,sizeof(int));
  chi->sibling_num = (int*)calloc(chi->max_present_cpu,sizeof(int));
  chi->candidate_cores = (int*)calloc(chi->max_present_cpu,sizeof(int));

  get_online_cpus(chi);
  get_siblings_list(chi);
  get_package_ids(chi);
  get_candidate_cores(chi);

  //printf("Number of CPU Sockets %d\n", chi->num_sockets);
  //printf("Number of CPU present %d\n", chi->max_present_cpu);
  //printf("Number of CPU online  %d\n", chi->max_online_cpu);

}

void from_cpu_heirarchy_info_get_information_about_socket_cores(struct cpu_heirarchy_info *chi, int socket_num, int* core_array,
	int* core_arraysize_phy, int* core_arraysize_log)
{
	int i,j,jj;
	j=0,jj=0;
	
	for(i=0;i<8;i++)
		core_array[i]=-1;

	for (i=0; i < chi->max_present_cpu ; i++){
		if(chi->package_num[i] == socket_num && chi->candidate_cores[i]==1 ){
			core_array[j++] = i;
		}		
		if(chi->package_num[i] == socket_num ){
			jj++;
		}		
 	}	
	*core_arraysize_phy = j;
	*core_arraysize_log = jj;
}

