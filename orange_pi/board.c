/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  7/5/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board.h"

#include "netbuf.h"

void
board_init ( void )
{
	// wdt_disable ();

	/* Turn on the green LED */
	led_init ();
	pwr_on ();

	gic_init ();

	serial_init ( CONSOLE_BAUD );
	timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();
}

/* This gets called after the network is alive and well
 *  to allow things that need the network up to initialize.
 *  (a hook for the PRU on the BBB).
 */
void
board_after_net ( void )
{
}

void
reset_cpu ( void )
{
	/* not working yet */
	main_reset ();
}

/* ---------------------------------- */

/* Initialize the network device */
int
board_net_init ( void )
{
	return 0;
}

/* Bring the network device online */
void
board_net_activate ( void )
{
}

void
board_net_show ( void )
{
}

void
get_board_net_addr ( char *addr )
{
}

void
board_net_send ( struct netbuf *nbp )
{
	// cpsw_send ( nbp );
}

/* THE END */
