#	Copyright 2008 Intel Corporation 
#	get_cpuid.asm
#	wrapper function to retrieve CPUID leaf and subleaf data, 
#	returns CPUID leaf/subleaf raw data in a data structure
#
#  Written by Patrick Fay 
#
#
#  caller supplies three parameters;
#  pointer to a data structure
#  leaf index
#  sub leaf index
#
.intel_syntax noprefix                  

# -------------------------- 64bit ----------------------
#          .686P                        
#          .XMM                         
#          .model      flat             
#INCLUDELIB LIBCMT
#INCLUDELIB OLDNAMES

# Function compile flags: /Ogtpy
           .text                        
#_get_cpuid_info PROC                   
           .global     get_cpuid_info  
get_cpuid_info:                        
           mov	edx, DWORD PTR 8[esp-4]   # addr of start of output array
           mov	eax, DWORD PTR 12[esp-4]  # leaf
           mov	ecx, DWORD PTR 16[esp-4]  # subleaf
           push        edi              
           push        ebx              
           mov         edi, edx         # edi has output addr
           cpuid                        
           mov         DWORD PTR [edi], eax 
           mov         DWORD PTR [edi+4], ebx 
           mov         DWORD PTR [edi+8], ecx 
           mov         DWORD PTR [edi+12], edx 
           pop         ebx              
           pop         edi              
           ret
#_get_cpuid_info ENDP                   
#_TEXT     ENDS                         

#          END                          
