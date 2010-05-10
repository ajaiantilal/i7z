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

#include "i7z.h"
//#include "CPUHeirarchy.h"

int Single_Socket(struct cpu_heirarchy_info *chi);
int Dual_Socket(struct cpu_heirarchy_info *chi);

int numPhysicalCores, numLogicalCores;
double TRUE_CPU_FREQ;

//Info: I start from index 1 when i talk about cores on CPU

int main ()
{
	struct cpu_heirarchy_info chi;
	{
		construct_cpu_hierarchy(&chi);
		print_cpu_list(chi);
		sleep(1);
	} 
	if (chi.num_sockets == 1){
		printf ("i7z DEBUG: SINGLE SOCKET DETECTED\n"); sleep(1);
		Single_Socket(&chi);
	}else if(chi.num_sockets == 2){
		printf ("i7z DEBUG: DUAL SOCKET DETECTED\n"); sleep(1);
		Dual_Socket(&chi);
	}else if(chi.num_sockets > 2){
		printf ("i7z DEBUG: MORE THAN 2 SOCKET DETECTED, BUT I WILL STILL PRINT DUAL SOCKET INFO\n"); sleep(2);
		Dual_Socket(&chi);
	}
	return(1);
}
