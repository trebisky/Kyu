# Makefile for Kyu on the Orange Pi
#
# build board.o
#
# Tom Trebisky  1-6-2017

INCS = -I. -I..
include ../Makefile.inc

OBJS =  board_orange_pi.o \
	intcon_gic.o \
	gpio.o \
	serial.o \
	timer.o

all: ../board.o

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

clean:
	rm -f *.o
