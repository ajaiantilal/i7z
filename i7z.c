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
#include "getopt.h"
#include "i7z.h"
//#include "CPUHeirarchy.h"

struct program_options prog_options;

void print_options(struct program_options p)
{
    if (p.logging)  printf("Logging (freq) is ON\n");         else printf("Logging (freq) is OFF\n");
    if (p.templogging) printf("Temperature logging is ON\n"); else printf("Temperature logging is OFF\n");
    if (p.cstatelogging) printf("Cstate logging is ON\n");    else printf("Cstate logging is OFF\n");
}
char* CPU_FREQUENCY_LOGGING_FILE_single="cpu_freq_log.txt";
char* CPU_FREQUENCY_LOGGING_FILE_dual="cpu_freq_log_dual_%d.txt";
char* CSTATE_LOGGING_FILE_single="cpu_cstate_log.txt";
char* CSTATE_LOGGING_FILE_dual="cpu_cstate_log_dual_%d.txt";

char* TEMP_LOGGING_FILE_single = "cpu_temp_log.txt";
char* TEMP_LOGGING_FILE_dual="cpu_temp_log_dual_%d.txt";

int Single_Socket();
int Dual_Socket();

int socket_0_num=0, socket_1_num=1;
bool use_ncurses = true;

/////////////////////LOGGING TO FILE////////////////////////////////////////
FILE *fp_log_file_freq;
FILE *fp_log_file_freq_1, *fp_log_file_freq_2;

FILE *fp_log_file_Cstates;
FILE *fp_log_file_Cstates_1, *fp_log_file_Cstates_2;

FILE *fp_log_file_temp;
FILE *fp_log_file_temp_1, *fp_log_file_temp_2;

//add a \n or \t depending on the logging type (append or overwrite)
void add_slashN_or_slashT_logfile(FILE* fp) \
{
    if(prog_options.logging==1) {
        fprintf(fp, "\n");
    } else if(prog_options.logging==2) {
        fprintf(fp, "\t");
    }
}

void add_slashN_or_slashT_logfile_CSTATE_single()
{
    if (prog_options.cstatelogging) {
        add_slashN_or_slashT_logfile(fp_log_file_Cstates);
    }
}

void add_slashN_or_slashT_logfile_CSTATE_dual(int socket_num)
{
    if(socket_num==0){
        add_slashN_or_slashT_logfile(fp_log_file_Cstates_1);
    }
    if(socket_num==1){
        add_slashN_or_slashT_logfile(fp_log_file_Cstates_2);
    }
}

void zeroLogFiles_single()
{
    if(prog_options.logging) {
        fp_log_file_freq = fopen(CPU_FREQUENCY_LOGGING_FILE_single,"w");
        if ( prog_options.cstatelogging ) {
            fp_log_file_Cstates = fopen(CSTATE_LOGGING_FILE_single,"w");
            fclose(fp_log_file_Cstates);
        }
        if ( prog_options.templogging ) {
            fp_log_file_temp = fopen(TEMP_LOGGING_FILE_single,"w");
            fclose(fp_log_file_temp);
        }
    }
}

void zeroLogFiles_dual(int socket_num)
{
    if(socket_num==0) {
        if(prog_options.logging) {
            fp_log_file_freq_1 = fopen(CPU_FREQUENCY_LOGGING_FILE_single,"w");
            if ( prog_options.cstatelogging ) {
                fp_log_file_Cstates_1 = fopen(CSTATE_LOGGING_FILE_single,"w");
                fclose(fp_log_file_Cstates_1);
            }
            if ( prog_options.templogging ) {
                fp_log_file_temp_1 = fopen(TEMP_LOGGING_FILE_single,"w");
                fclose(fp_log_file_temp_1);
            }
        }
    } 
    if(socket_num==1) {
        if(prog_options.logging) {
            fp_log_file_freq_2 = fopen(CPU_FREQUENCY_LOGGING_FILE_single,"w");
            if ( prog_options.cstatelogging ) {
                fp_log_file_Cstates_2 = fopen(CSTATE_LOGGING_FILE_single,"w");
                fclose(fp_log_file_Cstates_2);
            }
            if ( prog_options.templogging ) {
                fp_log_file_temp_2 = fopen(TEMP_LOGGING_FILE_single,"w");
                fclose(fp_log_file_temp_2);
            }
        }
    } 
}


