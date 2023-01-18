/*
 * Copyright (C) 2023  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * etimer.c
 * Tom Trebisky
 *
 * Event timer. 
 * Right now this is specific to the needs to debug some issues
 * with the orange pi emac driver.
 * I expect that is will eventually develop into a more general facility
 *
 * The key idea is to time different events using the ARM CCNT counter
 * This runs at the cpu frequency (400 Mhz or 1000 Mhz).
 */

#include "kyu.h"
#include <arch/cpu.h>

#define ET_MAX	10

struct times {
	int send_tcp;
	int send;
	int tx;
	int rx;
	int rx_tcp;
};

static struct times et[ET_MAX];

/* 2 is wait, 1 is done, 0 is run */
static int et_idle = 1;
static int et_skip = 10;
static int et_max = ET_MAX;
static int et_index = 0;

static int cpu_mhz;

void
etimer_init ( void )
{
	cpu_mhz = board_get_cpu_mhz ();
}

int
etimer ( void )
{
	int rv;

	rv = r_CCNT ();
	set_CCNT ( 0 );

	return rv / cpu_mhz;
}

void
etimer_arm ( int skip )
{
	// printf ( "etimer armed ...\n" );
	et_idle = 2;
	et_skip = skip;
}

void
etimer_show_times ( void )
{
	int i;
	struct times *ep;

	printf ( "     stcp     send       tx       rx     rtcp\n" );

	for ( i=0; i<ET_MAX; i++ ) {
	    ep = &et[i];
	    printf ( " %8d", ep->send_tcp );
	    printf ( " %8d", ep->send );
	    printf ( " %8d", ep->tx );
	    printf ( " %8d", ep->rx );
	    printf ( " %8d", ep->rx_tcp );
	    printf ( "\n" );
	}
}

void
etimer_show_times_OLD ( void )
{
	int i;
	struct times *ep;

	for ( i=0; i<ET_MAX; i++ ) {
	    ep = &et[i];
	    printf ( "  stcp  send          tx          rx          rtcp\n" );
	    printf ( " %d", ep->send_tcp );
	    printf ( " %d (%d)", ep->send, ep->send - ep->send_tcp );
	    printf ( " %d (%d)", ep->tx, ep->tx - ep->send );
	    printf ( " %d (%d)", ep->rx, ep->rx - ep->tx );
	    printf ( " %d (%d)", ep->rx_tcp, ep->rx_tcp - ep->rx );
	    printf ( "\n" );
	}
}

void
et_stcp ( void )
{
	if ( et_idle == 2 ) {
	    et_skip--;
	    if ( et_skip ) return;
	    et_idle = 0;
	    et_index = -1;
	}
	if ( et_idle == 1 ) return;
	et_index++;
	if ( et_index >= ET_MAX ) {
	    et_idle = 1;
	}
	if ( et_index == 0 )
	    et[et_index].send_tcp = 0;
	else
	    et[et_index].send_tcp = etimer ();
}

void
et_snd ( void )
{
	if ( et_idle ) return;
	et[et_index].send = etimer ();
}

void
et_tx ( void )
{
	if ( et_idle ) return;
	et[et_index].tx = etimer ();
}

void
et_rx ( void )
{
	if ( et_idle ) return;
	et[et_index].rx = etimer ();
}

void
et_rtcp ( void )
{
	if ( et_idle ) return;
	et[et_index].rx_tcp = etimer ();
}

/* THE END */
