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

// static void mmu_initialize ( unsigned long, unsigned long );

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
 
#ifdef  BOARD_NANOPI_NEO_XXX
/* I don't need this now, as I reset the Neo clock
 * to 1008 Mhz, just like the Orange Pi, so they can share
 * the same delay and timing code here.
 */
/* The autocal yielded a count of 81, but this gave 22 us rather
 * than 20 us delays, so I reduced it by hand to 74.
 */
#define CPU_CLOCK_INIT		408
// #define DELAY_COUNT_INIT	81
// #define DELAY_COUNT_INIT	72
#define DELAY_COUNT_INIT	90
#endif

/* These numbers seem right for a 1008 clock */
#define CPU_CLOCK_INIT		1008
// #define DELAY_COUNT_INIT	201
#define DELAY_COUNT_INIT	160	/* recal on Neo to give nice 5 us */

/* Note - once I reset the Neo CPU clock to 1008, it recalibrated the
 * delay count to 222 rather than 201
 * At least it is not an order of magnitude off due to the cache not
 * being enabled or something.
 * Then I got busy setting up some low overhead test loops to get the
 * 5 us delay right.
 */

static int us_delay_count = DELAY_COUNT_INIT;

#ifdef  BOARD_NANOPI_NEO_XX
/* I spent some time with the Neo (which runs at 408 Mhz
 * with my oscilloscope using a little routine in dallas.c
 * I found that calling with delay=0 gives a 3 us delay!
 * So for really short delays we need to take this
 * into account.
 */
void
delay_us ( int delay )
{
        volatile unsigned int count;

	if ( delay <= 3 )
	    return;

        count = (delay - 3) * us_delay_count;
        while ( count -- )
            ;
}
#endif

/* These are good for ballpark timings,
 * and are calibrated by trial and error.
 * See the above for overhead calibrations on the 408 Mhz Neo.
 * I am too lazy to fiddle with this for the 1008 Mhz Opi,
 * but I should do that someday and unify this with the above.
 * tjt  8-15-2021
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

	if ( cpu_clock_mhz != CPU_CLOCK_INIT ) {
	    us_delay_count *= cpu_clock_mhz;
	    us_delay_count /= CPU_CLOCK_INIT;
	    printf ( "--- DELAY recalibrated (cpu = %d)\n", cpu_clock_mhz );
	    printf ( "CPU us_delay_count = %d\n", us_delay_count );
	}

#ifdef notdef
	// if ( cpu_clock_mhz == CPU_CLOCK_INIT ) return;

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

	run_newcore ( core );
}

/* This is called to start a new core */
void
board_new_core ( int core, cfptr func, void *arg )
{
	h3_start_core ( core, func, arg );
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

	get_SP ( reg );
        printf ( "board_hardware_init - sp: %08x\n",  reg );
	get_CPSR ( reg );
        printf ( "board_hardware_init - cpsr: %08x\n",  reg );

	cache_init ();
	ram_init ( ram_start, ram_size );
	// core_stacks = ram_alloc ( NUM_CORES * STACK_PER_CORE );
	// stack_addr_show ();
}

int
board_get_cpu_mhz ( void )
{
	return cpu_clock_mhz;
}

static void
board_cpu_init ( void )
{
	cpu_clock = get_cpu_clock ();
	cpu_clock_mhz = cpu_clock / 1000 / 1000;
	printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );

	/* The Neo comes up at 408 Mhz, but let's fix that.
	 * (actually it isn't bad to run the Neo at 400
	 *  for thermal reasons).
	 */
	if ( cpu_clock_mhz != 1008 ) {
	    printf ( "Resetting CPU clock\n" );
	    set_cpu_clock_1008 ();
	    cpu_clock = get_cpu_clock ();
	    cpu_clock_mhz = cpu_clock / 1000 / 1000;
	    printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );
	}
}

/* With this ARM compiler, int and long are both 4 byte items
 */
static void
show_compiler_sizes ( void )
{
	printf ( "sizeof int: %d\n", sizeof(int) );
	printf ( "sizeof long: %d\n", sizeof(long) );
}

void
board_init ( void )
{
	printf ( "Starting board initialization for Orange Pi\n" );
	// show_compiler_sizes ();

	// wdt_disable ();
	board_cpu_init ();

	/* Turn on the green LED */
	gpio_init ();
	gpio_led_init ();

	/* This will NEVER work before gic_init() */
	// i2c_init ();

	pwr_on ();
	wd_init ();

	h3_spinlocks_init ();
	gic_init ();

	/* Must follow gic_init () */
	gpio_button_enable ();

	serial_init ( CONSOLE_BAUD );
	serial_aux_init ();

	i2c_init ();

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
#ifdef WANT_NET
        return emac_init ();
#endif
}

/* Bring the network device online */
void
board_net_activate ( void )
{
#ifdef WANT_NET
        emac_activate ();
#endif
}

void
board_net_show ( void )
{
#ifdef WANT_NET
        emac_show ();
#endif
}

void
get_board_net_addr ( char *addr )
{
#ifdef WANT_NET
        get_emac_addr ( addr );
#endif
}

void
board_net_send ( struct netbuf *nbp )
{
#ifdef WANT_NET
        emac_send ( nbp );
#endif
}

void
board_net_debug ( void )
{
#ifdef WANT_NET
	emac_debug ();
#endif
}

/* This is a hook in the IO test menu
 * for whatever we happen to be working on at the time.
 * It will accumulate a list of the various tests
 * that it has triggered through history.
 */
void
board_test_generic ( int arg )
{
	// gpio_rapid ();
	dallas_test ();
}

/* THE END */
