/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.c for the x86
 *
 * Tom Trebisky  1/5/2017
 *
 * XXX - eventually we will have a galileo directory with board.c
 * and the x86 directory will have hardware.c that is common to
 * various x86 targets.  For now this is a mess pending cleanup.
 */

#include "kyu.h"
#include "kyulib.h"
#include "board.h"

void
board_init ( void )
{
	/* Must call trap_init() first */
	trap_init ();
	timer_init ();
	/* timer_init ( DEFAULT_TIMER_RATE ); */
	kb_init ();
	sio_init ();

	/* CPU interrupts on */
	enable_irq ();
}

void
board_timer_init ( int rate )
{
	intel_timer_init ( rate );
}

void
board_timer_rate_set ( int rate )
{
	intel_timer_rate_set ( rate );
}

/* This gets called after the network is alive and well */
void
board_after_net ( void )
{
}

/* The rest of this is network interface stuff */

static int num_ee = 0;
static int num_rtl = 0;
// static int num_el3 = 0;

/* XXX - trouble if we have more than one network interface */
int
board_net_init ( void )
{
	int rv = 0;

	num_ee = ee_init ();
	rv += num_ee;

	num_rtl = rtl_init ();
	rv += num_rtl;

	// num_el3 = el3_init ();
	// rv += num_el3;
}

void
board_net_activate ( void )
{
	if ( num_rtl ) rtl_activate ();
	if ( num_ee ) ee_activate ();
}

void
board_net_show ( void )
{
	if ( num_ee ) ee_show ();
	if ( num_rtl ) rtl_show ();
}

void
get_board_net_addr ( char *addr )
{
	if ( num_ee ) get_ee_addr ( buf );
	if ( num_rtl ) get_rtl_addr ( buf );
}

void
board_net_send ( struct netbuf *nbp )
{
	if ( num_ee ) ee_send ( nbp );
	if ( num_rtl ) rtl_send ( nbp );
}

/* THE END */