void logOpenFile_single()
{
    if(prog_options.logging==1) {
        fp_log_file_freq = fopen(CPU_FREQUENCY_LOGGING_FILE_single,"w");
        if ( prog_options.cstatelogging ) 
            fp_log_file_Cstates = fopen(CSTATE_LOGGING_FILE_single,"w");
        if ( prog_options.templogging ) 
            fp_log_file_temp = fopen(TEMP_LOGGING_FILE_single,"w");
            
    } else if(prog_options.logging==2) {
        fp_log_file_freq = fopen(CPU_FREQUENCY_LOGGING_FILE_single,"a");
        if ( prog_options.cstatelogging ) 
            fp_log_file_Cstates = fopen(CSTATE_LOGGING_FILE_single,"a");
        if ( prog_options.templogging ) 
            fp_log_file_temp = fopen(TEMP_LOGGING_FILE_single,"a");

    }
}

void logCloseFile_single()
{
    if(prog_options.logging!=0){
        if(prog_options.logging==2) {
            fprintf(fp_log_file_freq,"\n");
            //the above line puts a \n after every freq is logged.

            if ( prog_options.cstatelogging ) 
                fprintf(fp_log_file_Cstates,"\n");
            if ( prog_options.templogging ) 
                fprintf(fp_log_file_temp,"\n");
            //the above line puts a \n after every CSTATE is logged.
        }
        fclose(fp_log_file_freq);
        if (prog_options.cstatelogging)   fclose(fp_log_file_Cstates);
        if (prog_options.templogging)     fclose(fp_log_file_temp);
    }
}

// For dual socket make filename based on the socket number
void logOpenFile_dual(int socket_num)
{
    char str_file1[100];
    snprintf(str_file1,100,CPU_FREQUENCY_LOGGING_FILE_dual,socket_num);

    char str_file2[100];
    snprintf(str_file2,100,CSTATE_LOGGING_FILE_dual,socket_num);

    char str_file3[100];
    snprintf(str_file3,100,TEMP_LOGGING_FILE_dual,socket_num);


    if(socket_num==0){
        if(prog_options.logging==1)
            fp_log_file_freq_1 = fopen(str_file1,"w");
        else if(prog_options.logging==2)
            fp_log_file_freq_1 = fopen(str_file1,"a");
    }
    if(socket_num==1){
        if(prog_options.logging==1)
            fp_log_file_freq_2 = fopen(str_file1,"w");
        else if(prog_options.logging==2)
            fp_log_file_freq_2 = fopen(str_file1,"a");
    }

    if(socket_num==0){
        if(prog_options.logging==1 && prog_options.cstatelogging)
            fp_log_file_Cstates_1 = fopen(str_file2,"w");
        else if(prog_options.logging==2)
            fp_log_file_Cstates_1 = fopen(str_file2,"a");
    }
    if(socket_num==1){
        if(prog_options.logging==1 && prog_options.cstatelogging)
            fp_log_file_Cstates_2 = fopen(str_file2,"w");
        else if(prog_options.logging==2)
            fp_log_file_Cstates_2 = fopen(str_file2,"a");
    }

    if(socket_num==0){
        if(prog_options.logging==1 && prog_options.templogging)
            fp_log_file_temp_1 = fopen(str_file3,"w");
        else if(prog_options.logging==2)
            fp_log_file_temp_1 = fopen(str_file3,"a");
    }
    if(socket_num==1){
        if(prog_options.logging==1 && prog_options.templogging)
            fp_log_file_temp_2 = fopen(str_file3,"w");
        else if(prog_options.logging==2)
            fp_log_file_temp_2 = fopen(str_file3,"a");
    }
}

