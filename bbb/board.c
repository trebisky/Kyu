/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.c for the BBB
 *
 * Tom Trebisky  7/5/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

#include "netbuf.h"

/* Called very early in initialization */
void
board_hardware_init ( void )
{
	ram_init ( BOARD_RAM_START, BOARD_RAM_SIZE );
}

void
board_init ( void )
{
	wdt_disable ();

	clocks_init ();
	modules_init ();

	mux_init ();
	intcon_init ();
	cm_init ();

	serial_init ( CONSOLE_BAUD );
	// board_timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();

	dma_init ();
	gpio_init ();
	i2c_init ();

	adc_init ();
}

void
board_timer_init ( int rate )
{
	// dmtimer_probe ();
	dmtimer_init ( rate );
}

void
board_timer_rate_set ( int rate )
{
	dmtimer_rate_set ( rate );
}

/* This gets called after the network is alive and well */
void
board_after_net ( void )
{
	pru_init ();
}

int
board_net_init ( void )
{
	return cpsw_init ();
}

void
board_net_activate ( void )
{
	cpsw_activate ();
}

void
board_net_show ( void )
{
	cpsw_show ();
}

void
get_board_net_addr ( char *addr )
{
	get_cpsw_addr ( addr );
}

void
board_net_send ( struct netbuf *nbp )
{
	cpsw_send ( nbp );
}

/* THE END */
