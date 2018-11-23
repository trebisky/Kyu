/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * net_ip
 * Handle an IP packet.
 * T. Trebisky  3-21-2005
 */

#include <arch/types.h>
#include <kyu.h>
#include <kyulib.h>

#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

extern struct host_info host_info;

void
ip_rcv ( struct netbuf *nbp )
{
	struct ip_hdr *ipp;
	int cksum;

	cksum = in_cksum ( nbp->iptr, sizeof(struct ip_hdr) );
	ipp = nbp->iptr;

	if ( cksum ) {
	    printf ( "bad IP packet from %s (%d) proto = %d, sum= %04x\n",
		    ip2str32 ( ipp->src ), nbp->ilen, ipp->proto, cksum );
	    netbuf_free ( nbp );
	    return;
	}

	if ( ipp->offset & IP_OFFMASK_SWAP ) {
	    printf ( "Fragmented (%04x) IP packet from %s (%d) proto = %d, sum= %04x\n",
		    ipp->offset, ip2str32 ( ipp->src ), nbp->ilen, ipp->proto, cksum );
	    netbuf_free ( nbp );
	    return;
	}

	nbp->pptr = (char *) nbp->iptr + sizeof(struct ip_hdr);
	nbp->plen = nbp->ilen - sizeof(struct ip_hdr);

// #define DEBUG_THIS

#ifdef DEBUG_THIS

	printf ( " IP packet: Source: %s", ip2str32 ( ipp->src ) );
	printf ( "   " );
	printf ( "Dest: %s", ip2str32 ( ipp->dst ) );
	if ( ipp->proto == IPPROTO_ICMP )
	    printf ( " ICMP" );
	else if ( ipp->proto == IPPROTO_UDP ) {
	    struct udp_hdr {
		unsigned short  sport;
		unsigned short  dport;
		/* - */
		unsigned short len;
		unsigned short sum;
	    } *udp;

	    udp = (struct udp_hdr *) nbp->pptr;
	    printf ( " UDP src/dst = %d/%d", ntohs(udp->sport), ntohs(udp->dport) );
	    printf ( " size = %d", nbp->plen );

	} else if ( ipp->proto == IPPROTO_TCP )
	    printf ( " TCP" );
	else
	    printf ( "Proto: %d", ipp->proto );
	printf ( "\n" );
#endif

	/* XXX - should verify that it is addressed to us, or to a broadcast */

	if ( ipp->proto == IPPROTO_ICMP ) {
	    // printf ( "ip_rcv: ilen, plen, len = %d %d %d (ICMP)\n", nbp->ilen, nbp->plen, ntohs(ipp->len) );
	    // dump_buf ( nbp->iptr, nbp->ilen );
	    icmp_rcv ( nbp );
	    /* Don't free the net_buf here */
	} else if ( ipp->proto == IPPROTO_UDP ) {
	    udp_rcv ( nbp );
	    netbuf_free ( nbp );
	} else if ( ipp->proto == IPPROTO_TCP ) {
#ifdef WANT_TCP_XINU
	    tcp_xinu_rcv ( nbp );
	    netbuf_free ( nbp );
#else
	    tcp_rcv ( nbp );
	    netbuf_free ( nbp );
#endif
	} else {
	    printf ( "IP from %s (size:%d) proto = %d, sum= %04x\n",
		    ip2str32 ( ipp->src ), nbp->plen, ipp->proto, ipp->sum );
	    netbuf_free ( nbp );
	}

#ifdef DEBUG_IP
	if ( ipp->proto == IPPROTO_ICMP ) {
	    printf ( "ICMP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else if ( ipp->proto == IPPROTO_UDP ) {
	    printf ( "UDP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else if ( ipp->proto == IPPROTO_TCP ) {
	    printf ( "TCP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else {
	    printf ( "IP packet (size:%d) proto = %d, sum= %04x", nbp->plen, ipp->proto, ipp->sum );
	}

	printf ( " Source: %s", ip2str32 ( ipp->src ) );
	printf ( "   " );
	printf ( "Dest: %s", ip2str32 ( ipp->dst ) );
	printf ( "\n" );
#endif
}

/* typically called to return an icmp packet.
 * (also to do a RST/ACK on a tcp connection.)
 * We take some significant short cuts here,
 *  recycling the netbuf already received.
 */
void
ip_reply ( struct netbuf *nbp )
{
	struct ip_hdr *ipp;
	u32 tmp;

	ipp = (struct ip_hdr *) nbp->iptr;

	tmp = ipp->src;
	ipp->src = ipp->dst;
	ipp->dst = tmp;

	ip_arp_send ( nbp );
}

static int ip_id = 1;

void
ip_send ( struct netbuf *nbp, u32 dest_ip )
{
	struct ip_hdr *ipp;

	// changed for Xinu TCP
	// ipp = (struct ip_hdr *) nbp->iptr;
	ipp = (struct ip_hdr *) ((char *) nbp->eptr + sizeof ( struct eth_hdr ));

	/* XXX - why redo this here ?? */
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	// printf ( "IP SEND: ilen = %d\n", nbp->ilen );

	ipp->hl = sizeof(struct ip_hdr) / sizeof(u32);
	ipp->ver = 4;

	ipp->tos = 0;
	// printf ( "***LEN: %04x %04x\n", ipp->len, nbp->ilen );
	ipp->len = htons(nbp->ilen);

	ipp->id = ip_id++;
	ipp->offset = 0;

	ipp->ttl = 64;

	/* proto is already filled in by caller */

	ipp->src = host_info.my_ip;
	ipp->dst = dest_ip;

	ipp->sum = 0;
	ipp->sum = in_cksum ( (char *) ipp, sizeof ( struct ip_hdr ) );

	ip_arp_send ( nbp );
}

/* THE END */
