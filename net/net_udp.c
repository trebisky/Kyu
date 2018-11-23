/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * net_udp.c
 * Handle an UDP packet.
 * T. Trebisky  4-11-2005
 */

#include <arch/types.h>
#include <kyu.h>
#include <kyulib.h>
#include <malloc.h>
#include <thread.h>

#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

/* 8 bytes */
struct udp_hdr {
	unsigned short	sport;
	unsigned short	dport;
	/* - */
	unsigned short len;
	unsigned short sum;
};

extern struct host_info host_info;

struct udp_proto {
	struct udp_proto *next;
	int port;
	ufptr func;
};

static struct udp_proto *head = (struct udp_proto *) 0;

/* 8-17-2016 - we really need this to avoid a race between
 * unhook and checking the table during packet reception.
 */
static struct sem *udp_sem;

#define UDP_LOCK	sem_block ( udp_sem )
#define UDP_UNLOCK	sem_unblock ( udp_sem )

void
udp_init ( void )
{
	udp_sem = sem_mutex_new ( SEM_FIFO );
}

void
udp_hookup ( int port, ufptr func )
{
	struct udp_proto *pp;

	UDP_LOCK;
	/* replace any existing entry (untested) */
	for ( pp = head; pp; pp = pp->next ) {
	    if ( pp->port == port ) {
		pp->func = func;
		UDP_UNLOCK;
		return;
	    }
	}

	pp = (struct udp_proto *) malloc ( sizeof(struct udp_proto) );
	pp->port = port;
	pp->func = func;
	pp->next = head;
	head = pp;
	UDP_UNLOCK;
}

#ifdef notdef
void
udp_showl ( void )
{
	struct udp_proto *pp;

	printf ( "UDP head: %08x\n", head );
	for ( pp = head; pp; pp = pp->next )
	    printf ( "UDP port %d\n", pp->port );
}
#endif

void
udp_unhook ( int port )
{
	struct udp_proto *pp;
	struct udp_proto *prior;
	struct udp_proto *save;

	// printf ( "UDP unhook for %d\n", port );
	if ( ! head )
	    return;

	UDP_LOCK;
	if ( head->port == port ) {
	    save = head;
	    head = save->next;
	    free ( save );
	    UDP_UNLOCK;
	    //udp_showl ();
	    return;
	}

	prior = head;
	for ( pp = prior->next; pp; pp = prior->next ) {
	    if ( pp->port == port ) {
		prior->next = pp->next;
		free ( pp );
		UDP_UNLOCK;
		//udp_showl ();
		return;
	    }
	    prior = pp;
	}

	UDP_UNLOCK;
}

int
udp_get_sport ( struct netbuf *nbp )
{
	struct udp_hdr *udp;

	udp = (struct udp_hdr *) nbp->pptr;
	return ntohs ( udp->sport );
}

void
udp_rcv ( struct netbuf *nbp )
{
	struct udp_hdr *udp;
	struct udp_phdr *psp;
	int sum;
	char *cptr;
	struct udp_proto *pp;
	int port;

	udp = (struct udp_hdr *) nbp->pptr;
	nbp->dlen = nbp->plen - sizeof(struct udp_hdr);
	nbp->dptr = nbp->pptr + sizeof(struct udp_hdr);

#ifdef DEBUG_UDP
	printf ( "UDP from %s: src/dst = %d/%d\n",
		ip2str32 ( nbp->iptr->src ), ntohs(udp->sport), ntohs(udp->dport) );
#endif

	/* XXX - validate checksum of received packets
	 */

	port = ntohs(udp->dport);

	pkt_dispatch ();

	// printf ( "UDP receive for port %d\n", port );
	// udp_showl ();

	UDP_LOCK;
	for ( pp = head; pp; pp = pp->next ) {
	    if ( port == pp->port )
		break;
	}
	UDP_UNLOCK;

	if ( pp )
	    ( *pp->func ) ( nbp );
}

struct bogus_ip {
    	u32 x1;
    	u32 x2;
	/* */
	unsigned char x3;
	unsigned char proto;
	unsigned short len;
	/* */
	u32 src;
	u32 dst;
};

void
udp_send ( u32 dest_ip, int sport, int dport, char *buf, int size )
{
	struct udp_hdr *udp;
	struct netbuf *nbp;
	struct bogus_ip *bip;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct udp_hdr );

	memcpy ( nbp->dptr, buf, size );

	size += sizeof(struct udp_hdr);
	nbp->plen = size;
	nbp->ilen = size + sizeof(struct ip_hdr);

	udp = (struct udp_hdr *) nbp->pptr;
	udp->sport = htons(sport);
	udp->dport = htons(dport);
	udp->len = htons(nbp->plen);
	udp->sum = 0;

	/* Fill out a bogus IP header just to do
	 * checksum computation.  It will pick up
	 * the proto field from this however.
	 */
	bip = (struct bogus_ip *) nbp->iptr;
	memset ( (char *) bip, 0, sizeof(struct bogus_ip) );

	bip->proto = IPPROTO_UDP;
	bip->len = udp->len;
	bip->dst = dest_ip;
	bip->src = host_info.my_ip;

	/*
	udp->sum = in_cksum ( nbp->iptr, nbp->ilen );
	*/
	udp->sum = ~in_cksum_i ( nbp->iptr, nbp->ilen, 0 );

	ip_send ( nbp, dest_ip );
}

/* This is a kind of funky hack to be sure the source IP gets set to
 *  zero for a broadcast, even if we know our IP (or think we do).
 */
void
udp_broadcast ( int sport, int dport, char *buf, int size )
{
	unsigned int save;

	/* XXX - maybe Kyu needs an "ip_broadcast" to avoid this hack.
	 */
	save = host_info.my_ip;
	host_info.my_ip = 0;

	udp_send ( IP_BROADCAST, sport, dport, buf, size );

	host_info.my_ip = save;
}

/* THE END */
