/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * net_arp
 * Handle an ARP packet.
 * T. Trebisky  3-21-2005
 */

/* XXX - Todo
 * expire arp cache entries (20 minutes),
 * (BSD expires incomplete entries in 3 minutes)
 * issue and wait for arp requests
 * (queue of waiting packets to go out).
 * IF we get a reply to our Grat. arp, we should
 * issue a "duplicate IP in use" message with the
 * MAC address of the offender.
 */

/* ARP messages are 42 bytes, padded to 60 for ethernet.
 * 14 bytes for ether header, 28 bytes for the following:
 */

#include <arch/types.h>
#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

// #define DEBUG_ARP
// #define DEBUG_ARP_MUCHO

#define ARP_SIZE	28
#define ARP_MIN		60

static unsigned char broad[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static unsigned char zeros[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* GCC by default wants to align structure elements on 4 byte
 * boundaries.  This leads to trouble with the alignment of
 * spa and tpa in the following structure.
 * Using "pragma pack" fixes this.
 */
#pragma pack(2)
struct eth_arp {
	unsigned short hfmt;
	unsigned short pfmt;
	unsigned char hlen;
	unsigned char plen;
	unsigned short op;
	unsigned char sha[ETH_ADDR_SIZE];
	// unsigned char spa[4];
	u32 spa;
	unsigned char tha[ETH_ADDR_SIZE];
	// unsigned char tpa[4];
	u32 tpa;
};
#pragma pack()

#define OP_REQ		1
#define OP_REPLY	2

#define OP_REQ_SWAP	0x0100
#define OP_REPLY_SWAP	0x0200

extern struct host_info host_info;

static struct sem *arp_ping_sem;

#define MAX_ARP_CACHE	32

#define F_PENDING	0x0001
#define F_PING		0x0002
#define F_PERM		0x0004
#define F_VALID		0x0008

struct arp_data {
	int flags;
	int ttl;
	unsigned int ip_addr;
	char ether[ETH_ADDR_SIZE];
	struct netbuf *outq;
} arp_cache[MAX_ARP_CACHE];

// static int arp_save ( char *, unsigned char * );
// void arp_save_icmp ( char *, unsigned char * );
static int arp_save ( char *, u32 );
void arp_save_icmp ( char *, u32 );

void arp_show ( void );
void arp_reply ( struct netbuf * );
static struct arp_data * arp_alloc ( void );

static void
arp_show_stuff ( char *str, struct eth_arp *eap )
{
	printf ( "%s\n", str );

	printf ( "Source: %s", ether2str(eap->sha) );
	// printf ( " (%s) ", ip2str ( eap->spa ) );
	printf ( " (%s) ", ip2str ( (char *) &eap->spa ) );

	printf ( "Target: %s", ether2str(eap->tha) );
	// printf ( " (%s)\n", ip2str ( eap->tpa ) );
	printf ( " (%s)\n", ip2str ( (char *) &eap->tpa ) );
}

struct arp_data *
arp_lookup_e ( u32 ip_addr )
{
	int i;
	struct arp_data *ap;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ap->ip_addr == ip_addr )
		return ap;
	}

	return (struct arp_data *) 0;
}

/* -------------------------- */

void
arp_rcv ( struct netbuf *nbp )
{
	struct eth_arp *eap;

	eap = (struct eth_arp *) nbp->iptr;

#ifdef DEBUG_ARP_MUCHO
	if ( eap->op == OP_REQ_SWAP ) {
	    arp_show_stuff ( "ARP request received ", eap );
	    arp_show ();
	} else {
	    arp_show_stuff ( "  ARP reply received ", eap );
	}
#endif

	if ( eap->tpa != host_info.my_ip ) {
	    netbuf_free ( nbp );
	    return;
	}

#ifdef DEBUG_ARP
	if ( eap->op == OP_REQ_SWAP ) {
	    arp_show_stuff ( "ARP request received ", eap );
	} else {
	    arp_show_stuff ( "  ARP reply received ", eap );
	}
#endif

	if ( eap->op == OP_REQ_SWAP ) {
	    (void) arp_save ( eap->sha, eap->spa );
	    /* XXX - we recycle the packet to return the reply
	     * -- so be careful about how the netbuf is disposed of.
	     */
	    arp_reply ( nbp );
	} else {
	    (void) arp_save ( eap->sha, eap->spa );
	    netbuf_free ( nbp );
	}
}

