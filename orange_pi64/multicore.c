/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * multicore.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/19/2017
 *
 * "It is the difference that makes the difference"  (Sal Glesser, Spyderco)
 */
#include <arch/types.h>

#include "kyu.h"
#include "h3_ints.h"
#include "arch/cpu.h"

#define CORE_0	0
#define CORE_1	1
#define CORE_2	2
#define CORE_3	3

#define CPUCFG_BASE     0x01f01c00
#define PRCM_BASE       0x01f01400

/* The CPUCFG registers are in a 1K section starting at 0x01f01c00
 * The ROM_START register is in here and is undocumented in the H3 datasheet.
 * See page 143 in the datasheet.
 * The overall address map is on page 84
 */

// #define ROM_START       ((vu32 *) 0x01f01da4)
#define ROM_START        ((vu32 *) (CPUCFG_BASE + 0x1A4))

/* We don't need these, but want to play with them */
#define PRIVA        ((vu32 *) (CPUCFG_BASE + 0x1A8))
#define PRIVB        ((vu32 *) (CPUCFG_BASE + 0x1AC))

#define GEN_CTRL        ((vu32 *) (CPUCFG_BASE + 0x184))

// #define DBG_CTRL1       ((u32 *) (CPUCFG_BASE + 0x1e4))

/* These two are suggestive */

/* This has one bit, set low to assert it */
#define CPU_SYS_RESET      ((vu32 *) (CPUCFG_BASE + 0x140))

/* This comes up set to 0x10f after reset they say,
 * and that is exactly how I find it.
 * I can set it to 0x101 and everything still runs fine
 * However, if I set it to 0x102, the system hangs,
 *  so I think I understand (and can ignore) this register.
 */
#define CPU_CLK_GATE       ((vu32 *) (CPUCFG_BASE + 0x144))
#define GATE_L2		0x100
#define GATE_CPU0	0x01
#define GATE_CPU1	0x02
#define GATE_CPU2	0x04
#define GATE_CPU3	0x08

#define POWER_OFF       ((u32 *) (PRCM_BASE + 0x100))

/* When any ARM cpu starts running, it will set the PC to 0xffff0000
 * This is the location of the H3 bootrom code.
 * This code does clever things for a second core.
 * In particular, it reads the processor affinity register and
 *  discovers that the low 2 bits are non-zero (the core number).
 * Once it discovers this, it loads the PC from the value at 0x01f01da4
 *
 * Also as a side note on the bootrom.  The data sheet says various
 *  contradictory things about it (such as it being 32K, 64K, and 96K)
 * What I discover is that it is in fact a 32K image at ffff0000
 * There is an exact second copy immediately following at ffff8000
 * This is probably a redundant address decode, but who knows.
 */

typedef void (*vfptr) ( void );

/* We just pick a location in low memory (the on chip ram)
 */
// #define SENTINEL	ROM_START
#define SENTINEL	(vu32 *) 4

/* This is in locore.S */
extern void secondary_start ( void );

/* ------------ */

static void start_test0 ( void );
static void start_test00 ( void );
static void start_test1 ( void );
static void start_test2 ( void );
// static void start_test3 ( void );
static void start_test4 ( void );
static void start_test5 ( void );

static void pulses ( int, int );
static void run_blink ( int, int, int );

static void core_demo1 ( int, void * );
static void core_demo2 ( int, void * );
// static void core_demo3 ( int, void * );
static void core_demo4 ( int, void * );

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

static cfptr	core_func[NUM_CORES];
static void *	core_arg[NUM_CORES];
static int	core_run[NUM_CORES] = { 1, 0, 0, 0 };

static int wait_core ( void );
static void launch_core ( int );

/* Start a single core.
 * This is the public entry point.
 */
