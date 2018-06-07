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

unsigned long core_stacks;

static void mmu_initialize ( unsigned long, unsigned long );

/* Called very early in initialization */
void
board_hardware_init ( void )
{
	cache_init ();
	ram_init ( BOARD_RAM_START, BOARD_RAM_SIZE );
	core_stacks = ram_alloc ( NUM_CORES * CORE_STACK_SIZE );
	printf ( "Core stacks at %08x\n", core_stacks );
}

void
board_init ( void )
{
	// wdt_disable ();

	/* Turn on the green LED */
	gpio_led_init ();
	pwr_on ();
	wd_init ();

	spinlocks_init ();
	gic_init ();

	serial_init ( CONSOLE_BAUD );
	timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();
}

/* Called by timer_init () */
void
board_timer_init ( int rate )
{
	opi_timer_init ( rate );
}

void
board_timer_rate_set ( int rate )
{
	opi_timer_rate_set ( rate );
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
	system_reset ();
}

/* ---------------------------------- */

/* Initialize the network device */
int
board_net_init ( void )
{
        return emac_init ();
}

/* Bring the network device online */
void
board_net_activate ( void )
{
        emac_activate ();
}

void
board_net_show ( void )
{
        emac_show ();
}

void
get_board_net_addr ( char *addr )
{
        get_emac_addr ( addr );
}

void
board_net_send ( struct netbuf *nbp )
{
        emac_send ( nbp );
}

void
board_net_debug ( void )
{
	emac_debug ();
}

/* THE END */