void
arp_reply ( struct netbuf *nbp )
{
	struct eth_arp *eap;

	/* This isn't an IP packet at all, so force
	 * this pointer to an ARP header
	 */
	eap = (struct eth_arp *) nbp->iptr;

	// memcpy ( eap->tpa, eap->spa, 4 );
	eap->tpa = eap->spa;
	memcpy ( eap->tha, eap->sha, ETH_ADDR_SIZE ); 
	// memcpy ( eap->spa, (char *) &host_info.my_ip, 4 );
	eap->spa = host_info.my_ip;
	eap->op = OP_REPLY_SWAP;

	net_addr_get ( eap->sha );

	nbp->eptr->type = ETH_ARP_SWAP;
	memcpy ( nbp->eptr->dst, eap->tha, ETH_ADDR_SIZE );

	net_send ( nbp );
}

#define ARP_ETHER_SWAP	0x0100

void
arp_request ( u32 target_ip )
{
	struct netbuf *nbp;
	struct eth_arp *eap;
	// u32 unknown = target_ip;

	/* get a netbuf for this */
	if ( ! (nbp = netbuf_alloc ()) )
	    return;

	eap = (struct eth_arp *) nbp->iptr;

	eap->hfmt = ARP_ETHER_SWAP;
	eap->pfmt = ETH_IP_SWAP;
	eap->hlen = ETH_ADDR_SIZE;
	eap->plen = 4;

	net_addr_get ( eap->sha );
	// memcpy ( eap->spa, (char *) &host_info.my_ip, 4 );
	eap->spa = host_info.my_ip;

	memcpy ( eap->tha, zeros, ETH_ADDR_SIZE ); 
	// memcpy ( eap->tpa, (char *) &unknown, 4 );
	// eap->tpa =  unknown;
	eap->tpa = target_ip;;
	eap->op = OP_REQ_SWAP;

	nbp->eptr->type = ETH_ARP_SWAP;
	memcpy ( nbp->eptr->dst, broad, ETH_ADDR_SIZE );
	nbp->ilen = sizeof ( struct eth_arp );

	net_send ( nbp );
}

/* Called from the IP layer when we have a packet to send */
void
ip_arp_send ( struct netbuf *nbp )
{
	struct arp_data *ap;
	u32 dest_ip = nbp->iptr->dst;

	nbp->eptr->type = ETH_IP_SWAP;

	/* broadcast is easy */
	if ( dest_ip == IP_BROADCAST ) {
	    memcpy ( nbp->eptr->dst, broad, ETH_ADDR_SIZE );
	    net_send ( nbp );
	    return;
	}

	/*  XXX - here is the sum total of our IP routing.
	 * maybe this should be going to the gateway.
	 */
	if ( (dest_ip & host_info.net_mask) != host_info.my_net ) {
	    dest_ip = host_info.gate_ip;
	}

	ap = arp_lookup_e ( dest_ip );
	if ( ap ) {
	    memcpy ( nbp->eptr->dst, ap->ether, ETH_ADDR_SIZE );
	    net_send ( nbp );
#ifdef DEBUG_ARP
	    printf ( "IP arp send -- sent packet to %s\n", ip2str32 ( dest_ip ) );
#endif
	    return;
	}

	/* build a pending entry and queue the packet
	 */
	ap = arp_alloc ();

	ap->ip_addr = dest_ip;
	ap->flags = F_PENDING;

#ifdef DEBUG_ARP
	printf ("IP arp send: ARP request sent for %s\n", ip2str32 ( dest_ip ) );
#endif

	/* We only wait 20 seconds.
	 * (BSD waits 3 minutes)
	 */
	ap->ttl = 20;
	nbp->next = ap->outq;
	nbp->refcount++;
	ap->outq = nbp;

#ifdef DEBUG_ARP
	printf ("Pending packet queued for %s\n", ip2str32 ( ap->ip_addr ) );
#endif

	arp_request ( dest_ip );
}

