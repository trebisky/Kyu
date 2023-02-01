# Makefile for Kyu on the ARM processor
#
# build machine.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc

# locore must be first
OBJS =  locore.o \
	hardware.o \
	ram.o \
	interrupts.o \
	cache.o \
	cache_v7.o \
	mmu.o \
	show_regs.o \
	cpufunc_asm_armv7.o \
	eabi_stubs.o

OLDOBJ = xyz.o \
	armv7_cache.o \
	abc.o

all: ../machine.o

../machine.o: $(OBJS)
	$(LD) -r -o ../machine.o $(OBJS)

clean:
	rm -f *.o
