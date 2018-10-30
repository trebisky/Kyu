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
#include "arch/cpu.h"

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
	    ticks = r_CCNT ();
	    printf ( "US Delay ticks = %d\n", ticks/10 );
	}

	for ( i=0; i< 10; i++ ) {
	    reset_ccnt ();
	    delay_ms ( 10 );
	    ticks = r_CCNT ();
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
        while ( r_CCNT() < delay )
            ;
}
#endif

static unsigned int ram_start;
static unsigned int ram_size;

/* Let the compiler find a place for this so
 * that we can use this as early as possible during
 * initialization.
 * Aligned for no particular reason.
 */
static char stack_area[NUM_CORES*STACK_PER_CORE] __attribute__ ((aligned (256)));

char *core_stacks = stack_area;

/* Called very early in initialization
 *  (now from locore.S to set up MMU)
 *
 * The BBB always has 512M of RAM.
 *  at 0x80000000 -- 0x9fffffff
 */
void
board_mmu_init ( void )
{
	ram_start = BOARD_RAM_START;
	// ram_size = BOARD_RAM_SIZE;

	printf ( "Probing for amount of ram\n" );
	ram_size = ram_probe ( ram_start );
	printf ( "Found %d M of ram\n", ram_size/(1024*1024) );

	mmu_initialize ( ram_start, ram_size );
}

/* extra cores come here on startup */
void
board_core_startup ( int core )
{
	/* the BBB has only one core */
}

void
board_new_core ( int core, cfptr func, void *arg )
{
	/* nothing to do here */
}

/* Called early, but not as early as board_mmu_init ()
 */
void
board_hardware_init ( void )
{
	cache_init ();
	// ram_init ( BOARD_RAM_START, BOARD_RAM_SIZE );
	ram_init ( ram_start, ram_size );
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

	// printf ( "TJT turn int on\n" );
	/* CPU interrupts on */
	// INT_unlock;

	dma_init ();
	gpio_init ();
	i2c_init ();

	adc_init ();
	// printf ( "board_init done\n" );
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