/* Funky old test fixture to test the stack when all
 * we had was the ARP facility.
 * Argument in network byte order.
 * This must take care if there already is an entry in the cache.
 */
int
arp_ping ( u32 target_ip, char *ether )
{
	static int busy = 0;	/* paranoid */
	struct arp_data *ap;
	int rv = 0;

	if ( busy )
	    return 0;
	busy = 1;

	ap = arp_lookup_e ( target_ip ); 
	if ( ! ap )
	    ap = arp_alloc ();

	ap->ip_addr = target_ip;
	ap->flags = F_PING;

	/*
	printf ("ARP ping request sent for %s\n", ip2str32 ( target_ip ) );
	*/

	/* We only wait 10 seconds.
	 */
	ap->ttl = 10;
	ap->outq = (struct netbuf *) 0;

    	arp_request ( target_ip );

	sem_block ( arp_ping_sem );

	if ( ap->flags & F_VALID ) {
	    memcpy ( ether, ap->ether, ETH_ADDR_SIZE );
	    rv = 1;
	}

	/* free the entry */
	ap->ip_addr = 0;

	busy = 0;
	return rv;
}

/* Actually kind of bogus, but an early test
 * of this network setup
 */
void
show_arp_ping ( u32 target_ip )
{
    	unsigned char ether[ETH_ADDR_SIZE];
	u32 net_ip = htonl(target_ip);

	if ( arp_ping ( net_ip, ether ) )
	    printf ("%s is at %s\n", ip2str32 ( net_ip ), ether2str (ether) );
	else
	    printf ("No response from: %s\n", ip2str32 ( net_ip ) );
}

/* emit a gratuitous ARP message.
 * (an arp request looking for our own address).
 */
void
arp_announce ( void )
{
	struct netbuf *nbp;
	struct eth_arp *eap;

	if ( ! (nbp = netbuf_alloc ()) )
	    return;

#ifdef DEBUG_ARP
    printf ( "Sending Gratuitous ARP\n" );
#endif

	eap = (struct eth_arp *) nbp->iptr;

	eap->hfmt = ARP_ETHER_SWAP;
	eap->pfmt = ETH_IP_SWAP;
	eap->hlen = ETH_ADDR_SIZE;
	eap->plen = 4;

	net_addr_get ( eap->sha );

	// memcpy ( eap->spa, (char *) &host_info.my_ip, 4 );
	eap->spa = host_info.my_ip;
	memcpy ( eap->tha, zeros, ETH_ADDR_SIZE ); 
	// memcpy ( eap->tpa, (char *) &host_info.my_ip, 4 );
	eap->tpa = host_info.my_ip;
	eap->op = OP_REQ_SWAP;

	nbp->eptr->type = ETH_ARP_SWAP;
	memcpy ( nbp->eptr->dst, broad, ETH_ADDR_SIZE );
	nbp->ilen = sizeof ( struct eth_arp );

	net_send ( nbp );
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* May need lock */
void
arp_expire ( struct arp_data *ap )
{
	struct netbuf *nbp, *xbp;

	/*
	printf ( "ARP entry for %s expired\n", ip2str32 ( ap->ip_addr ) );
	*/

	ap->ip_addr = 0;

	if ( ap->flags & F_PING ) {
	    sem_unblock ( arp_ping_sem );
	}

	for ( nbp = ap->outq; nbp; nbp = xbp ) {
	    xbp = nbp->next;
	    netbuf_free ( nbp );
	}

}

/* This is called once a second */
void
arp_tick ( void )
{
	struct arp_data *ap;
	int i;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ! ap->ip_addr )
		continue;
	    if ( ap->flags & F_PERM )	/* permanent */
		continue;

	    if ( ap->ttl )
		ap->ttl--;
	    else
		arp_expire ( ap );
	}
}

