#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "assert.h"
#define MAX_PROCESSORS  32
#define bool int
#define false 0
#define true 1

#define MAX_HI_PROCESSORS    MAX_PROCESSORS

struct cpu_heirarchy_info {
    int max_online_cpu;
    int num_sockets;
    int sibling_num[MAX_HI_PROCESSORS];
    int processor_num[MAX_HI_PROCESSORS];
    int package_num[MAX_HI_PROCESSORS];
    int coreid_num[MAX_HI_PROCESSORS];
    int display_cores[MAX_HI_PROCESSORS];
    bool HT;
};

#define MAX_SK_PROCESSORS    (MAX_PROCESSORS/4)

struct cpu_socket_info {
    int max_cpu;
    int socket_num;
    int processor_num[MAX_SK_PROCESSORS];
    int num_physical_cores;
    int num_logical_cores;
};

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
    char socket_1_list[200]="", socket_0_list[200]="";

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
    int i;
    FILE *fp = fopen("/proc/cpuinfo","r");
    char strinfo[200];

    int processor_num, physicalid_num, coreid_num;
    int it_processor_num=-1, it_physicalid_num=-1, it_coreid_num=-1;
    int tmp_processor_num, tmp_physicalid_num, tmp_coreid_num;
    int old_processor_num=-1;


    if (fp!=NULL) {
        while ( fgets(strinfo,200,fp) != NULL) {
            printf(strinfo);
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

}

void print_CPU_Heirarchy(struct cpu_heirarchy_info chi)
{
    int i;
    printf("Legend: processor number is the processor number as linux knows, socket number is the socket number,\n \
	    coreid number is the core with which this processor is associated, thus for HT machines there will be 2 processors\n\
	    sharing the same core id. display core is to specify if i need to show the information about this core or not (i.e.\n\
	    if i am already showing information about a core then i dont need to show information about the sibling\n");
    for (i=0;i < chi.max_online_cpu;i++) {
        assert(i < MAX_HI_PROCESSORS);
        printf("--[%d] Processor number %d\n",i,chi.processor_num[i]);
        printf("--[%d] Socket number/Sibling number  %d,%d\n",i,chi.package_num[i],chi.sibling_num[i]);
        printf("--[%d] Core id number %d\n",i,chi.coreid_num[i]);
        printf("--[%d] Display core %d\n\n",i,chi.display_cores[i]);
    }
}

int main()
{
    struct cpu_heirarchy_info chi;
    struct cpu_socket_info socket_0={.max_cpu=0, .socket_num=0, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};
    struct cpu_socket_info socket_1={.max_cpu=0, .socket_num=1, .processor_num={-1,-1,-1,-1,-1,-1,-1,-1}};

    construct_CPU_Heirarchy_info(&chi);
    construct_sibling_list(&chi);
    print_CPU_Heirarchy(chi);
    construct_socket_information(&chi, &socket_0, &socket_1);
    print_socket_information(&socket_0);
    print_socket_information(&socket_1);

}
