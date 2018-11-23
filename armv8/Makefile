# Makefile for Kyu on the ARM processor
#
# build machine.o
#
# Tom Trebisky  9-23-2018

INCS = -I. -I..
include ../Makefile.inc

# startup must be first
OBJS =  startup.o \
	hardware.o \
	interrupts.o \
	ram.o \
	cache.o \
	cache_v8.o \
	mmu.o \
	show_regs.o

OBJS_V7 =  locore.o \
	hardware.o \
	ram.o \
	interrupts.o \
	armv7_cache.o \
	cache.o \
	cache_v7.o \
	mmu.o \
	show_regs.o \
	eabi_stubs.o

all: ../machine.o

../machine.o: $(OBJS)
	$(LD) -r -o ../machine.o $(OBJS)

clean:
	rm -f *.o
