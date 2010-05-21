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

int Single_Socket();
int Dual_Socket();

int numPhysicalCores, numLogicalCores;
double TRUE_CPU_FREQ;

//Info: I start from index 1 when i talk about cores on CPU

int main ()
{
        Print_Information_Processor ();

        Test_Or_Make_MSR_DEVICE_FILES ();


        struct cpu_heirarchy_info chi;
        struct cpu_socket_info socket_0={.max_cpu=0, .socket_num=0, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};
        struct cpu_socket_info socket_1={.max_cpu=0, .socket_num=1, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};

        construct_CPU_Heirarchy_info(&chi);
        construct_sibling_list(&chi);
        print_CPU_Heirarchy(chi);
        construct_socket_information(&chi, &socket_0, &socket_1);
        print_socket_information(&socket_0);
        print_socket_information(&socket_1);

        if (socket_0.max_cpu>0 && socket_1.max_cpu>0) {
                //Path for Dual Socket Code
                printf("i7z DEBUG: Dual Socket Detected\n");
                Dual_Socket();
        } else {
                //Path for Single Socket Code
                printf("i7z DEBUG: Single Socket Detected\n");
                Single_Socket();
        }
        return(1);
}
