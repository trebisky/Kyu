# Makefile for Kyu on the X86 processor
#
# build machine.o
#
# Tom Trebisky  9-17-2016

INCS = -I. -I..
include ../Makefile.inc

# locore must be first
OBJS =  locore.o \
	board.o \
	interrupts.o \
	timer.o \
	serial.o \
	terminal.o \
	ee100.o \
	rtl.o

#	das16.o
#	el3.o
#	if_ne.o
#	user.o
#	tests.o

all: ../machine.o

../machine.o: $(OBJS)
	$(LD) -r -o ../machine.o $(OBJS)

clean:
	rm -f *.o
