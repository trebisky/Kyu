/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* dhcp.c
 * T. Trebisky  8-10-2016
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

#include "dhcp.h"

/* We don't necessarily use all of these options */

#define OP_PAD		0
#define OP_END		0xff

#define OP_SUBNET_MASK	1
#define OP_TIME_OFFSET	2
#define OP_GATEWAY	3
#define OP_DNS		6
#define OP_HOSTNAME	12
#define OP_DOMAIN	15

#define OP_REQIP	50	/* Requested IP */
#define OP_LEASE_TIME	51

#define OP_TYPE		53	/* Message type */
#define OP_SERVER	54	/* Server IP */
#define OP_PARAM	55	/* Parameter request list */

#define TYPE_DISCOVER	1
#define TYPE_OFFER	2
#define TYPE_REQUEST	3
#define TYPE_DECLINE	4
#define TYPE_ACK	5
#define TYPE_NACK	6
#define TYPE_RELEASE	7
#define TYPE_INFORM	8

void dhcp_rcv ( struct netbuf * );
void dhcp_discover ( void );
void dhcp_request ( void );

/* DHCP is a "refinement" of BOOTP and is indicated
 * by a magic cookie value at the start of the options area
 */
static char cookie[] = { 0x63, 0x82, 0x53, 0x63 };

// extern struct host_info host_info;

/* The game goes like this.
 * 4 packets get exchanged in the simplest case.
 * 2 are sent (broadcase) 2 are received.
 *
 * First the client (us) broadcasts a DHCP discover.
 * Then it waits a while for servers to respond.
 * One or more servers send a DHCP offer.
 * The client (us) picks one, perhaps the first one.
 * The client broadcasts a DHCP request that contains the chosen address.
 * The server should then send a DHCP ACK.
 */

static char *
option_add ( char *op, int val, int size, char *data )
{
	*op++ = val;
	*op++ = size;
	memcpy ( op, data, size );
	return op + size;
}

static char *
option_add_1 ( char *op, int val, int data )
{
	*op++ = val;
	*op++ = 1;
	*op++ = data;
	return op;
}

static char *
option_add_2 ( char *op, int val, int d1, int d2 )
{
	*op++ = val;
	*op++ = 2;
	*op++ = d1;
	*op++ = d2;
	return op;
}

int our_xid;
int our_clock;

void
dhcp_discover ( void )
{
	struct bootp dhcp;
	char *op;

	memset ( (char *) &dhcp, 0, sizeof(struct bootp) );

	dhcp.op = BOOTP_REQUEST;
	dhcp.htype = BOOTP_ETHER;
	dhcp.hlen = ETH_ADDR_SIZE;
	dhcp.hops = 0;

	// our_xid = 0xabcd1234;	/* XXX should be random value */
	our_xid = gb_next_rand ();

	dhcp.xid = our_xid;
	dhcp.secs = our_clock;
	dhcp.flags = 0;

	dhcp.client_ip = 0;
	dhcp.your_ip = 0;
	dhcp.server_ip = 0;
	dhcp.gateway_ip = 0;

	net_addr_get ( dhcp.chaddr );	/* client MAC address (allows server to bypass ARP to reply) */

	op = dhcp.options;
	memcpy ( dhcp.options, cookie, 4 );
	op += 4;
	op = option_add_1 ( op, OP_TYPE, TYPE_DISCOVER );
	op = option_add_2 ( op, OP_PARAM, OP_SUBNET_MASK, OP_GATEWAY );
	*op++ = OP_END;

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &dhcp, sizeof(struct bootp) );
}

/* Stuff collected from OFFER packet */
/* Don't ask me why we need two independent values of
 * the server_ip (but from different places in the packet)
 * which actually have the same value.
 */
static unsigned int your_ip;
static unsigned int server_ip1;
static unsigned int server_ip2;