void logCloseFile_dual(int socket_num)
{
    if(socket_num==0){
        if(prog_options.logging!=0){
            if(prog_options.logging==2)
                fprintf(fp_log_file_freq_1,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_freq_1);
        }
    }
    if(socket_num==1){
        if(prog_options.logging!=0){
            if(prog_options.logging==2)
                fprintf(fp_log_file_freq_2,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_freq_2);
        }
    }


    if(socket_num==0){
        if(prog_options.logging!=0){
            if(prog_options.logging==2 && prog_options.cstatelogging)
                fprintf(fp_log_file_Cstates_1,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_Cstates_1);
        }
    }
    if(socket_num==1){
        if(prog_options.logging!=0){
            if(prog_options.logging==2 && prog_options.cstatelogging)
                fprintf(fp_log_file_Cstates_2,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_Cstates_2);
        }
    }

    if(socket_num==0){
        if(prog_options.logging!=0){
            if(prog_options.logging==2 && prog_options.templogging)
                fprintf(fp_log_file_temp_1,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_temp_1);
        }
    }
    if(socket_num==1){
        if(prog_options.logging!=0){
            if(prog_options.logging==2 && prog_options.templogging)
                fprintf(fp_log_file_temp_2,"\n");
                //the above line puts a \n after every freq is logged.
            fclose(fp_log_file_temp_2);
        }
    }
}


void logCpuFreq_single(float value)
{
    //below when just logging
    if(prog_options.logging!=0) {
        fprintf(fp_log_file_freq,"%f",value); //newline, replace \n with \t to get everything separated with tabs
    }
    add_slashN_or_slashT_logfile(fp_log_file_freq);
}

void logCpuFreq_single_c(char* value)
{
    //below when just logging
    if(prog_options.logging!=0) {
        fprintf(fp_log_file_freq,"%s",value); //newline, replace \n with \t to get everything separated with tabs
    }
    add_slashN_or_slashT_logfile(fp_log_file_freq);
}

void logCpuFreq_single_d(int value)
{
    //below when just logging
    if(prog_options.logging!=0) {
        fprintf(fp_log_file_freq,"%d",value); //newline, replace \n with \t to get everything separated with tabs
    }
    add_slashN_or_slashT_logfile(fp_log_file_freq);
}

