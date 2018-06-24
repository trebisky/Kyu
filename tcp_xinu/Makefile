# Makefile for Kyu on the ARM processor
#
# build tcp_xinu
#
# Tom Trebisky  1-10-2017

INCS = -I. -I..
include ../Makefile.inc

OBJS = mq.o \
    timer.o \
    tcbclear.o \
    tcbref.o \
    tcpabort.o \
    tcpack.o \
    tcpalloc.o \
    tcpcksum.o \
    tcpclose.o \
    tcpclosing.o \
    tcpcwait.o \
    tcpdata.o \
    tcpdisp.o \
    tcpestd.o \
    tcpfin1.o \
    tcpfin2.o \
    tcplastack.o \
    tcplisten.o \
    tcpnextseg.o \
    tcprto.o \
    tcpreset.o \
    tcpsendseg.o \
    tcpsynrcvd.o \
    tcpsynsent.o \
    tcptimer.o \
    tcptwait.o \
    tcpupdate.o \
    tcpwake.o \
    tcpxmit.o \
    tcp_init.o \
    tcp_in.o \
    tcp_out.o \
    tcp_register.o \
    tcp_send.o \
    tcp_recv.o \
    tcp_hton.o \
    kyu_test_tcp.o \
    kyu_glue.o

all: ../tcp_xinu.o

../tcp_xinu.o: $(OBJS)
	$(LD) -r -o ../tcp_xinu.o $(OBJS)

clean:
	rm -f *.o
