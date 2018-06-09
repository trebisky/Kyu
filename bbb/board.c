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

#define CALIB_MICROSECONDS      10

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
}

void
board_init ( void )
{
	wdt_disable ();

	// cpu_clock = get_cpu_clock ();
	cpu_clock = 1000 * 1000 * 1000;
        cpu_clock_mhz = cpu_clock / 1000 / 1000;
        printf ( "CPU clock %d Mhz (?? XXX)\n", cpu_clock_mhz );

	clocks_init ();
	modules_init ();

	mux_init ();
	intcon_init ();
	cm_init ();

	serial_init ( CONSOLE_BAUD );

	timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();

	dma_init ();
	gpio_init ();
	i2c_init ();

	adc_init ();
}

/* Called by timer_init () */
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

void
board_net_debug ( void )
{
	cpsw_debug ();
}

/* THE END */