// fix for issue 48, suggested by Hakan
void logCpuFreq_single_ts(struct timespec  *value) //HW use timespec to avoid floating point overflow
{
    //below when just logging
    if(prog_options.logging!=0) {
        fprintf(fp_log_file_freq,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
    }
    add_slashN_or_slashT_logfile(fp_log_file_freq);
}


void logCpuFreq_dual(float value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if(prog_options.logging!=0){
            fprintf(fp_log_file_freq_1,"%f",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_1);
    }
    if(socket_num==1){
        //below when just logging
        if(prog_options.logging!=0){
            fprintf(fp_log_file_freq_2,"%f",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_2);
    }
}

void logCpuFreq_dual_c(char* value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if(prog_options.logging!=0){
            fprintf(fp_log_file_freq_1,"%s",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_1);
    }
    if(socket_num==1){
        //below when just logging
        if(prog_options.logging!=0) {
            fprintf(fp_log_file_freq_2,"%s",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_2);
    }
}

void logCpuFreq_dual_d(int value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if(prog_options.logging!=0) {
            fprintf(fp_log_file_freq_1,"%d",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_1);
    }
    if(socket_num==1){
        //below when just logging
        if(prog_options.logging!=0) {
            fprintf(fp_log_file_freq_2,"%d",value); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_2);
    }
}

void logCpuFreq_dual_ts(struct timespec  *value, int socket_num) //HW use timespec to avoid floating point overflow
{
    if(socket_num==0){
        //below when just logging
        if(prog_options.logging!=0){
            fprintf(fp_log_file_freq_1,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_1);
    }
    if(socket_num==1){
        //below when just logging
        if(prog_options.logging!=0){
            fprintf(fp_log_file_freq_2,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
        }
        add_slashN_or_slashT_logfile(fp_log_file_freq_2);
    }
}


//cstate logging here
//i am not so sure if i am going to do a \n for the overwrite version of logging here
void logCpuCstates_single(float value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
        fprintf(fp_log_file_Cstates,"%f",value); //newline, replace \n with \t to get everything separated with tabs
    }
}

void logCpuCstates_single_c(char* value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
        fprintf(fp_log_file_Cstates,"%s",value); //newline, replace \n with \t to get everything separated with tabs
    }
}

void logCpuCstates_single_d(int value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
        fprintf(fp_log_file_Cstates,"%d",value); //newline, replace \n with \t to get everything separated with tabs
    }
}

// fix for issue 48, suggested by Hakan
void logCpuCstates_single_ts(struct timespec  *value) //HW use timespec to avoid floating point overflow
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
        fprintf(fp_log_file_Cstates,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
    }
}

void logCpuCstates_dual(float value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) { 
            fprintf(fp_log_file_Cstates_1,"%f",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_2,"%f",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
}

void logCpuCstates_dual_c(char* value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_1,"%s",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_2,"%s",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
}

void logCpuCstates_dual_d(int value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_1,"%d",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_2,"%d",value); //newline, replace \n with \t to get everything separated with tabs
        }
    }
}

void logCpuCstates_dual_ts(struct timespec  *value, int socket_num) //HW use timespec to avoid floating point overflow
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_1,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs    
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.cstatelogging) ) {
            fprintf(fp_log_file_Cstates_2,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
        }
    }
}


//temp logging here
void logCpuTemp_single(float value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
        //printf("called logCpuTemp_single loggign %d, templogging %d\n", prog_options.logging, prog_options.templogging);
        fprintf(fp_log_file_temp,"%f",value); //newline, replace \n with \t to get everything separated with tabs
        add_slashN_or_slashT_logfile(fp_log_file_temp);
    }
}

void logCpuTemp_single_c(char* value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
        fprintf(fp_log_file_temp,"%s",value); //newline, replace \n with \t to get everything separated with tabs
        add_slashN_or_slashT_logfile(fp_log_file_temp);
    }
}

void logCpuTemp_single_d(int value)
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
        fprintf(fp_log_file_temp,"%d",value); //newline, replace \n with \t to get everything separated with tabs
        add_slashN_or_slashT_logfile(fp_log_file_temp);
    }
}

// fix for issue 48, suggested by Hakan
void logCpuTemp_single_ts(struct timespec  *value) //HW use timespec to avoid floating point overflow
{
    //below when just logging
    if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
        fprintf(fp_log_file_temp,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
        add_slashN_or_slashT_logfile(fp_log_file_temp);
    }
}

void logCpuTemp_dual(float value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) { 
            fprintf(fp_log_file_temp_1,"%f",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_1);
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_2,"%f",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_2);
        }
    }
}

void logCpuTemp_dual_c(char* value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_1,"%s",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_1);        
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_2,"%s",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_2);        
        }
    }
}

void logCpuTemp_dual_d(int value,int socket_num)
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_1,"%d",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_1);
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_2,"%d",value); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_2);        
        }
    }
}

void logCpuTemp_dual_ts(struct timespec  *value, int socket_num) //HW use timespec to avoid floating point overflow
{
    if(socket_num==0){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_1,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs    
            add_slashN_or_slashT_logfile(fp_log_file_temp_1);        
        }
    }
    if(socket_num==1){
        //below when just logging
        if ( (prog_options.logging != 0)  && (prog_options.templogging) ) {
            fprintf(fp_log_file_temp_2,"%d.%.9d",value->tv_sec,value->tv_nsec); //newline, replace \n with \t to get everything separated with tabs
            add_slashN_or_slashT_logfile(fp_log_file_temp_1);        
        }
    }
}



