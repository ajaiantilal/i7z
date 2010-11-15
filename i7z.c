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

struct program_options prog_options;

int Single_Socket();
int Dual_Socket();


//Info: I start from index 1 when i talk about cores on CPU

int main (int argc, char **argv)
{
    prog_options.logging=0; //0=no logging, 1=logging, 2=appending

    //////////////////// GET ARGUMENTS //////////////////////
    int c;
    char *cvalue = NULL;
    while( (c=getopt(argc,argv,"w:")) !=-1){
	cvalue = optarg;
    	//printf("argument %c\n",c);
    	if(cvalue == NULL){
    	    printf("With -w option, requires an argument for append or logging\n");
    	    exit(1);
    	}else{
     	    //printf("         %s\n",cvalue);
     	    if(strcmp(cvalue,"a")==0){
     		printf("Appending frequencies to %s\n", CPU_FREQUENCY_LOGGING_FILE);
     		prog_options.logging=2;
     	    }else if(strcmp(cvalue,"l")==0){
     		printf("Logging frequencies to %s\n", CPU_FREQUENCY_LOGGING_FILE);
     		prog_options.logging=1;
     	    }else{
     		printf("Unknown Option, ignoring -w option.\n");
     		prog_options.logging=0;

     	    }
     	    sleep(3);
        }
    }
    ///////////////////////////////////////////////////////////
    
    
    Print_Version_Information();

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
        //Dual_Socket(&prog_options);
        Dual_Socket();
    } else {
        //Path for Single Socket Code
        printf("i7z DEBUG: Single Socket Detected\n");
        //Single_Socket(&prog_options);
        Single_Socket();
    }
    return(1);
}
