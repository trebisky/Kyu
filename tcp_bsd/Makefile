# Makefile for Kyu on the ARM processor
#
# build tcp_bsd
#
# Tom Trebisky  10-29-2022

INCS = -I. -I..
# INCS += -I$(BSD_SRC)

# Including ".." in the above lets us get the kyu include
# files - such as arch/cpu.h for htonl() and such.

# To get time.h ???
# /u1/BSD/4.4BSD-Lite/usr/src/include/time.h
#INCS += -I$(BSD_NET3)/include

DEFS += -DKERNEL

# I get the complaint: machine/endian.h: No such file or directory
#/u1/BSD/4.4BSD-Lite/usr/src/sys/i386/include/endian.h

# to solve this I need a link in the source directory from
# machine --> i386/include

include ../Makefile.inc


OBJS = kyu_main.o \
	kyu_subs.o \
	mbuf.o \
	tcp_debug.o \
	tcp_input.o \
	tcp_output.o \
	tcp_subr.o \
	tcp_timer.o \
	tcp_usrreq.o \
	in_pcb.o \
	in.o \
	tcp_cksum.o \
	uipc_mbuf.o \
	socket_io.o \
	socket_sb.o \
	test.o

all: ../tcp_bsd.o

../tcp_bsd.o: $(OBJS)
	$(LD) -r -o ../tcp_bsd.o $(OBJS)

clean:
	rm -f *.o

# --------------------------

BSD_NET3 = /u1/BSD/4.4BSD-Lite/usr/src
BSD_SRC = $(BSD_NET3)/sys

# Use this to copy, then comment out the lines (or delete them)
# otherwise you will overwrite any changes.

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

#	cp $(BSD_SRC)/netinet/in_pcb.c .
#	cp $(BSD_SRC)/netinet/in.c .

#	cp $(BSD_SRC)/netinet/in_pcb.h .
#	cp $(BSD_SRC)/netinet/in.h .
#	cp $(BSD_SRC)/netinet/in_systm.h .
#	cp $(BSD_SRC)/netinet/ip.h .
#	cp $(BSD_SRC)/netinet/ip_var.h .
#	cp $(BSD_SRC)/netinet/ip_icmp.h .
#	cp $(BSD_SRC)/netinet/in_var.h .

#	cp $(BSD_SRC)/netinet/if_ether.h .

#	cp  $(BSD_SRC)/sys/types.h sys
#	cp  $(BSD_SRC)/sys/mbuf.h sys
#	cp  $(BSD_SRC)/sys/param.h sys
#	cp  $(BSD_SRC)/sys/systm.h sys
#	cp  $(BSD_SRC)/sys/socketvar.h sys
#	cp  $(BSD_SRC)/sys/socket.h sys
#	cp  $(BSD_SRC)/sys/cdefs.h sys
#	cp  $(BSD_SRC)/sys/syslimits.h sys
#	cp  $(BSD_SRC)/sys/errno.h sys
#	cp  $(BSD_SRC)/sys/time.h sys
#	cp  $(BSD_SRC)/sys/resource.h sys
#	cp  $(BSD_SRC)/sys/ucred.h sys
#	cp  $(BSD_SRC)/sys/uio.h sys
#	cp  $(BSD_SRC)/sys/signal.h sys
#	cp  $(BSD_SRC)/sys/malloc.h sys
#	cp  $(BSD_SRC)/sys/select.h sys
#	cp  $(BSD_SRC)/sys/protosw.h sys
#	cp  $(BSD_SRC)/sys/stat.h sys
#	cp  $(BSD_SRC)/sys/sockio.h sys

# -- for uipc_mbuf.h
#	cp  $(BSD_SRC)/sys/domain.h sys

#	cp  $(BSD_SRC)/net/if.h net
#	cp  $(BSD_SRC)/net/if_arp.h net
#	cp  $(BSD_SRC)/net/route.h net
#	cp  $(BSD_SRC)/net/radix.h net

# machine is a gross hack (link to i386/include)
#	cp  $(BSD_SRC)/machine/endian.h machine
#	cp  $(BSD_SRC)/machine/ansi.h machine
#	cp  $(BSD_SRC)/machine/types.h machine
#	cp  $(BSD_SRC)/machine/signal.h machine
#	cp  $(BSD_SRC)/machine/trap.h machine
#	cp  $(BSD_SRC)/machine/param.h machine
#	cp  $(BSD_SRC)/machine/limits.h machine

#	cp  $(BSD_SRC)/i386/i386/in_cksum.c tcp_cksum.c

#	cp  $(BSD_SRC)/libkern/libkern.h libkern

#	cp  $(BSD_SRC)/kern/uipc_mbuf.c .
#	cp  $(BSD_SRC)/kern/uipc_socket.c ./socket_io.c

	chmod u+w *.h *.c
	chmod u+w sys/*
	chmod u+w net/*
	chmod u+w machine/*
	chmod u+w libkern/*

# THE END