void atexit_runsttysane()
{
    printf("Quitting i7z\n");
    system("stty sane");
}

void modprobing_msr()
{
    system("modprobe msr");
}

//Info: I start from index 1 when i talk about cores on CPU

#define MAX_FILENAME_LENGTH 1000

int main (int argc, char **argv)
{
    atexit(atexit_runsttysane);

    char log_file_name[MAX_FILENAME_LENGTH], log_file_name2[MAX_FILENAME_LENGTH+3], templog_file_name[MAX_FILENAME_LENGTH], templog_file_name2[MAX_FILENAME_LENGTH];
    char clog_file_name[MAX_FILENAME_LENGTH], clog_file_name2[MAX_FILENAME_LENGTH+3];

    struct cpu_heirarchy_info chi;
    struct cpu_socket_info socket_0={.max_cpu=0, .socket_num=0, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};
    struct cpu_socket_info socket_1={.max_cpu=0, .socket_num=1, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};

    //////////////////// GET ARGUMENTS //////////////////////
    int c;
    //char *cvalue = NULL;
    //static bool logging_val_append=false, logging_val_replace=false;
    bool presupplied_socket_info = false;
    
    static struct option long_options[]=
    {
        {"write", required_argument, 0, 'w'},
        {"socket0", required_argument,0 ,'z'},
        {"socket1", required_argument,0 ,'y'},
        {"logfile", required_argument,0,'l'},
        {"templogfile", required_argument, 0, 'x'},
        {"cstatelogfile", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {"nogui", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'v'},
        {"logtemp", no_argument, 0, 't'},
        {"logcstate", no_argument, 0, 'c'},
        {NULL, 0, 0, 0}
    };
    
    prog_options.logging=0; //0=no logging, 1=logging, 2=appending
    prog_options.templogging = 0;
    prog_options.cstatelogging = 0;

    while(1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv,"w:z:y:l:x:p:hnvtc", long_options, &option_index);
        if (c==-1)
            break;
        //printf("got option %c\n", c);
        switch(c)
        {
            case 'z':
                socket_0_num = atoi(optarg);
                presupplied_socket_info = true;
                printf("Socket_0 information will be about socket %d\n", socket_0.socket_num);
                break;
            case 'y':
                socket_1_num = atoi(optarg);
                presupplied_socket_info = true;
                printf("Socket_1 information will be about socket %d\n", socket_1.socket_num);
                break;
            case 'w':
                //printf("write options specified %s\n", optarg);
                if (strcmp("l",optarg)==0)
                {
                    prog_options.logging = 1;
                    printf("Logging is ON and set to replace\n");
                }
                if (strcmp("a",optarg)==0)
                {
                    prog_options.logging = 2;
                    printf("Logging is ON and set to append\n");
                }
                break;
            case 'l':
                strncpy(log_file_name, optarg, MAX_FILENAME_LENGTH-3);
                strcpy(log_file_name2, log_file_name);
                strcat(log_file_name2, "_%d");
                CPU_FREQUENCY_LOGGING_FILE_single = log_file_name;
                CPU_FREQUENCY_LOGGING_FILE_dual = log_file_name2;
                printf("Logging frequencies to %s for single sockets, %s for dual sockets(0,1 for multiple sockets)\n", CPU_FREQUENCY_LOGGING_FILE_single, CPU_FREQUENCY_LOGGING_FILE_dual);
                break;

            case 'p':
                strncpy(clog_file_name, optarg, MAX_FILENAME_LENGTH-3);
                strcpy(clog_file_name2, clog_file_name);
                strcat(clog_file_name2, "_%d");
                CSTATE_LOGGING_FILE_single = clog_file_name;
                CSTATE_LOGGING_FILE_dual = clog_file_name2;
                prog_options.cstatelogging = 1;
                break;
            case 'c':
                prog_options.cstatelogging = 1;
                break;

            case 'x':
                strncpy(templog_file_name, optarg, MAX_FILENAME_LENGTH-3);
                strcpy(templog_file_name2, templog_file_name);
                strcat(templog_file_name2, "_%d");
                TEMP_LOGGING_FILE_single = templog_file_name;
                TEMP_LOGGING_FILE_dual = templog_file_name2;
                prog_options.templogging = 1;
                break;
            case 't':
                prog_options.templogging = 1;
                //print_options(prog_options);
                break;

            case 'n':
                use_ncurses = false;
                printf("Not Spawning the GUI\n");
                break;

            case 'h':
                printf("\ni7z Tool Supports the following functions:\n");
                printf("Append to a log file (freq is always logged when logging is enabled):  ");
                printf("%c[%d;%d;%dm./i7z --write a ", 0x1B,1,31,40);
                printf("%c[%dm[OR] ",0x1B,0);
                printf("%c[%d;%d;%dm./i7z -w a\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);
                printf("Replacement instead of Append:  ");
                printf("%c[%d;%d;%dm./i7z --write l ", 0x1B,1,31,40);
                printf("%c[%dm[OR]", 0x1B,0);
                printf(" %c[%d;%d;%dm./i7z -w l\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);
                printf("(Enabling) Log the temperature (also needs the -w l or -w a option): ");
                printf("%c[%d;%d;%dm./i7z --logtemp ", 0x1B,1,31,40);
                printf("%c[%dm[OR]", 0x1B,0);
                printf(" %c[%d;%d;%dm./i7z -t\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);
                printf("(Enabling) Log the C-states (also needs the -w l or -w a option): ");
                printf("%c[%d;%d;%dm./i7z --logcstate ", 0x1B,1,31,40);
                printf("%c[%dm[OR]", 0x1B,0);
                printf(" %c[%d;%d;%dm./i7z -c\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);

                printf("Specifying a particular socket to print: %c[%d;%d;%dm./i7z --socket0 X \n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);
                printf("In order to print to a second socket use: %c[%d;%d;%dm./i7z --socket1 X \n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);
                printf("To turn the ncurses GUI off use: %c[%d;%d;%dm./i7z --nogui\n", 0x1B, 1, 31, 40);
                printf("%c[%dm",0x1B,0);


                printf("\nLOGGING DEFAULTS:\n(ENABLED by default when logging is enable) Default (freq) log file name is %s (single socket) or %s (dual socket)\n", CPU_FREQUENCY_LOGGING_FILE_single, CPU_FREQUENCY_LOGGING_FILE_dual);
                printf("(DISABLED by default) Default temp log file name is %s (single socket) or %s (dual socket)\n", TEMP_LOGGING_FILE_single, TEMP_LOGGING_FILE_dual);
                printf("(DISABLED by default) Default cstate log file name is %s (single socket) or %s (dual socket)\n\n", CSTATE_LOGGING_FILE_single, CSTATE_LOGGING_FILE_dual);
                printf("Specifying a different log file (freq): ");
                printf("%c[%d;%d;%dm./i7z --logfile filename ", 0x1B,1,31,40);
                printf("%c[%dm[OR] ", 0x1B,0);
                printf("%c[%d;%d;%dm./i7z -l filename\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);

                printf("Specifying a different temperature log file: ");
                printf("%c[%d;%d;%dm./i7z --templogfile filename ", 0x1B,1,31,40);
                printf("%c[%dm[OR] ", 0x1B,0);
                printf("%c[%d;%d;%dm./i7z -x filename\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);

                printf("Specifying a different cstate log file: ");
                printf("%c[%d;%d;%dm./i7z --cstatelogfile filename ", 0x1B,1,31,40);
                printf("%c[%dm[OR] ", 0x1B,0);
                printf("%c[%d;%d;%dm./i7z -p filename\n", 0x1B,1,31,40);
                printf("%c[%dm",0x1B,0);

                printf("Example: To print for two sockets and also change the log file %c[%d;%d;%dm./i7z --socket0 0 --socket1 1 -logfile /tmp/logfilei7z -w l\n", 0x1B, 1, 31, 40);
                printf("%c[%dm",0x1B,0);

                exit(1);

            case 'v':
                printf("Version: Nov-16-2012\n");
                exit(1);

            default:
                printf("no such option");
                exit(1);
        }
    }

    if(prog_options.templogging) {
            printf("Logging temperature to %s for single sockets, %s for dual sockets(0,1 for multiple sockets)\n", TEMP_LOGGING_FILE_single, TEMP_LOGGING_FILE_dual);
    }

    if(prog_options.cstatelogging) {
            printf("Logging cstates to %s for single sockets, %s for dual sockets(0,1 for multiple sockets)\n", CSTATE_LOGGING_FILE_single, CSTATE_LOGGING_FILE_dual);
    }
    Print_Version_Information();

    Print_Information_Processor (&prog_options.i7_version.nehalem, &prog_options.i7_version.sandy_bridge);

//	printf("nehalem %d, sandy brdige %d\n", prog_options.i7_version.nehalem, prog_options.i7_version.sandy_bridge);

    Test_Or_Make_MSR_DEVICE_FILES ();
    modprobing_msr();

    /*
    prog_options.logging = 0;
    if (logging_val_replace){
        prog_options.logging = 1;
        printf("Logging is ON and set to replace\n");
    }
    if (logging_val_append){
        prog_options.logging = 2;
        printf("Logging is ON and set to append\n");
    }
    */
    /*
    while( (c=getopt(argc,argv,"w:")) !=-1){
		cvalue = optarg;
    	//printf("argument %c\n",c);
    	if(cvalue == NULL){
    	    printf("With -w option, requires an argument for append or logging\n");
    	    exit(1);
    	}else{
     	    //printf("         %s\n",cvalue);
     	    if(strcmp(cvalue,"a")==0){
     		printf("Appending frequencies to %s (single_socket) or cpu_freq_log_dual_(%d/%d).txt (dual socket)\n", CPU_FREQUENCY_LOGGING_FILE_single,0,1);
     		prog_options.logging=2;
     	    }else if(strcmp(cvalue,"l")==0){
     		printf("Logging frequencies to %s (single socket) or cpu_freq_log_dual_(%d/%d).txt (dual socket) \n", CPU_FREQUENCY_LOGGING_FILE_single,0,1);
     		prog_options.logging=1;
     	    }else{
     		printf("Unknown Option, ignoring -w option.\n");
     		prog_options.logging=0;
     	    }
     	    sleep(3);
        }
    }
    */
    ///////////////////////////////////////////////////////////
    
    construct_CPU_Heirarchy_info(&chi);
    construct_sibling_list(&chi);
    print_CPU_Heirarchy(chi);
    construct_socket_information(&chi, &socket_0, &socket_1, socket_0_num, socket_1_num);
    print_socket_information(&socket_0);
    print_socket_information(&socket_1);

    if (!use_ncurses){
        printf("GUI has been Turned OFF\n");
        print_options(prog_options);
    } else {
        printf("GUI has been Turned ON\n");
        print_options(prog_options);
        /*
        if (prog_options.logging ==0)
        {
            printf("Logging is OFF\n");
        } else {
            printf("Logging is ON\n");
            if (prog_options.cstatelogging) {
                printf("Cstate logging is enabled\n");
            }
            if (prog_options.templogging) {
                printf("temp logging is enabled\n");
            }
        }
        */
    }
 
    if (!presupplied_socket_info){
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
    } else {
        Dual_Socket();
    }
    return(1);
}
