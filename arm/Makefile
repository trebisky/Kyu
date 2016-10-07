# Makefile for Kyu on the ARM processor
#
# build machine.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc

#	phy.o smsc.o \

# locore must be first
OBJS =  locore.o hardware.o \
	wdt.o timer.o serial.o mux.o prcm.o gpio.o cm.o \
	pru.o i2c.o i2c_hw.o iic.o i2c_demos.o \
	if_cpsw.o \
	interrupts.o intcon.o \
	cache_v7.o \
	show_regs.o \
	eabi_stubs.o

all: ../machine.o

../machine.o: $(OBJS)
	$(LD) -r -o ../machine.o $(OBJS)

i2c_hw.o:	omap_ints.h
i2c.o:	i2c.h
iic.o:	i2c.h

if_cpsw.e:	if_cpsw.c
	$(CC) -E if_cpsw.c >if_cpsw.e

clean:
	rm -f *.o