void
h3_start_core ( int core, cfptr func, void *arg )
{
	int stat;

	/* We won't try to restart core 0 */
	if ( core < 1 || core >= NUM_CORES )
	    return;

	if ( core_run[core] )
	    return;

	*SENTINEL = 0xdeadbeef;

	/* Added 10-13-2018 */
	core_func[core] = func;

	// printf ( "Starting core %d ...\n", core );
	launch_core ( core );
	// printf ( "Waiting for core %d ...\n", core );

	// watch_core ();
	stat = wait_core ();
	if ( ! stat ) {
	    printf ( "** Core %d failed to start\n", core );
	    return;
	}

	core_run[core] = 1;

	// if ( stat ) printf ( " Core %d verified to start\n", core );

	*ROM_START = 0;
}

/* When a core comes alive, this is the first C code it runs.
 */
void
run_newcore ( int core )
{
	vu32 *sent;

	/* Clear flag to indicate we are running */
	sent = SENTINEL;
	*sent = 0;
	// spin_red ();

	(*core_func[core]) ( core, core_arg[core]  );
}

/* Most of the time a core takes 30 counts to start */
#define MAX_CORE	100

static int
wait_core ( void )
{
	vu32 *sent;
	int i;

	sent = SENTINEL;

	for ( i=0; i<MAX_CORE; i++ ) {
	    if ( *sent == 0 ) {
		// printf ( "Core started in %d\n", i );
		return 1;
	    }
	}
	return 0;
}

/* Manipulate the hardware to power up a core
 */
