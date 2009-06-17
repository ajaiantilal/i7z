v0.1

simple i7 detection utility for overclockers and clockers :)

v0.01- very simple program to examine i7 feature clocking and running with speedstep
Checked on 64-bit linux only. Should work on 32-bit too.

need ncurses library: usually something like libncurses on debian. 
also needs support of MSR (model specific register) in kernel. Usually most kernels
have it. Else run the MAKEDEV file. I do modprobing of msr within the C-program.

Compiling:
64-bit linux:
  make -f Makefile.x86_64
  
32-bit linux  
  make -f Makefile.i386

Running:
  sudo ./i7z
  
  needs sudo as MSR are usually only superuser readable/writeable
  

  
license: mixture of GPLv1, GPLv2 and Intel's license. Need to clean up code and fix the
licenses! My code in GPLv2

Intel's CPUID code can be disabled by not using the flag  -DUSE_INTEL_CPUID in the Makefiles/


coder: Abhishek Jaiantilal (abhishek.jaiantilal@colorado.edu)