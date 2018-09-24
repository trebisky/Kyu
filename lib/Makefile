# Makefile for Kyu on the ARM processor
#
# build lib.o
#
# Tom Trebisky  9-30-2016

INCS = -I. -I..

include ../Makefile.inc

OBJS =  string.o ctype.o vsprintf.o string_helpers.o \
	hexdump.o kstrtox.o div64.o

# XXX do we really need these ??
#	findbit.o \
#	div64asm.o

#AOPTS = -D__ASSEMBLY__ -include asm/unified.h
AOPTS = -D__ASSEMBLY__

all: ../lib.o

../lib.o: $(OBJS)
	$(LD) -r -o ../lib.o $(OBJS)

clean:
	rm -f *.o

# $(CC) $(AOPTS) -o $@ -c $<
#div64asm.o:	div64asm.S

div64asm.o:	arm/div64asm.S
	$(CC) $(AOPTS) -o $@ -c $<

findbit.o:	arm/findbit.S
	$(CC) $(AOPTS) -o $@ -c $<

# incomplete
string.o:	ctype.h
vsprintf.o:	string_helpers.h linux_headers.h
kstrtox.o:	ctype.h

