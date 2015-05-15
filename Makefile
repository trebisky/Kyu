# Makefile for simple standalone application
# to run under U-boot  Tom Trebisky  4-15-2015

# Note that I copied stdarg.h from here.
# I probably should arrange so the compiler looks
# here by default.
#  /usr/lib/gcc/arm-linux-gnueabi/4.9.1/include/stdarg.h

CCX = arm-linux-gnu-gcc
#COPTS = -DKYU -nostdinc -fno-builtin -Os -marm -march=armv7-a -msoft-float -I.
COPTS = -DKYU -nostdinc -fno-builtin -marm -march=armv7-a -msoft-float -I.
CC = $(CCX) $(COPTS)

# It is a bug that we cannot invoke gcc to do this,
# but doing this directly gets the job done.
LD = arm-linux-gnu-ld.bfd

LIBS = -L /usr/lib/gcc/arm-linux-gnueabi/4.9.2 -lgcc

BIN = arm-linux-gnu-objcopy -O binary
DUMP = arm-linux-gnu-objdump -d
NM = arm-linux-gnu-nm

# For now, locore.o must come first since U-boot simply branches
# to 80300000 to start this running.
OBJS =  locore.o \
    interrupts.o show_regs.o \
    main.o version.o user.o tests.o \
    console.o hardware.o thread.o \
    intcon.o timer.o wdt.o prm.o serial.o gpio.o mux.o prf.o \
    random.o kyulib.o eabi_stubs.o

all: install tags

dump: kyu.dump

syms: kyu.syms

.PHONY:	tags
tags:
	ctags *.S *.h *.c

install: kyu.bin
	cp kyu.bin /var/lib/tftpboot/hello.bin

kyu.dump:	kyu
	$(DUMP) kyu >kyu.dump

kyu.syms:	kyu
	$(NM) kyu >kyu.syms

version:
	$(CC) --version

help:
	$(CC) --help=optimizers

.c.o:
	$(CC) -c $<

.c.s:
	$(CC) -S $<

kyu: $(OBJS)
	$(LD) -g -Ttext 0x80300000 -e kern_startup -o kyu $(OBJS) $(LIBS)

kyu.bin: kyu
	$(BIN) kyu kyu.bin

clean:
	rm -f *.o *.s kyu kyu.bin kyu.dump kyu.syms
