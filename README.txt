license: My code is GPLv2
Current Version: v0.21-1 (13/Feb/2010)

Compiling:
64-bit linux:
  make -f Makefile.x86_64
  
32-bit linux  
  make -f Makefile.i386

Running:
  sudo ./i7z

  needs sudo as MSR are usually only superuser readable/writeable

  need ncurses library: usually something like libncurses on debian. 
also needs support of MSR (model specific register) in kernel. Usually most kernels
have it. Else run the MAKEDEV file. I do modprobing of msr within the C-program.

I added in new code that shows a nice GUI.
The Makefile for that is in GUI/ subdirectory. Just install a couple of qt packages
and you should be all set to run it. There is a README file that lists those packages

Version and Bug History:
v0.21-2 (13/Feb/2010)
    Minor Bug Fix, that happened as auto typecasting of double to float wasn't done
    A variable (numLogical) was getting overwritten, so moved it to global

v0.21 (12/Feb/2010)
- Lots of edits. Namely changed the way the number of cores were always fixed at 4
Now arbitrary number of cores can be detected, thus i3, i5, i7 and gulftown (6-cores)
can be detected. Also added code to detect the whole nehalem family rather than just i7

- removed Intel_CPUID directory which was used earlier to know the number of logical
and physical processor. It was iffy when cores where shut down in OS and when license
was concerned. Now i use just /proc/cpuinfo to figure out stuff.

v0.2

-added a gui version that uses qt4. makefile in GUI subdirectory
-now C0+C1+C3+C6 = 100% 

v0.1

simple i7 detection utility for overclockers and clockers :)

v0.01- very simple program to examine i7 feature clocking and running with speedstep
Checked on 64-bit linux only. Should work on 32-bit too.

need ncurses library: usually something like libncurses on debian. 
also needs support of MSR (model specific register) in kernel. Usually most kernels
have it. Else run the MAKEDEV file. I do modprobing of msr within the C-program.

  
  


coder: Abhishek Jaiantilal (abhishek.jaiantilal@colorado.edu)