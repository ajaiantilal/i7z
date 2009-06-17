#!/bin/sh


gcc -g -c get_cpuid_lix64.s -o get_cpuid_lix64.o
gcc -g -c util_os.c
gcc -g -DBUILD_MAIN cpu_topo.c -o cpu_topology64.out get_cpuid_lix64.o util_os.o
