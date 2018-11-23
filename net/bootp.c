/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* bootp.c
 * T. Trebisky  4-11-2005
 * T. Trebisky  6-2-2015 8-10-2016
 *
 * The problem with BOOTP is that it is almost obsolete.
 * Linux DHCP can support it (with "allow bootp;" as an option)
 * Many routers will do DHCP, but not BOOTP.
 * Working and tested 8-10-2016
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

#include "dhcp.h"

void bootp_rcv ( struct netbuf * );
void bootp_send ( void );

void
bootp_send ( void )
{
	struct bootp bootp;

	memset ( (char *) &bootp, 0, sizeof(struct bootp) );

	bootp.op = BOOTP_REQUEST;
	bootp.htype = BOOTP_ETHER;
	bootp.hlen = ETH_ADDR_SIZE;
	bootp.hops = 0;

	bootp.xid = 0xabcd1234;		/* XXX should be random value */
	bootp.secs = 10;		/* XXX should be how many seconds we've been trying */
	bootp.flags = 0;		/* ignored by bootp */

	bootp.client_ip = 0;
	bootp.your_ip = 0;
	bootp.server_ip = 0;
	bootp.gateway_ip = 0;

	net_addr_get ( bootp.chaddr );	/* client MAC address (allows server to bypass ARP to reply */

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &bootp, sizeof(struct bootp) );

}

static int bootp_debug;
static int bootp_ip;

void
bootp_rcv ( struct netbuf *nbp )
{
    	struct bootp *bpp;
	char dst[20];

	if ( bootp_debug )
	    printf ("Received BOOTP reply (%d bytes)\n", nbp->dlen );

	bpp = (struct bootp *) nbp->dptr;
	bootp_ip = bpp->your_ip;

	if ( bootp_debug ) {
	    strcpy ( dst, ether2str ( nbp->eptr->dst ) );
	    printf (" Server %s (%s) to %s (xid = %08x)\n",
		ip2str32 ( bpp->server_ip ),
		ether2str ( nbp->eptr->src ), dst,
		bpp->xid );
	    printf (" gives my IP as: %s\n", ip2str32 ( bootp_ip ) );
	}
}

#ifdef notdef
void
bootp_init ( void )
{
	// udp_hookup ( BOOTP_CLIENT, bootp_rcv );
}
#endif

#define BOOTP_RETRIES	5

static int
bootp_get ( void )
{
	int i;

	udp_hookup ( BOOTP_CLIENT, bootp_rcv );
	bootp_ip = 0;

	for ( i=0; i<BOOTP_RETRIES; i++ ) {
	    bootp_send ();
	    /* XXX - this would be an ideal place for a semaphore with a timeout */
	    thr_delay ( 1000 );
	    if ( bootp_ip )
		break;
	}

	udp_unhook ( BOOTP_CLIENT );

	return bootp_ip;
}

int
bootp_get_ip ( void )
{
	bootp_debug = 0;
	return bootp_get ();
}

void
bootp_test ( int arg )
{
	int ip;

	bootp_debug = 1;

	ip = bootp_get ();

	if ( ip == 0 )
	    printf ( "BOOTP test failed\n" );
	else
	    printf ( "BOOTP got IP of %s\n", ip2str32 ( ip ) );
}

/* THE END */
