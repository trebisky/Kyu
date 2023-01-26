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
#include "thread.h"

#include <arch/cpu.h>

extern struct thread *cur_thread;

#define ET_MAX	10

struct times {
	char *name;
	int send_tcp;
	int a;
	int b;
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

/* ---------- */

void
etimer_arm ( int skip )
{
	// printf ( "etimer armed ...\n" );
	et_idle = 2;
	et_skip = skip;
}

#define ET_FLAG		999999

static void
etimer_print ( int val )
{
	if ( val == ET_FLAG )
	    printf ( "        -", val );
	else
	    printf ( " %8d", val );
}

static void
etimer_print_s ( char *name )
{
	printf ( " %10s", name );
}

void
etimer_show_times ( void )
{
	int i;
	struct times *ep;

	printf ( "     thread     stcp        A        B     send       tx       rx     rtcp\n" );

	for ( i=0; i<ET_MAX; i++ ) {
	    ep = &et[i];
	    etimer_print_s ( ep->name );
	    etimer_print ( ep->send_tcp );
	    etimer_print ( ep->a );
	    etimer_print ( ep->b );
	    etimer_print ( ep->send );
	    etimer_print ( ep->tx );
	    etimer_print ( ep->rx );
	    etimer_print ( ep->rx_tcp );
	    printf ( "\n" );
	}
}

static void
etimer_clear ( void )
{
	int i;
	struct times *ep;

	for ( i=0; i<ET_MAX; i++ ) {
	    ep = &et[i];
	    ep->send_tcp = ET_FLAG;
	    ep->a = ET_FLAG;
	    ep->b = ET_FLAG;
	    ep->send = ET_FLAG;
	    ep->tx = ET_FLAG;
	    ep->rx = ET_FLAG;
	    ep->rx_tcp = ET_FLAG;
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
	    etimer_clear ();
	}
	if ( et_idle == 1 ) return;
	et_index++;
	if ( et_index >= ET_MAX ) {
	    et_idle = 1;
	}

	et[et_index].name = cur_thread->name;
	if ( et_index == 0 )
	    et[et_index].send_tcp = ET_FLAG;
	else
	    et[et_index].send_tcp = etimer ();
}

void
et_A ( void )
{
	if ( et_idle ) return;
	et[et_index].a = etimer ();
}

void
et_B ( void )
{
	if ( et_idle ) return;
	et[et_index].b = etimer ();
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

/* ======================================================= */
/* ======================================================= */
/* Tacking this on the end for lack of a better place.
 *  1-26-2023
 */

static int
time_one ( int nk )
{
	int nb = nk * 1000;
	// char *adr = (char *) 0x48000000;
	// char *adr = (char *) 0x47000000;
	// char *adr = (char *) 0x44000000;
	char *adr = (char *) BOARD_RAM_START;
	int rv;

	/* BBB has 512M from 0x8... to 0x9ff... */
	adr += 64*1024*1024;

	(void) etimer ();
	memcpy ( adr, adr, nb );
	rv = etimer ();

	return rv;
}

static void
time_set ( int nk )
{
	int tpk;

	printf ( "%4dK:", nk );
	printf ( " %6d", time_one ( nk ) );
	printf ( " %6d", time_one ( nk ) );
	printf ( " %6d", time_one ( nk ) );
	printf ( " %6d", time_one ( nk ) );
	tpk = time_one ( nk ) / nk;
	printf ( " per 1K = %5d", tpk );
	printf ( "\n" );
}

void
cache_timings ( void )
{
	printf ( " -- Cache timings:\n" );
	time_set ( 500 );
	time_set ( 100 );
	time_set ( 20 );
	time_set ( 1 );
}

/* THE END */
