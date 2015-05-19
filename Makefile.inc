# Makefile.inc
# 
# These are the common definitions for building on the ARM processor
#
# Tom Trebisky  5-18-2015

# Note that I copied stdarg.h from here.
# I probably should arange so the compiler looks
# here by default.
#  /usr/lib/gcc/arm-linux-gnueabi/4.9.1/include/stdarg.h

CCX = arm-linux-gnu-gcc
COPTS = -DKYU -nostdinc -fno-builtin -Os -marm -march=armv7-a -msoft-float $(INCS)
#COPTS = -DKYU -nostdinc -fno-builtin -marm -march=armv7-a -msoft-float $(INCS)
CC = $(CCX) $(COPTS)

# It is a bug that we cannot invoke gcc to do this,
# but doing this directly gets the job done.
LD = arm-linux-gnu-ld.bfd

LIBS = -L /usr/lib/gcc/arm-linux-gnueabi/4.9.2 -lgcc

BIN = arm-linux-gnu-objcopy -O binary
DUMP = arm-linux-gnu-objdump -d
NM = arm-linux-gnu-nm

.c.o:
	$(CC) -c $<

.c.s:
	$(CC) -S $<