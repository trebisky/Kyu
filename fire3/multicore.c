/*
 * Copyright (C) 2018  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * multicore.c for the Samsung S5P6818 as in the NanoPi Fire3 board
 *
 * Tom Trebisky  10/14/2018
 */

#include "kyu.h"
#include "fire3_ints.h"
#include "arch/cpu.h"

#define CORE_0	0
#define CORE_1	1
#define CORE_2	2
#define CORE_3	3
#define CORE_4	4
#define CORE_5	5
#define CORE_6	6
#define CORE_7	7

/* This is in locore.S */
extern void secondary_start ( void );

/* ------------ */

static void run_blink ( int, int, int );

struct cpu_alive {
	vu32	jump_addr;
	vu32	cpuid;
	vu32	wakeup;
};

#define CPU_ALIVE_BASE	0xc0010230

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

static cfptr	core_func[NUM_CORES];
static void *	core_arg[NUM_CORES];
static int	core_run[NUM_CORES] = { 1, 0, 0, 0, 0, 0, 0, 0 };

/* Start a single core.
 * This is the public entry point.
 */
void
fire3_start_core ( int core, cfptr func, void *arg )
{
	int stat;

	/* We won't try to restart core 0 */
	if ( core < 1 || core >= NUM_CORES )
	    return;

	if ( core_run[core] )
	    return;

	/* Added 10-13-2018 */
	core_func[core] = func;

	// printf ( "Starting core %d ...\n", core );
	// launch_core ( core );
	// printf ( "Waiting for core %d ...\n", core );

	// watch_core ();
	// stat = wait_core ();
	stat = 0;
	if ( ! stat ) {
	    printf ( "** Core %d failed to start\n", core );
	    return;
	}

	core_run[core] = 1;

	// if ( stat ) printf ( " Core %d verified to start\n", core );

	// *ROM_START = 0;
}

/* When a core comes alive, this is the first C code it runs.
 */
void
run_newcore ( int core )
{
	volatile unsigned long *sent;

	/* Clear flag to indicate we are running */
	// sent = SENTINEL;
	// *sent = 0;
	// spin_red ();

	(*core_func[core]) ( core, core_arg[core]  );
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* Test fixtures below here */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------- */
/* Demo 1, test LED blink */
/*  should yield 4 sec period, 2 blinks */

static void
start_test1 ( void )
{
	printf ( "Press reset to end test\n" );

	run_blink ( 2, 100, 4000 );
}

/* -------------------------------------------------------------------------------- */
/* Demo 2 - sort of simplest possible core launch */

static void
core_demo2 ( int core, void *xxx )
{
	// printf ( "Core %d blinking\n", core );

	/* Blink red status light */
	run_blink ( core+1, 100, 4000 );
}

static void
start_test2 ( void )
{
	fire3_start_core ( CORE_1, core_demo2, NULL );
}

/* -------------------------------------------------------------------------------- */
/* Demo 3, hard delay loop to blink LED patterns.
 * Starts all 3 extra cores, and they blink 2 3 4 2 3 4 ....
 */

static void
core_demo3 ( int core, void *xxx )
{
	// printf ( "Core %d blinking\n", core );

	/* Blink red status light */
	run_blink ( core+1, 100, 4000 );
}

static void
start_test3 ( void )
{
	fire3_start_core ( CORE_1, core_demo3, NULL );
	thr_delay ( 1000 );
	fire3_start_core ( CORE_2, core_demo3, NULL );
	thr_delay ( 1000 );
	fire3_start_core ( CORE_3, core_demo3, NULL );
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/* This gets called by the test menu
 */
void
test_core ( void )
{
	u32	mpidr;

	printf ( "Testing multicore startup\n" );
	get_MPIDR ( mpidr );
	printf ( "MPIDR = %08x\n", mpidr );

	// Verify that blink works
	// start_test1 ();

	// Simple start of one core
	start_test2 ();
}

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

/* blink the green status LED on the Fire3
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

/* THE END */