static void
launch_core ( int core )
{
        vu32 *reset;
        u32 mask = 1 << core;

        reset = (vu32 *) ( CPUCFG_BASE + (core+1) * 0x40);
	// printf ( "-- reset = %08x\n", reset );

        *ROM_START = (u32) secondary_start;  /* in locore.S */

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */
        *POWER_OFF &= ~mask;            /* power on */
	// thr_delay ( 2 );
	delay_ms ( 2 );

        *reset = 3;			/* take out of reset */
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* Test fixtures below here */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/* This gets called by the test menu
 */
void
test_core ( void )
{
	printf ( "Testing multicore startup\n" );
	// printf ( "Testing multicore startup 1\n" );
	// thr_delay ( 1000 );
	// printf ( "Testing multicore startup 2\n" );

#ifdef notdef
	int reg;
	get_CPSR ( reg );
	printf ( "CPSR  = %08x\n", reg );

	get_SCTRL ( reg );
	printf ( "SCTRL = %08x\n", reg );
#endif

#ifdef notdef
	reg = h3_spin_check ( 0 );
	printf ( "Spin: %d\n", reg );

	h3_spin_lock ( 0 );

	reg = h3_spin_check ( 0 );
	printf ( "Spin: %d\n", reg );
#endif

	// start_test0 ();
	// start_test00 ();
	// start_test1 ();

	// test H3 spin locks
	// start_test2 ();

	// test ARM spin locks
	//  (broken on H3)
	// start_test3 ();

	// test interrupt between cores 
	// OK 10-13-2018
	start_test4 ();

	// 10-12-2018 OK
	// start_test5 ();
}

/* -------------------------------------------------------------------------------- */
/* Demo 0, test LED blink */
/* Works fine 10-13-2018, 4 sec period, 2 blinks */

static void
start_test0 ( void )
{
	run_blink ( 2, 100, 4000 );
	for ( ;; )
	    ;
}

/* -------------------------------------------------------------------------------- */
/* Demo 00 - sort of simplest possible core launch */
/* Used 10-13-2018 to fix regression bug */

static void
core_demo00 ( int core, void *xxx )
{
	// printf ( "Core %d blinking\n", core );

	/* Blink red status light */
	run_blink ( core+1, 100, 4000 );
}

static void
start_test00 ( void )
{
	h3_start_core ( CORE_1, core_demo00, NULL );
}

/* -------------------------------------------------------------------------------- */
/* Demo 1, hard delay loop to blink LED patterns.
 * Starts all 3 extra cores, and they blink 2 3 4 2 3 4 ....
 */

static void
core_demo1 ( int core, void *xxx )
{
	// printf ( "Core %d blinking\n", core );

	/* Blink red status light */
	run_blink ( core+1, 100, 4000 );
}

static void
start_test1 ( void )
{
	h3_start_core ( CORE_1, core_demo1, NULL );
	thr_delay ( 1000 );
	h3_start_core ( CORE_2, core_demo1, NULL );
	thr_delay ( 1000 );
	h3_start_core ( CORE_3, core_demo1, NULL );
}

/* -------------------------------------------------------------------------------- */
/* Demo 2,
 *  cores all wait on spin lock
 *  "master" core signals them when to blink.
 */

static void
core_demo2 ( int core, void *xx )
{
	// printf ( "Core %d blinking\n", core );

	for ( ;; ) {
	    h3_spin_lock ( core );
	    pulses ( core+1, 100 );
	}
}

static void
start_test2 ( void )
{
	/* acquire all the locks */
	h3_spin_lock ( 1 );
	h3_spin_lock ( 2 );
	h3_spin_lock ( 3 );

	/* start all the cores */
	h3_start_core ( CORE_1, core_demo2, NULL );
	h3_start_core ( CORE_2, core_demo2, NULL );
	h3_start_core ( CORE_3, core_demo2, NULL );

	for ( ;; ) {
	    thr_delay ( 2000 );
	    h3_spin_unlock ( 1 );
	    thr_delay ( 1000 );
	    h3_spin_unlock ( 2 );
	    thr_delay ( 1000 );
	    h3_spin_unlock ( 3 );
	}
}

/* -------------------------------------------------------------------------------- */

#ifdef notdef
/* Demo 3,
 *  cores all wait on spin lock
 *  "master" core signals them when to blink.
 *  like Demo 2, but uses ARM spinlocks
 * Or tries to anyway.
 * See locore.S -- I suspect these just don't work on the H3 chip.
 */

static int demo3_locks[4];

static void
core_demo3 ( int core, void *xx )
{
	for ( ;; ) {
	    spin_lock ( &demo3_locks[core] );
	    pulses ( core+1, 250 );
	}
}

/* XXX - still buggy */
static void
start_test3 ( void )
{
	// Initialize these all locked
	printf ( "Initializing lock\n" );
	demo3_locks[1] = 0;
	    spin_lock ( &demo3_locks[1] );
	printf ( "Initializing lock\n" );
	demo3_locks[2] = 0;
	    spin_lock ( &demo3_locks[2] );
	printf ( "Initializing lock\n" );
	demo3_locks[3] = 0;
	    spin_lock ( &demo3_locks[3] );

	printf ( "Starting other core[s]\n" );
	/* start the cores */
	// h3_start_core ( CORE_1, core_demo3, NULL );
	// h3_start_core ( CORE_2, core_demo3, NULL );
	h3_start_core ( CORE_3, core_demo3, NULL );

	for ( ;; ) {
	    //thr_delay ( 2000 );
	    //spin_unlock ( &demo3_locks[1] );
	    //thr_delay ( 1000 );
	    //spin_unlock ( &demo3_locks[2] );
	    thr_delay ( 4000 );
	    printf ( "Blip\n" );
	    spin_unlock ( &demo3_locks[CORE_3] );
	}
}

#endif

/* -------------------------------------------------------------------------------- */
/* Demo 4,
 *  Play with GIC and Software generated interrupts
 */

static void
test4_handler_1 ( int xxx )
{
	printf ( "BANG!\n" );
}

static void
test4_handler_2 ( int xxx )
{
	printf ( "BOOM!\n" );
}

static void
test4_handler_3 ( int xxx )
{
	printf ( "BONK!\n" );
}

/* The extra core itself, just sets up a handler and
 * waits for the interrupt.
 */
static void
core_demo4 ( int core, void *xxx )
{
	irq_hookup ( IRQ_SGI_3, test4_handler_3, 0 );

	// printf ( "Core %d started\n", core );

	/* Spin */
	for ( ;; ) {
	    delay_ms ( 1000 );
	}
}

/* Set up core 0 to receive BANG and BOOM.
 *  Core 0 interupts itself (BANG)
 *  Core 0 interupts itself (BOOM)
 * Start core 1
 *  Core 0 sends 10 interrupts to core 1 (BONK ...)
 *  Leaves core 1 spinning.
 */
static void
start_test4 ( void )
{
	int i;

	irq_hookup ( IRQ_SGI_1, test4_handler_1, 0 );
	irq_hookup ( IRQ_SGI_2, test4_handler_2, 0 );
	// irq_hookup ( IRQ_SGI_3, test4_handler_3, 0 );

	gic_soft_self ( SGI_1 );
	gic_soft ( SGI_2, CORE_0 );

	/* acquire the lock */
	// h3_spin_lock ( 1 );

	/* Start another core */
	h3_start_core ( CORE_1, core_demo4, NULL );
	thr_delay ( 500 );

	/* Interrupt core 1 */
	for ( i=0; i<10; i++ ) {
	    gic_soft ( SGI_3, CORE_1 );
	    thr_delay ( 100 );
	}
}

volatile int test5_flag;

static void
core_demo5 ( int core, void *xxx )
{
	test5_flag = 1;
	for ( ;; )
	    ;
}

/* Basic core startup */

static void
start_test5 ( void )
{
	int count;

	test5_flag = 0;
	printf ( "Waiting for core to start\n" );

	h3_start_core ( CORE_1, core_demo5, NULL );

	count = 1000;
	while ( count-- ) {
	    thr_delay ( 2 );
	    if ( test5_flag ) {
		printf ( "Core %d has started\n", CORE_1 );
		return;
	    }
	}

	printf ( "Core %d failed to start\n", CORE_1 );
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

static void
pulses ( int num, int ms )
{
	int i;

	for ( i=0; i<num; i++ ) {
	    status_on ();
	    delay_ms ( ms );
	    status_off ();
	    delay_ms ( ms );
	}
}

/* blink the red status LED on the orange pi
 */
static void
run_blink ( int num, int a, int b )
{
	b -= 2*num*a;

        for ( ;; ) {
	    pulses ( num, a );
	    delay_ms ( b );
        }
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* old cruft below here */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

#ifdef notdef

#define CORE_QUIET

void
run_newcore_OLD ( int core )
{
	vu32 *sent;

	/* Clear flag to indicate we are running */
	sent = SENTINEL;
	*sent = 0;

#ifdef notdef
	/* Read processor affinity register */
	asm volatile("mrc 15, 0, %0, cr0, cr0, 5" : "=r" (xcore) : : "cc");
	xcore &= 0x3;
#endif


#ifndef CORE_QUIET
	u32 sp;
	/* Get the value of sp */
	asm volatile ("add %0, sp, #0\n" :"=r"(sp));

	/* Makes a mess without synchronization */
	printf ( "Core %d running with sp = %08x\n", core, sp );
	printf ( "Core %d core (arg) = %08x\n", core, xcore );
	// printf ( "Core %d running\n", core );
#endif

#ifdef notdef
	if ( core != 1 ) {
	    // spin
	    for ( ;; )
		;
	}
#endif
	core_demo1 ( core );

}

void
watch_core ( void )
{
	vu32 *sent;
	int i;

	sent = SENTINEL;

	printf ( "\n" );
	printf ( "Sentinel addr: %08x\n", sent );

	for ( i=0; i<40; i++ ) {
	    thr_delay ( 100 );
	    // printf ( "Core status: %08x\n", *sent );
	    if ( *sent == 0 ) {
		printf ( "Core started !!\n" );
		return;
	    }
	}
	printf ( "** Core failed to start\n" );

}
#endif

void
test_reg ( vu32 *reg )
{
	u32 val;

	printf ( "Test REG, read %08x as %08x\n", reg, *reg );
	*reg = val = 0;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
	*reg = val = 0xdeadbeef;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
	*reg = val = 0;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
}

/* THE END */