void
dhcp_request ( void )
{
	struct bootp dhcp;
	char *op;

	memset ( (char *) &dhcp, 0, sizeof(struct bootp) );

	dhcp.op = BOOTP_REQUEST;
	dhcp.htype = BOOTP_ETHER;
	dhcp.hlen = ETH_ADDR_SIZE;
	dhcp.hops = 0;

	dhcp.xid = our_xid;
	dhcp.secs = our_clock;
	dhcp.flags = 0;			/* ignored by bootp */

	dhcp.client_ip = 0;
	dhcp.your_ip = 0;
	dhcp.server_ip = server_ip1;	/* *** */
	dhcp.gateway_ip = 0;

	net_addr_get ( dhcp.chaddr );	/* client MAC address (allows server to bypass ARP to reply */

	op = dhcp.options;
	memcpy ( dhcp.options, cookie, 4 );
	op += 4;
	op = option_add_1 ( op, OP_TYPE, TYPE_REQUEST );
	op = option_add ( op, OP_REQIP, 4, (char *) &your_ip );
	op = option_add ( op, OP_SERVER, 4, (char *) &server_ip2 );
	*op++ = OP_END;

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &dhcp, sizeof(struct bootp) );
}

static void
show_options ( struct bootp *bpp )
{
	char *op = bpp->options + 4;
	char *opend = bpp->options + DHCP_OPTION_SIZE;

	while ( op < opend && *op != OP_END ) {
	    if ( *op == 0 ) {
		printf ( "Pad\n" ); op++;
		continue;
	    }
	    printf ( "Option %02x, len %d\n", *op, op[1] );
	    op += 2 + op[1];
	}
}

static char *
find_option ( struct bootp *bpp, int key )
{
	char *op = bpp->options + 4;
	char *opend = bpp->options + DHCP_OPTION_SIZE;

	while ( op < opend && *op != OP_END ) {
	    if ( *op == 0 )
		op++;
	    else {
		if ( *op == key )
		    return op;
		op += 2 + op[1];
	    }
	}
}

static char * dhcp_type_string[] = { "ZERO", "DISCOVER", "OFFER", "REQUEST", "DECLINE", "ACK", "NACK", "RELEASE", "INFORM" };

static void
dhcp_show_pkt ( struct bootp *bpp, struct netbuf *nbp )
{
	char *p, *t;

	p = find_option ( bpp, OP_TYPE );
	t = dhcp_type_string[p[2]];
	printf ("Received DHCP reply from %s (xid = %08x, %d bytes), type = %s\n",
	    ip2str32 ( bpp->server_ip ), bpp->xid, nbp->dlen, t );

	// printf (" From macs %s to %s\n",
	    // ether2str ( nbp->eptr->src ), ether2str ( nbp->eptr->dst ) );

	// show_options ( bpp );
}

static int dhcp_debug = 0;

static struct thread *dhcp_thread;
static int dhcp_state;

/* A little state machine */
#define ST_OWAIT1	1
#define ST_OWAIT2	2
#define ST_AWAIT1	3
#define ST_AWAIT2	4

static struct host_info *hip;

/* packet evaporates once we return,
 * so we copy things out of it as needed
 * 
 * About byte order:
 *  Kyu keeps IP addresses (and netmasks and such) in
 *  network byte order.  Also in this code we sometimes just
 *  copy IP addresses around, so there is no need for byte order
 *  rearranging.
 */
