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

CFLAGSANY = -g -O0 -fomit-frame-pointer -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DBUILD_MAIN 

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
   CFLAGS = $(CFLAGSANY) -Dx64_BIT
else
   CFLAGS = $(CFLAGSANY) -Dx86
endif

CC       = gcc 

LDFLAGS  = -lncurses -lpthread -lrt
INCLUDEFLAGS = 

OBJS = helper_functions

BIN	= i7z
SRC	= i7z.c helper_functions.c i7z_Single_Socket.c i7z_Dual_Socket.c

sbindir = /usr/sbin

all: clean message bin

message:
	@echo "If the compilation complains about not finding ncurses.h, install ncurses (libncurses5-dev on ubuntu/debian)"

bin: clean
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)
	@test -f i7z && echo 'Succeeded, now run sudo ./i7z' || echo 'Compilation failed'

clean:
	rm -f *.o $(BIN)

distclean: clean
	rm -f *~ \#*

install: all
	install -m 755 $(BIN) $(sbindir)

