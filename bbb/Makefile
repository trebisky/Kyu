# Makefile for Kyu on the BBB (Beagle Board Black)
#
# build board.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc

OBJS =  board.o \
	intcon.o \
	gpio.o mux.o \
	timer.o wdt.o dma.o \
	cm.o prcm.o \
	i2c.o i2c_hw.o iic.o i2c_demos.o \
	serial.o \
	cpsw.o \
	spi.o pru.o adc.o

all: ../board.o

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

adc.o:		omap_ints.h

i2c_hw.o:	omap_ints.h
i2c.o:	i2c.h
iic.o:	i2c.h

#cpsw.e:	cpsw.c
#	$(CC) -E cpsw.c >cpsw.e

clean:
	rm -f *.o