void
dhcp_rcv ( struct netbuf *nbp )
{
    	struct bootp *bpp;
	char *p;
	int type;

	bpp = (struct bootp *) nbp->dptr;

	if ( dhcp_debug )
	    dhcp_show_pkt ( bpp, nbp );

	if ( bpp->xid != our_xid ) {
	    printf ("Bad xid, ingnoring packet\n" );
	    return;
	}

	p = find_option ( bpp, OP_TYPE );
	type = p[2];

	if ( type == TYPE_OFFER && dhcp_state == ST_OWAIT1 ) {
	    server_ip1 = bpp->server_ip;
	    your_ip = bpp->your_ip;
	    // Silly ...
	    // p = find_option ( bpp, OP_SERVER );
	    // memcpy ( &server_ip2, &p[2], 4 );
	    server_ip2 = server_ip1;
	    if ( dhcp_debug ) {
		printf ( "OFFER your ip = %s\n", ip2str32 ( your_ip ) );
		printf ( "OFFER server ip 1 = %s\n", ip2str32 ( server_ip1 ) );
		// printf ( "OFFER server ip 2 = %s\n", ip2str32 ( server_ip2 ) );
	    }

	    dhcp_state = ST_OWAIT2;
	    thr_unblock ( dhcp_thread );
	}
	else if ( type == TYPE_ACK && dhcp_state == ST_AWAIT1 ) {
	    hip->my_ip = bpp->your_ip;			/* many places in packet to get this from */
	    p = find_option ( bpp, OP_SUBNET_MASK );	/* Subnet mask 4 bytes */
	    if ( p )
		memcpy ( &hip->net_mask, &p[2], 4 );
	    p = find_option ( bpp, OP_GATEWAY );	/* Router IP (gateway) n*4 bytes*/
	    if ( p )
		memcpy ( &hip->gate_ip, &p[2], 4 );

	    // Other good things we could harvest ...
	    // Note that hostname may or may not end with null byte(s)
	    // p = find_option ( bpp, OP_SERVER );	/* Server */
	    // p = find_option ( bpp, OP_LEASE_TIME );	/* Lease time (seconds) 4 bytes */
	    // p = find_option ( bpp, OP_DNS );		/* DNS IP (n*4 bytes) */
	    // p = find_option ( bpp, OP_HOSTNAME );	/* host name (3 bytes for "kyu") */
	    // p = find_option ( bpp, OP_DOMAIN );	/* domain name (8 bytes for "mmto.org") */

	    dhcp_state = ST_AWAIT2;
	    thr_unblock ( dhcp_thread );
	}
	else
	    printf ( "Ignoring packet\n" );
}

#define DHCP_RETRIES	4

static int
dhcp_once ( void )
{
	dhcp_thread = thr_self();

	dhcp_state = ST_OWAIT1;
	dhcp_discover ();

	/* This delay is what motivated adding the ability in Kyu
	 * to unblock a thread that is doing a delay.
	 * That makes this just a timeout, but we expect to be
	 * signaled when we get a response we like.
	 */
	thr_delay ( 1000 );	/* wait for offers */
	if ( dhcp_state != ST_OWAIT2 )
	    return 0;

	dhcp_state = ST_AWAIT1;
	dhcp_request ();

	thr_delay ( 1000 );	/* wait for ACK */
	if ( dhcp_state != ST_AWAIT2 )
	    return 0;

	/* good */
	return 1;
}

static int
dhcp_get ( void )
{
	int rv;

	udp_hookup ( BOOTP_CLIENT, dhcp_rcv );

	for ( our_clock=0; our_clock<DHCP_RETRIES; our_clock++ ) {
	    if ( rv = dhcp_once () )
		break;
	}

	udp_unhook ( BOOTP_CLIENT );

	return rv;
}

void
dhcp_set_debug ( int val )
{
	dhcp_debug = 1;
}

/* This is what the system calls to do DHCP */
int
dhcp_host_info ( struct host_info *hp )
{
	hip = hp;
	return dhcp_get ();
}

void
dhcp_test ( int arg )
{
	int ip;
	struct host_info scratch;

	/* Fetch data into trash area in case we have valid
	 * info that we don't want the test to corrupt.
	 */
	hip = &scratch;

	dhcp_set_debug ( 1 );

	if ( dhcp_get()  == 0 )
	    printf ( "DHCP test failed\n" );
	else  {
	    printf ( "DHCP got IP of %s\n", ip2str32 ( hip->my_ip ) );
	    printf ( "DHCP got netmask of  %08x\n", ntohl ( hip->net_mask ) );
	}

	dhcp_set_debug ( 0 );
}

/* THE END */
