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
#include "arch/cpu.h"

#include "netbuf.h"

// static void mmu_initialize ( u32, u32 );

static unsigned int cpu_clock;
static int cpu_clock_mhz;

// This is for the standard 1008 Mhz CPU clock
// This value can be adjusted at run time if
//  we change the CPU clock frequency.
// 200 gives 1006 (we want 1008)
// 201 gives 1011 (we want 1008)

// The Nano Pi NEO comes out of U-Boot running at 408 Mhz
// This is good given that it runs hot even at 408,
// and would certainly need a heat sink to run faster.
// With the delay adjustment done in delay_calib(),
// I get the following timings:
//  US Delay ticks = 411
//  MS Delay ticks = 406261
// These are within 1 percent, so I leave them be.

// Note also that these are with the D cache enabled.
// The delays will be MUCH longer if the D cache
//  is not enabled.

#define CPU_CLOCK_STD	1008
#define DELAY_COUNT_STD	201
static int us_delay_count = DELAY_COUNT_STD;

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

// 1003 gives 1.000 ms
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

	if ( cpu_clock_mhz != CPU_CLOCK_STD ) {
	    us_delay_count *= cpu_clock_mhz;
	    us_delay_count /= CPU_CLOCK_STD;
	}

#ifdef notdef
	// if ( cpu_clock_mhz == CPU_CLOCK_STD ) return;

	for ( i=0; i< 10; i++ ) {
	    reset_ccnt ();
	    delay_us ( 10 );
	    get_CCNT ( ticks );
	    printf ( "US Delay ticks = %d\n", ticks/10 );
	}

	for ( i=0; i< 10; i++ ) {
	    reset_ccnt ();
	    delay_ms ( 10 );
	    get_CCNT ( ticks );
	    printf ( "MS Delay ticks = %d\n", ticks/10 );
	}
#endif
}

#ifdef NOTSAFE
// works OK, but assumes 1Ghz cpu clock
// and far worse, is not thread safe.
void
delay_ns ( int delay )
{
        reset_ccnt ();
        while ( r_CCNT() < delay )
            ;
}
#endif

#ifdef notdef
/* I abandoned this overly complex scheme of dynamically
 * calibrating the delay loops.
 */
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
	get_CCNT ( ticks );
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );

	delay_factor *= ticks;
	delay_factor /= want;

	reset_ccnt ();
	delay_ns ( 1000 * CALIB_MICROSECONDS );
	get_CCNT ( ticks );
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );

	/* This reports a value of about 53 ticks */
	want = 0;
	reset_ccnt ();
	delay_ns ( 0 );
	get_CCNT ( ticks );
	printf ( "Delay w/ calib %d: ticks = %d (want: %d)\n", delay_factor, ticks, want );
}
#endif

/* Let the compiler find a place for this so
 * that we can use this as early as possible during
 * initialization.
 * Aligned for no particular reason.
 */
static char stack_area[NUM_CORES*STACK_PER_CORE] __attribute__ ((aligned (256)));

char *core_stacks = stack_area;

static unsigned int ram_start;
static unsigned int ram_size;

/* Called very early in initialization
 *  (now from locore.S to set up MMU)
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

/* Comes here when a new core starts up */
void
board_core_startup ( int core )
{
	int sp;

	get_SP ( sp );
	// garbled without lock
	// printf ( "Core %d starting with sp = %08x\n", core, sp );

	gic_cpu_init ();
	INT_unlock;

	// XXX notyet
	// run_newcore ( core );
}

/* This is called to start a new core */
void
board_new_core ( int core, cfptr func, void *arg )
{
	// XXX notyet
	// h3_start_core ( core, func, arg );
}

#ifdef notdef
static void
stack_addr_show ( void )
{
	int i;
	char * core_base;
	char * s_base;
	char * i_base;

	printf ( "Core stacks at %08x\n", core_stacks );
	printf ( "Core stack area at %08x\n", stack_area );
	for ( i=0; i< NUM_CORES; i++ ) {
	    core_base = &stack_area[i*STACK_PER_CORE];
	    printf ( "Core %d stacks at: %08x\n", i, core_base );
	    s_base = core_base;
	    i_base = s_base + MODE_STACK_SIZE;
	    printf ( " Core %d SVR stack at: %08x (%08x)\n", i, s_base, i_base );
	    s_base = s_base + MODE_STACK_SIZE;
	    i_base = s_base + MODE_STACK_SIZE;
	    printf ( " Core %d IRQ stack at: %08x (%08x)\n", i, s_base, i_base );
	    s_base = s_base + MODE_STACK_SIZE;
	    i_base = s_base + MODE_STACK_SIZE;
	    printf ( " Core %d FIQ stack at: %08x (%08x)\n", i, s_base, i_base );
	}
}
#endif

/* Called early, but not as early as the above.
 */
void
board_hardware_init ( void )
{
	unsigned int reg;

	/*
	// armv7 calls XXX
	get_SP ( reg );
        printf ( "board_hardware_init - sp: %08x\n",  reg );
	get_CPSR ( reg );
        printf ( "board_hardware_init - cpsr: %08x\n",  reg );
	*/

	cache_init ();
	ram_init ( ram_start, ram_size );
	// core_stacks = ram_alloc ( NUM_CORES * STACK_PER_CORE );
	// stack_addr_show ();
}

/* This gets a fault, but given that we are running at EL 2
 * that doesn't really tell us if we are in secure mode or not.
 * We should get a fault just for trying to access
 * a register that is designated only for EL3.
 * and there is no such register for EL2.
 */
static inline unsigned int
get_secure(void)
{
        unsigned int val;

        // asm volatile("mrs %0, SCR_EL2" : "=r" (val) : : "cc");
        asm volatile("mrs %0, SCR_EL3" : "=r" (val) : : "cc");
	return val;
}

static inline unsigned int
get_el(void)
{
        unsigned int val;

        asm volatile("mrs %0, CurrentEL" : "=r" (val) : : "cc");
        return val >> 2;
}

/* Added 1-9-2022 when I got curious about these things.
 *
 *  It shows us running at EL 2.
 */
void
board_check_el ( void )
{
	printf ( "ARM EL = %d\n", get_el() );
	// printf ( "ARM SCR = %d\n", get_secure() );
}

int
board_get_cpu_mhz ( void )
{
	return cpu_clock_mhz;
}

void
board_init ( void )
{
	// wdt_disable ();
	cpu_clock = get_cpu_clock ();
	cpu_clock_mhz = cpu_clock / 1000 / 1000;
	printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );

	ccu_init ();
	r_ccu_init ();

	printf ( "Board_init - initializing GPIO and LED\n" );
	gpio_init ();
	gpio_led_init ();

	/* Turn on the green LED */
	pwr_on ();
	/* Turn on the red one too */
	status_on ();

	wd_init ();

	h3_spinlocks_init ();
	gic_init ();

	board_check_el ();

	/* Must follow gic_init () */
	gpio_button_enable ();

	serial_init ( CONSOLE_BAUD );
	serial_aux_init ();

	timer_init ( DEFAULT_TIMER_RATE );

	delay_calib ();

	/* CPU interrupts on */
	INT_unlock;
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