void
arp_init ( void )
{
	int i;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    arp_cache[i].ip_addr = 0;
	}
	arp_ping_sem = sem_signal_new ( SEM_FIFO );
}

void
arp_show ( void )
{
	struct arp_data *ap;
	int i;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ! ap->ip_addr )
		continue;

	    printf ( "arp: %s at %s (ttl= %d)",
		ip2str32 ( ap->ip_addr ),
		ether2str( ap->ether), ap->ttl );
	    if ( ap->flags )
		printf ( " %04x", ap->flags );
	    printf ( "\n" );
	}
}

/* Find a free entry in the arp cache.
 * make second pass if needed to find the entry
 * nearly ready to expire.
 * ALWAYS returns something.
 * XXX - may want a lock here.
 */
static struct arp_data *
arp_alloc ( void )
{
	int i;
	struct arp_data *ap;
	struct arp_data *minp;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ! ap->ip_addr )
	    	return ap;
	}

	minp = &arp_cache[0];
	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ! ap->flags )	/* XXX */
		continue;
	    if ( ap->ttl < minp->ttl )
	    	minp = ap;
	}
	return minp;
}

static int
arp_save ( char *ether, u32 ip_addr )
{
	int i;
	struct arp_data *ap;
	struct arp_data *zp;
	struct netbuf *nbp, *xbp;

	zp = (struct arp_data *) 0;

	for ( i=0; i<MAX_ARP_CACHE; i++ ) {
	    ap = &arp_cache[i];
	    if ( ap->ip_addr == ip_addr ) {
		/* Refresh entry or filling a pending request */
		memcpy ( ap->ether, ether, ETH_ADDR_SIZE ); 
		ap->ttl = 20 * 60;
		ap->flags |= F_VALID;

		if ( ap->flags & F_PING ) {
		    sem_unblock ( arp_ping_sem );
		}

		if ( ap->flags & F_PENDING ) {
		    /*
		    printf ("Pending ARP for %s satisfied\n", ip2str32 ( ap->ip_addr ) );
		    */
		    ap->flags &= ~F_PENDING;

		    for ( nbp = ap->outq; nbp; nbp = xbp ) {
			memcpy ( nbp->eptr->dst, ap->ether, ETH_ADDR_SIZE );
			xbp = nbp->next;
			net_send ( nbp );
			netbuf_free ( nbp );
			/*
			printf ("Pending packet sent\n");
			*/
		    }
		    ap->outq = (struct netbuf *) 0;
		}

	    	return 0;
	    }

	    if ( ! zp && ! ap->ip_addr )
	    	zp = ap;
	}

	/* The above loop is an optimization, but
	 * we fall back on this if we need to replace an
	 * entry to get a slot.
	 */
	zp = arp_alloc ();

	/* New entry */
	// memcpy ( &zp->ip_addr, ip_addr, 4 );
	zp->ip_addr = ip_addr;
	memcpy ( zp->ether, ether, ETH_ADDR_SIZE );
	zp->flags = 0;
	zp->ttl = 20 * 60;
	zp->outq = (struct netbuf *) 0;

	return 1;
}

/* Public entry point for illicit arp caching.
 * Same as above, but allows debug.
 */
void
arp_save_icmp ( char *ether, u32 ip_addr )
{
	if ( arp_save ( ether, ip_addr ) ) {
	    /*
	    printf ( "Sneaky ARP from ICMP: " );
	    printf ( "machine: %s", ether2str(ether) );
	    printf ( " (%s)\n", ip2str ( ip_addr ) );
	    */
	}
}

/* THE END */
