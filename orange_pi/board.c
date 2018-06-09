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

static unsigned int cpu_clock;
static int cpu_clock_mhz;
static int delay_factor = 5000;

void
delay_ns ( int ns_delay )
{
	volatile unsigned int count;

	count = ns_delay * 1000 / delay_factor;
	while ( count -- )
	    ;
}

#define CALIB_MICROSECONDS	10

/* This uses the CCNT register to adjust the delay_factor to get more
 * precise timings.  But it doesn't work all that well.
 * We end up trimming the 5000 value to 5200 or so.
 * In other words, we divide the ns delay by 5.
 */
static void
delay_calib ( void )
{
	int ticks;
	int want = CALIB_MICROSECONDS * cpu_clock_mhz;

	reset_ccnt ();
	delay_ns ( 1000 * CALIB_MICROSECONDS );
	ticks = get_ccnt ();
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );

	delay_factor *= ticks;
	delay_factor /= want;

	reset_ccnt ();
	delay_ns ( 1000 * CALIB_MICROSECONDS );
	ticks = get_ccnt ();
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );

	/* This reports a value of about 53 ticks */
	want = 0;
	reset_ccnt ();
	delay_ns ( 0 );
	ticks = get_ccnt ();
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );
}

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
	cpu_clock = get_cpu_clock ();
	cpu_clock_mhz = cpu_clock / 1000 / 1000;
	printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );

	/* Turn on the green LED */
	gpio_led_init ();
	pwr_on ();
	wd_init ();

	spinlocks_init ();
	gic_init ();

	serial_init ( CONSOLE_BAUD );
	timer_init ( DEFAULT_TIMER_RATE );

	delay_calib ();

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
