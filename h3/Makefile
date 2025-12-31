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
	dallas_opt.o \
	bogus.o

OBJS =  board.o \
	intcon_gic.o \
	gpio.o \
	serial.o \
	emac.o \
	emac_phy.o \
	dallas.o \
	i2c_lcd.o \
	i2c_lcd2.o \
	i2c_hdc1008.o \
	i2c_dac.o \
	i2c.o \
	i2c_hw.o \
	timer.o \
	spinlocks.o \
	ccu.o \
	multicore.o

all: ../board.o

dallas_opt.o: dallas.c
	$(OCC) -o $@ -c $<

../board.o: $(OBJS)
	$(LD) -r -o ../board.o $(OBJS)

clean:
	rm -f *.o
