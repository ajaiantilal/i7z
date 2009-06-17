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
           .text                        
#get_cpuid_info PROC                    
#The ABI for passing integer arguments is different in windows and linux. 
#In linux (that's what I used for the port), %rdi, %rsi, %rdx, %rcx, %r8 
#and %r9 are used to pass INT type parameters, in windows rcx, rdx, r8 and 
#r9 are used.
#
#REGISTER USAGE FOR WINDOWS:
#parameter regs        RCX, RDX, R8, R9, XMM0-XMM3
#scratch registers     RAX, RCX, RDX, R8-R11, ST(0)-ST(7), XMM0-XMM5
#callee-save registers RBX, RSI, RDI, RBP, R12-R15, xmm6-xmm15
#registers for return  RAX, XMM0

#REGISTER RULES FOR LINUX:
#parameter registers   RDI, RSI, RDX, RCX, R8, R9, XMM0-XMM7
#scratch registers     RAX, RCX, RDX, RSI, RDI, R8-R11, ST(0)-ST(7), XMM0-XMM15
#callee-save registers RBX, RBP, R12-R15
#registers for return  RAX, RDX, XMM0, XMM1, st(0), st(1)
#
# rax   rcx   rdx   rbx   rsp   rbp   rsi   rdi
# eax   ecx   edx   ebx   esp   ebp   esi   edi 
#  ax    cx    dx    bx    sp    bp    si    di
#  ah    ch    dh    bh    sph?  bph?  sih?  dih?
#  al    cl    dl    bl    spl   bpl   sil   dil

           .global     get_cpuid_info   
get_cpuid_info:                         
           mov r8, rdi   #  array addr
           mov r9, rsi   #  leaf
           mov r10, rdx  #  subleaf

           push        rax              
           push        rbx              
           push        rcx              
           push        rdx              
           mov         eax, r9d         
           mov         ecx, r10d
           cpuid                        
           mov         DWORD PTR [r8], eax 
           mov         DWORD PTR [r8+4], ebx 
           mov         DWORD PTR [r8+8], ecx 
           mov         DWORD PTR [r8+12], edx 
           pop         rdx              
           pop         rcx              
           pop         rbx              
           pop         rax              
           ret         0                
#get_cpuid_info ENDP                    
#_TEXT     ENDS                         

