#ident "$Id: Makefile,v 1.2 2004/07/20 15:54:59 hpa Exp $"
## -----------------------------------------------------------------------
##
##   Copyright 2000 Transmeta Corporation - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for MSRs
#

#makefile updated from patch by anestling

#explicitly disable two scheduling flags as they cause segfaults, two more seem to crash the GUI version so putting them
#here 
CFLAGS_FOR_AVOIDING_SEG_FAULT = -fno-schedule-insns2  -fno-schedule-insns -fno-inline-small-functions -fno-caller-saves
CFLAGS ?= -O0 -g
CFLAGS += $(CFLAGS_FOR_AVOIDING_SEG_FAULT) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DBUILD_MAIN -Wimplicit-function-declaration

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
   CFLAGS += -Dx64_BIT
else
   CFLAGS += -Dx86
endif

CC       ?= gcc

LIBS  += -lncurses -lpthread -lrt -lm
INCLUDEFLAGS = 

OBJS = helper_functions

BIN	= i7z
# PERFMON-BIN = perfmon-i7z #future version to include performance monitor, either standalone or integrated
SRC	= i7z.c helper_functions.c i7z_Single_Socket.c i7z_Dual_Socket.c
OBJ	= $(SRC:.c=.o)

prefix = /usr
sbindir = $(prefix)/sbin
docdir = $(prefix)/share/doc/$(BIN)

all: clean test_exist

message:
	@echo "If the compilation complains about not finding ncurses.h, install ncurses (libncurses5-dev on ubuntu/debian)"

bin: message $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJ) $(LIBS)

#http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=644728 for -ltinfo on debian
static-bin: message $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJ) -static-libgcc -DNCURSES_STATIC -static -lpthread -lncurses -lrt -lm -ltinfo

# perfmon-bin: message $(OBJ)
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PERFMON-BIN) perfmon-i7z.c helper_functions.c $(LIBS)

test_exist: bin
	@test -f i7z && echo 'Succeeded, now run sudo ./i7z' || echo 'Compilation failed'

clean:
	rm -f *.o $(BIN)

distclean: clean
	rm -f *~ \#*

install: $(BIN)
	install -d $(DESTDIR)$(sbindir) $(DESTDIR)$(docdir)
	install -m 755 $(BIN) $(DESTDIR)$(sbindir)
	install -m 0644 README.txt put_cores_offline.sh put_cores_online.sh MAKEDEV-cpuid-msr $(DESTDIR)$(docdir)
