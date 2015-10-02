# Makefile for Kyu on the ARM processor
#
# Tom Trebisky  4-15-2015

INCS = -I. -Iarm

include Makefile.inc

#subdirs = arm

# This is clean with no dependencies
#    linux/linux-lib.o \

#    linux/linux-i2c.o \
#    linux/linux-mm.o \

# For now, machine.o must come first since U-boot simply branches
# to 80300000 to start this running.

# from below
#    linux/linux-lib.o

OBJS =  machine.o net.o \
    main.o version.o user.o tests.o \
    console.o thread.o prf.o \
    shell.o \
    dlmalloc.o random.o \
    kyulib.o \
    linux/linux-lib.o

# For experimenting with linux imported stuff
#OBJS =  machine.o net.o \
#    linux/linux-lib.o \
#    linux/linux-mm.o \
#    main.o version.o user.o tests.o \
#    console.o thread.o prf.o \
#    dlmalloc.o random.o kyulib.o

all: install dump sym tags

dump: kyu.dump

sym: kyu.sym

.PHONY:	tags
tags:
	ctags -R

machine.o:	bogus
	cd arm ; make
	cd net ; make
	cd linux ; make

bogus:

dlmalloc.o:	malloc.h

thread.e:	thread.c
	$(CC) -E thread.c -o thread.e

#.PHONY: subdirs $(SUBDIRS)
#
#subdirs: $(SUBDIRS)
#
#$(SUBDIRS):
#	$(MAKE) -C $@

install: kyu.bin kyu.sym
	cp kyu.bin /var/lib/tftpboot/kyu.bin
	cp kyu.sym /var/lib/tftpboot/kyu.sym

kyu.dump:	kyu
	$(DUMP) kyu >kyu.dump

kyu.sym:	kyu
	$(NM) kyu >kyu.sym

version:
	$(CC) --version

help:
	$(CC) --help=optimizers

kyu:	kyu.lds

#	$(LD) -g -Ttext 0x80300000 -T kyu.lds -e kern_startup -o kyu $(OBJS) $(LIBS)

kyu: $(OBJS)
	$(LD) -g -T kyu.lds -e kern_startup -o kyu $(OBJS) $(LIBS)

kyu.bin: kyu
	$(BIN) kyu kyu.bin

clean:
	rm -f *.o *.s kyu kyu.bin kyu.dump kyu.sym
	cd arm ; make clean
	cd net ; make clean
	cd linux ; make clean
