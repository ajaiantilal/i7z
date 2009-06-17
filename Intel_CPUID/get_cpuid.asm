;	Copyright 2008  Intel Corporation 
;	get_cpuid.asm
;	wrapper function to retrieve CPUID leaf and subleaf data, 
;	returns CPUID leaf/subleaf raw data in a data structure
;	This file can be compiled under 32-bit and 64-bit Windows environments.
;  Written by Patrick Fay 
;
;
;  caller supplies three parameters;
;  pointer to a data structure
;  leaf index
;  sub leaf index
;
ifdef X86_64   
; -------------------------- 64bit ----------------------
PUBLIC	get_cpuid_info
_TEXT	SEGMENT
get_cpuid_info	PROC
;The ABI for passing integer arguments is different in windows and linux. 
;In linux (that's what I used for the port), %rdi, %rsi, %rdx, %rcx, %r8 
;and %r9 are used to pass INT type parameters, in windows rcx, rdx, r8 and 
;r9 are used.
;
;REGISTER USAGE FOR WINDOWS:
;parameter regs        RCX, RDX, R8, R9, XMM0-XMM3
;scratch registers     RAX, RCX, RDX, R8-R11, ST(0)-ST(7), XMM0-XMM5
;callee-save registers RBX, RSI, RDI, RBP, R12-R15, xmm6-xmm15
;registers for return  RAX, XMM0
    ; rcx = dest
    ; dl = fill char
    ; r8 count

;REGISTER RULES FOR LINUX:
;parameter registers   RDI, RSI, RDX, RCX, R8, R9, XMM0-XMM7
;scratch registers     RAX, RCX, RDX, RSI, RDI, R8-R11, ST(0)-ST(7), XMM0-XMM15
;callee-save registers RBX, RBP, R12-R15
;registers for return  RAX, RDX, XMM0, XMM1, st(0), st(1)
;
; rax   rcx   rdx   rbx   rsp   rbp   rsi   rdi
; eax   ecx   edx   ebx   esp   ebp   esi   edi 
;  ax    cx    dx    bx    sp    bp    si    di
;  ah    ch    dh    bh    sph?  bph?  sih?  dih?
;  al    cl    dl    bl    spl   bpl   sil   dil

    ; arg0 = rcx, arg1= rdx
    mov r10, r8   ; arg2, subleaf
    mov r8, rcx   ; arg0, array addr
    mov r9, rdx   ; arg1, leaf
	push rax
	push rbx
	push rcx
	push rdx
	mov eax, r9d
	mov ecx, r10d
	cpuid
	mov	DWORD PTR [r8], eax
	mov	DWORD PTR [r8+4], ebx
	mov	DWORD PTR [r8+8], ecx
	mov	DWORD PTR [r8+12], edx
	pop rdx
	pop rcx
	pop rbx
	pop rax
	ret
get_cpuid_info	ENDP
_TEXT	ENDS
else
; -------------------------- 64bit ----------------------
	.686P
	.XMM
	.model	flat
;INCLUDELIB LIBCMT
;INCLUDELIB OLDNAMES

PUBLIC	_get_cpuid_info
; Function compile flags: /Ogtpy
_TEXT	SEGMENT
_get_cpuid_info PROC
	mov	edx, DWORD PTR 8[esp-4]   ; addr of start of output array
	mov	eax, DWORD PTR 12[esp-4]  ; leaf
	mov	ecx, DWORD PTR 16[esp-4]  ; subleaf
	push edi
	push ebx
	mov  edi, edx                      ; edi has output addr
	cpuid
	mov	DWORD PTR [edi], eax
	mov	DWORD PTR [edi+4], ebx
	mov	DWORD PTR [edi+8], ecx
	mov	DWORD PTR [edi+12], edx
	pop ebx
	pop edi
	ret
_get_cpuid_info ENDP
_TEXT	ENDS


endif
END
