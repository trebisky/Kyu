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

extern u32 loopback_ip;

static int ip_debug = 0;

void
set_ip_debug ( int val )
{
	ip_debug = val;
}

static char *
proto_name ( int proto )
{
	static char buf[8];

	if ( proto == IPPROTO_ICMP )
	    return "icmp";
	if ( proto == IPPROTO_UDP )
	    return "udp";
	if ( proto == IPPROTO_TCP )
	    return "tcp";
	sprintf ( buf, "%d", proto );
	return buf;
}

/* Called for every packet with our mac address and with ether type IP
 */
void
ip_rcv ( struct netbuf *nbp )
{
	struct ip_hdr *ipp;
	int cksum;

	ipp = nbp->iptr;
	cksum = in_cksum ( nbp->iptr, sizeof(struct ip_hdr) );

	if ( ip_debug ) {
	    printf ( "ip_rcv - packet from %s (%d) proto = %s, sum= %04x\n",
		ip2str32 ( ipp->src ), nbp->ilen, proto_name(ipp->proto), cksum );
	    printf ( "eth: " );
	    dump_buf ( nbp->eptr, 14 );
	    printf ( " --\n" );
	    dump_buf ( nbp->iptr, nbp->ilen );
	}

	// cksum = in_cksum ( nbp->iptr, sizeof(struct ip_hdr) );
	// ipp = nbp->iptr;

	if ( cksum ) {
	    printf ( "bad IP packet from %s (%d) proto = %d, sum= %04x\n",
		    ip2str32 ( ipp->src ), nbp->ilen, ipp->proto, cksum );
	    netbuf_free ( nbp );
	    return;
	}

	/* We aren't handling fragmented IP packets.
	 * I feel sort of guilty, but we skipped this "for now"
	 */
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
	    // printf ( "Sizeof eth = %d\n", sizeof(struct eth_hdr) );
	    // printf ( "Sizeof ip = %d\n", sizeof(struct ip_hdr) );
	    tcp_rcv ( nbp );
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

	if ( ! valid_ram_address ( nbp ) ) {
	    printf ( "ip_send, nbp = %08x\n", nbp );
	    netbuf_show ();
	    panic ( "ip_send = bad netbuf addr\n" );
	}


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
	/* XXX - this may not work out well for TCP as
	 * these are included in the TCP checksum.
	 * As long as this matches what BSD set, OK.
	 */

	ipp->src = host_info.my_ip;
	ipp->dst = dest_ip;

	ipp->sum = 0;
	ipp->sum = in_cksum ( (char *) ipp, sizeof ( struct ip_hdr ) );

	if ( ip_debug ) {
	    printf ( "IP ip_send - ready to go: ...\n" );
	    dump_buf ( (char *) ipp, 20 );
	}

	if ( dest_ip == loopback_ip ) {
	    // Must do this or our IP code will toss the packet
	    memcpy ( nbp->eptr->dst, host_info.our_mac, ETH_ADDR_SIZE );
	    // Must do this also
	    nbp->eptr->type = ETH_IP_SWAP;
	    // This too
	    nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	    printf ( "Sending IP packet to loopback\n" );
	    printf ( " ilen = %d\n", nbp->ilen );
	    net_rcv_noint ( nbp );
	    return;
	}

	/* Send over the wire */
	ip_arp_send ( nbp );
}

/* THE END */
