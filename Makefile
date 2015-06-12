# Makefile for Kyu on the ARM processor
#
# Tom Trebisky  4-15-2015

INCS = -I. -Iarm

include Makefile.inc

#subdirs = arm

#    linux/linux-i2c.o \
#    linux/linux-mm.o \

# For now, machine.o must come first since U-boot simply branches
# to 80300000 to start this running.
OBJS =  machine.o net.o \
    linux/linux-lib.o \
    linux/linux-i2c.o \
    main.o version.o user.o tests.o \
    console.o thread.o prf.o \
    dlmalloc.o random.o kyulib.o

all: install dump tags

dump: kyu.dump

syms: kyu.syms

.PHONY:	tags
tags:
	ctags -R

machine.o:	bogus
	cd arm ; make
	cd net ; make
	cd linux ; make

bogus:

dlmalloc.o:	malloc.h

#.PHONY: subdirs $(SUBDIRS)
#
#subdirs: $(SUBDIRS)
#
#$(SUBDIRS):
#	$(MAKE) -C $@

install: kyu.bin
	cp kyu.bin /var/lib/tftpboot/kyu.bin

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
	cd net ; make clean
	cd linux ; make clean
