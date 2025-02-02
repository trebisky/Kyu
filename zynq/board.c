/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 * Copyright (C) 2024  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.c for the Zynq
 *
 * Tom Trebisky  11/14/2024
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board.h"
#include "arch/cpu.h"

#include "netbuf.h"

static unsigned int cpu_clock;
static int cpu_clock_mhz;

static unsigned int ram_start;
static unsigned int ram_size;

/* XXX XXX this delay stuff needs rework and checking for Zynq */
/* XXX XXX this delay stuff needs rework and checking for Zynq */
/* XXX XXX this delay stuff needs rework and checking for Zynq */

/* These comments are all for the Orange Pi H3 */
// This is for the standard 1008 Mhz CPU clock
// This value can be adjusted at run time if
//  we change the CPU clock frequency.
// 200 gives 1006 (we want 1008)
// 201 gives 1011 (we want 1008)
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
}

void
board_early_putchar ( int c )
{
	uart_early_putc ( c );
}

/* Let the compiler find a place for this so
 * that we can use this as early as possible during
 * initialization.
 * Aligned for no particular reason.
 */
static char stack_area[NUM_CORES*STACK_PER_CORE] __attribute__ ((aligned (256)));

char *core_stacks = stack_area;

void
board_mmu_init ( void )
{}

// static void mmu_initialize ( unsigned long, unsigned long );

void
board_ram_init ( void )
{
	ram_start = BOARD_RAM_START;
	// ram_size = BOARD_RAM_SIZE;

	printf ( "Probing for amount of ram\n" );
	ram_size = ram_probe ( ram_start );
	printf ( "Found %d M of ram\n", ram_size/(1024*1024) );

	// mmu_initialize ( ram_start, ram_size );
}

/* This is called to start a new core */
void
board_new_core ( int core, cfptr func, void *arg )
{
	// h3_start_core ( core, func, arg );
}

/* Comes here when a new core starts up */
void
board_core_startup ( int core )
{
}

/* Called early, but not as early as board_init()
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

	// OLD way
	// core_stacks = ram_alloc ( NUM_CORES * STACK_PER_CORE );
	// stack_addr_show ();
}

#ifdef OLD_ORANGE_PI
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

	// mmu_initialize_opi ( ram_start, ram_size );
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
#endif /* OLD_ORANGE_PI */

int
board_get_cpu_mhz ( void )
{
	return cpu_clock_mhz;
}

/*
 * Returns rate in Hz
 * Use the Kyu "i 8" command to check this
 */
static unsigned int
get_cpu_clock ( void )
{
		/* XXX Hack for Zynq */
		return 666*1000*1000;
}

/* See TRM section 3.4 and registers on page 1393
 */
static void
zynq_l2_check ( void )
{
		vu32 *rp = (vu32 *) 0xF8F02100;

		printf ( "Zynq L2 control: %08x\n", *rp );
}

static void
board_cpu_init ( void )
{
	zynq_l2_check ();
	cpu_clock = get_cpu_clock ();
	cpu_clock_mhz = cpu_clock / 1000 / 1000;
	printf ( "CPU clock %d Mhz\n", cpu_clock_mhz );
}

#ifdef notdef
/* With this ARM compiler, int and long are both 4 byte items
 */
static void
show_compiler_sizes ( void )
{
	printf ( "sizeof int: %d\n", sizeof(int) );
	printf ( "sizeof long: %d\n", sizeof(long) );
}
#endif

void
board_init ( void )
{
	printf ( "Starting board initialization for the Zynq\n" );

	// show_compiler_sizes ();

	board_cpu_init ();

	gic_init ();

	serial_init ( CONSOLE_BAUD );
	// serial_aux_init ();

	// upstream in Kyu code
	timer_init ( DEFAULT_TIMER_RATE );

	// delay_calib ();

	/* CPU interrupts on */
	INT_unlock;

	// It works here, but the rest of Kyu never starts
	// led_demo ();

	// This kills the on-board LED light show
	//  from the bitstream loaded by U-boot
	// pl_reset ();

	// This loads our bitstream
	pl_load ();
	// emio_test ();

#ifdef OLD_ORANGE_PI
	// wdt_disable ();

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
#endif /* OLD_ORANGE_PI */
}

/* Called by timer_init () */
void
board_timer_init ( int rate )
{
	zynq_timer_init ( rate );
}

void
board_timer_rate_set ( int rate )
{
	zynq_timer_rate_set ( rate );
}

/* This gets called after the network is alive and well
 *  to allow things that need the network up to initialize.
 *  (a hook for the PRU on the BBB).
 */
void
board_after_net ( void )
{
	// A bit unconventional to call from here, but it works well
	led_demo ();
	axi_test ();
	// axi_gpio_test ();

	/* investigate fabric (and CPU) clocks */
	fabric_test ();

	/* My Hub75 64x64 led panel */
	hub_test ();

	/* The normal thing */
	(void) eth_init ();
}

#define SLCR_UNLOCK		((volatile unsigned int *)0xf8000008)
#define PSS_RST_CTRL	((volatile unsigned int *)0xf8000200)

void
reset_cpu ( void )
{
	*SLCR_UNLOCK = 0xdf0d;
	*PSS_RST_CTRL = 1;
}

/* ---------------------------------- */

/* Initialize the network device */
/* only called if WANT_NET */
int
board_net_init ( void )
{
#ifdef WANT_NET
        return eth_init ();
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
	// dallas_test ();
}

/* THE END */
