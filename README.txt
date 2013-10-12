license: My code is GPLv2, Details in COPYING
Current Version: git-93 (May/2013)

Prerequisites:
Ivy Bridge processors seem to need phc_intel to function correctly.

Compiling:
32/64-bit linux:
  make   
  
Running:
  sudo ./i7z

  needs sudo as MSR are usually only superuser readable/writeable. or if the device nodes
  are readable under your account then they will work out fine without the sudo

  need ncurses library: usually something like libncurses on debian. 
also needs support of MSR (model specific register) in kernel. Usually most kernels
have it. Else run the MAKEDEV file. I do modprobing of msr within the C-program.


I added in new code that shows a nice GUI.
The Makefile for that is in GUI/ subdirectory. Just install a couple of qt packages
and you should be all set to run it. There is a README file that lists those packages.
Run the following commands in GUI directory: qmake; make clean; make

Running GUI:
    sudo ./i7z_GUI
    

Installation 
    sudo make install
    

Version and Bug History:
v git-93 (27/May/2013)
	added full (lol) support for Ivy Bridge

v svn 103 (Sep/2012)
	some fixes for segv fault when cpuid code is not inline
	
v svn-43 (27/May/2010)
	moved some global variables into individual functions.
        removed a redundant line that was bieng printed
	GUI version should support upto 12 physical cores. can be easily edited for more cores.

v svn-40 (27/May/2010)
	fixed bugs in dual and single socket when there are too many cores.

v svn-36 (18/May/2010)
	Fixed a bug with printing the Multiplier Line when only 1 core was enabled.
	
v svn-31 (17/May/2010)
	Supports Dual sockets. Allows for on the fly disabling/enabling of cores without
	crashing.

v0.21-4 (22/Feb/2010)
    No bugs fixed, except better documentation of the code and fixing on the Makefiles,
    c/header files and better loading/checking of msr    

v0.21-3 (13/Feb/2010)
    Minor Bug Fix, that happened as auto typecasting of double to float wasn't done
    A variable (numLogical) was getting overwritten, so moved it to global
    Seems that flags for optimization were screwing things up, so now no optimization
    GUI still has -O1 optimization flags
    
    BTW why sudden increase from 0.2-1 to 0.21-3. There were 3 minor edits in between
    And then for me 0.21 and 0.2 are just 0.01 apart rather then 19 minor aparts. 
    I realised it too late and the svn was updated so many times that i'll keep it 
    this way this time

v0.21 (12/Feb/2010)
    Lots of edits. Namely changed the way the number of cores were always fixed at 4
    Now arbitrary number of cores can be detected, thus i3, i5, i7 and gulftown (6-cores)
    can be detected. Also added code to detect the whole nehalem family rather than just i7

    Removed Intel_CPUID directory which was used earlier to know the number of logical
    and physical processor. It was iffy when cores where shut down in OS and when license
    was concerned. Now i use just /proc/cpuinfo to figure out stuff.

v0.2

    added a gui version that uses qt4. makefile in GUI subdirectory
    now C0+C1+C3+C6 = 100% 

v0.1

    simple i7 detection utility for overclockers and clockers :)

v0.01- very simple program to examine i7 feature clocking and running with speedstep
    Checked on 64-bit linux only. Should work on 32-bit too.

    need ncurses library: usually something like libncurses on debian. 
    also needs support of MSR (model specific register) in kernel. Usually most kernels
    have it. Else run the MAKEDEV file. I do modprobing of msr within the C-program.

  
  


coder: Abhishek Jaiantilal (abhishek.jaiantilal@colorado.edu). And suggestions/help from multiple people, let me know if i am missing your contribution.
contributor: raininja <daniel.mclellan@gmail.com>
contributor: Richard Hull <rm_hull@yahoo.co.uk>
contributor: andareed