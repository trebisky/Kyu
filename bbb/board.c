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

// This is for the standard 1000 Mhz CPU clock
// This value can be adjusted at run time if
//  we change the CPU clock frequency.
// 249 gives 1001.4 ns
// 248 gives 997.4 ns
static int us_delay_count = 249;

/* These are good for ballpark timings,
 * and are calibrated by trial and error
 */
void
delay_us ( int delay )
{
        volatile unsigned int count;

        count = delay * us_delay_count;
        while ( count -- )
            ;
}

// 1003 gives 0.999 ms
void
delay_ms ( int delay )
{
	unsigned int n;

	for ( n=delay; n; n-- )
	    delay_us ( 1003 );
}

static void
delay_calib ( void )
{
        int ticks;
	int i;

	for ( i=0; i< 10; i++ ) {
	    reset_ccnt ();
	    delay_us ( 10 );
	    ticks = get_ccnt ();
	    printf ( "US Delay ticks = %d\n", ticks/10 );
	}

	for ( i=0; i< 10; i++ ) {
	    reset_ccnt ();
	    delay_ms ( 10 );
	    ticks = get_ccnt ();
	    printf ( "MS Delay ticks = %d\n", ticks/10 );
	}
}

#ifdef NOTSAFE
// works OK, but assumes 1Ghz cpu clock
// and is not thread safe.
void
delay_ns ( int delay )
{
        reset_ccnt ();
        while ( get_ccnt() < delay )
            ;
}
#endif

/* Called very early in initialization.
 *  (now from locore.S to set up MMU)
 */
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
        printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );

	clocks_init ();
	modules_init ();

	mux_init ();
	intcon_init ();
	cm_init ();

	serial_init ( CONSOLE_BAUD );

	timer_init ( DEFAULT_TIMER_RATE );

	// delay_calib ();

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
