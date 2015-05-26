# Makefile for Kyu on the ARM processor
#
# build machine.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc


# locore must be first
OBJS =  locore.o hardware.o \
	wdt.o timer.o serial.o mux.o prm.o gpio.o cm.o \
	cpsw.o \
	interrupts.o intcon.o \
	cache_v7.o \
	show_regs.o \
	eabi_stubs.o

all: ../machine.o

../machine.o: $(OBJS)
	$(LD) -r -o ../machine.o $(OBJS)

clean:
	rm -f *.o
