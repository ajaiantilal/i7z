By Abhishek Jaiantilal (abhishek.jaiantilal @@ colorado.edu)

how to run?
1. make 
2. sudo ./perfmon-i7z

what does it do?
this is a pre-alpha program to know what your machine is doing at your behest
or behind your back. 

it uses performance monitoring counters to understand what type of cycles are 
being run in the background, like int, floats or memory cycles. 

a hypothesis is that the number of cycles executed cause a proportional increase
in power consumption (depending on how much area does that unit occupies on the
chip die)

limitations (i can recall)
1. right now it runs on the first 4 cores, with a bit of coding it should be 
extendable to more cores/packages
2. works on a couple of my machine (4 core nehalem, 6 core SB) and mysteriously
doesn't work on a 4 core SB machine. i have to look into that
3. should be straightforward to extend on older machines like core2 which have a
slightly less number of PMC counters
4. i lost a backup in which i estimated cpu_frequency on the fly, this version has
it hardcoded, i dont know if that shows wrong values. i will have to look into that.

so why did this program come into being?
1. to prove that power and cycles executed have a direct relationship. ask anyone
who has run avx-linx on their SB machine, or linx on their machine!
2. i had trouble getting vtune/perfmon and other fancy tools to take least amount of
cpu usage while giving me the right values for the various counters.