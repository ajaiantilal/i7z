By Abhishek Jaiantilal
Under GPL v2



#to make the gui working you will need qt4 installed. 
libqt4-dev, qmake-qt4 should be enough i think

running the Makefile should be enough

svn-r43
Wworks for Dual Socket Boards but wont work if core is taken offline while the tool is running.

Right way of compiling is the following 3 steps

qmake
make clean
make