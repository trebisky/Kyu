# Makefile for Kyu on the ARM processor
#
# build net.o
#
# Tom Trebisky  5-18-2015

INCS = -I. -I..
include ../Makefile.inc

#	net_tcp.o \

OBJS =  net_main.o \
	in_cksum.o \
	net_arp.o net_icmp.o net_ip.o \
	net_udp.o dns.o \
	dhcp.o bootp.o tftp.o

all: ../net.o

../net.o: $(OBJS)
	$(LD) -r -o ../net.o $(OBJS)

clean:
	rm -f *.o
