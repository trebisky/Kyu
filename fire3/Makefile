# Makefile for Kyu on the Orange Pi
#
# build board.o
#
# Tom Trebisky  1-6-2017

INCS = -I. -I..
include ../Makefile.inc

OBJS =  board.o \
	multicore.o \
	serial.o \
	gpio.o \
	timer.o \
	emac.o \
	intcon_gic400.o

all: ../board.o

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

clean:
	rm -f *.o
