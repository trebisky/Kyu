# Makefile for Kyu on the ARM processor
#
# Tom Trebisky  4-15-2015

# Note that I copied stdarg.h from here.
# I probably should arrange so the compiler looks
# here by default.
#  /usr/lib/gcc/arm-linux-gnueabi/4.9.1/include/stdarg.h

INCS = -I. -Iarm

include Makefile.inc

# For now, machine.o must come first since U-boot simply branches
# to 80300000 to start this running.
OBJS =  machine.o \
    main.o version.o user.o tests.o \
    console.o thread.o prf.o \
    random.o kyulib.o

all: install tags

dump: kyu.dump

syms: kyu.syms

.PHONY:	tags
tags:
	ctags -R

machine.o:
	cd arm ; make

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

kyu: $(OBJS)
	$(LD) -g -Ttext 0x80300000 -e kern_startup -o kyu $(OBJS) $(LIBS)

kyu.bin: kyu
	$(BIN) kyu kyu.bin

clean:
	rm -f *.o *.s kyu kyu.bin kyu.dump kyu.syms
	cd arm ; make clean
