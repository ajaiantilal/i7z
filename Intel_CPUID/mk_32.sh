#!/bin/sh


gcc -m32 -g -c get_cpuid_lix32.s -o get_cpuid_lix32.o

gcc -m32 -g -c util_os.c -o util_os.o
gcc -m32 -g -DBUILD_MAIN cpu_topo.c -o cpu_topology32.out get_cpuid_lix32.o util_os.o
