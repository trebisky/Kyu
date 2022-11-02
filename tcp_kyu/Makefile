# Makefile for Kyu on the ARM processor
#
# build tcp_kyu.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc

OBJS =  tcp.o

all: ../tcp_kyu.o

../tcp_kyu.o: $(OBJS)
	$(LD) -r -o ../tcp_kyu.o $(OBJS)

clean:
	rm -f *.o
