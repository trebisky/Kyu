# Makefile for Kyu on the Orange Pi
#
# build board.o
#
# Tom Trebisky  1-6-2017

INCS = -I. -I..
include ../Makefile.inc

# A place to park things we aren't linking right now.
XX_OBJS = \
	h3_thermal.o \
	h3_sid.o \
	bogus.o

NOT_YET = \
	multicore.o

OBJS =  board.o \
	intcon_gic.o \
	gpio.o \
	serial.o \
	emac.o \
	timer.o \
	h3_spinlocks.o \
	ccu.o

all: ../board.o

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

clean:
	rm -f *.o
