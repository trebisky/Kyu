# Makefile for Kyu on the ARM processor
#
# build tcp_bsd
#
# Tom Trebisky  10-29-2022

BSD_NET3 = /u1/BSD/4.4BSD-Lite/usr/src
BSD_SRC = $(BSD_NET3)/sys

INCS = -I. -I..
INCS += -I$(BSD_SRC)

# To get time.h
# /u1/BSD/4.4BSD-Lite/usr/src/include/time.h
INCS += -I$(BSD_NET3)/include

DEFS += -DKERNEL

# I get the complaint: machine/endian.h: No such file or directory
#/u1/BSD/4.4BSD-Lite/usr/src/sys/i386/include/endian.h

# to solve this I need a link in the source directory from
# machine --> i386/include

include ../Makefile.inc

OBJS = kyu_bsd.o
#	tcp_debug.o \
#	tcp_input.o

all: ../tcp_bsd.o

../tcp_bsd.o: $(OBJS)
	$(LD) -r -o ../tcp_bsd.o $(OBJS)

clean:
	rm -f *.o

# --------------------------

get:
#	cp $(BSD_SRC)/netinet/tcp_debug.h .
#	cp $(BSD_SRC)/netinet/tcp_fsm.h .
#	cp $(BSD_SRC)/netinet/tcp.h .
#	cp $(BSD_SRC)/netinet/tcpip.h .
#	cp $(BSD_SRC)/netinet/tcp_seq.h .
#	cp $(BSD_SRC)/netinet/tcp_timer.h .
#	cp $(BSD_SRC)/netinet/tcp_var.h .
#
#	cp $(BSD_SRC)/netinet/tcp_debug.c .
#	cp $(BSD_SRC)/netinet/tcp_input.c .
#	cp $(BSD_SRC)/netinet/tcp_output.c .
#	cp $(BSD_SRC)/netinet/tcp_subr.c .
#	cp $(BSD_SRC)/netinet/tcp_timer.c .
#	cp $(BSD_SRC)/netinet/tcp_usrreq.c .

	chmod u+w *.h *.c
