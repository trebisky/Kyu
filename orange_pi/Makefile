# Makefile for Kyu on the Orange Pi
#
# build board.o
#
# Tom Trebisky  1-6-2017

INCS = -I. -I..
include ../Makefile.inc

OBJS =  board.o \
	intcon_gic.o \
	gpio.o \
	serial.o \
	timer.o \
	spinlocks.o \
	cores.o \
	smp.o

all: ../board.o

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

clean:
	rm -f *.o
