/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * cpu.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/19/2017
 *
 * "It is the difference that makes the difference"  (Sal Glesser, Spyderco)
 */

#define CPUCFG_BASE     0x01f01c00
#define PRCM_BASE       0x01f01400

/* The CPUCFG registers are in a 1K section starting at 0x01f01c00
 * The ROM_START register is in here and is undocumented in the H3 datasheet.
 * See page 143 in the datasheet.
 * The overall address map is on page 84
 */

// #define ROM_START       ((volatile unsigned long *) 0x01f01da4)
#define ROM_START        ((volatile unsigned long *) (CPUCFG_BASE + 0x1A4))

/* We don't need these, but want to play with them */
#define PRIVA        ((volatile unsigned long *) (CPUCFG_BASE + 0x1A8))
#define PRIVB        ((volatile unsigned long *) (CPUCFG_BASE + 0x1AC))

#define GEN_CTRL        ((volatile unsigned long *) (CPUCFG_BASE + 0x184))

// #define DBG_CTRL1       ((unsigned long *) (CPUCFG_BASE + 0x1e4))

/* These two are suggestive */

/* This has one bit, set low to assert it */
#define CPU_SYS_RESET      ((volatile unsigned long *) (CPUCFG_BASE + 0x140))

/* This comes up set to 0x10f after reset they say,
 * and that is exactly how I find it.
 * I can set it to 0x101 and everything still runs fine
 * However, if I set it to 0x102, the system hangs,
 *  so I think I understand (and can ignore) this register.
 */
#define CPU_CLK_GATE       ((volatile unsigned long *) (CPUCFG_BASE + 0x144))
#define GATE_L2		0x100
#define GATE_CPU0	0x01
#define GATE_CPU1	0x02
#define GATE_CPU2	0x04
#define GATE_CPU3	0x08

#define POWER_OFF       ((unsigned long *) (PRCM_BASE + 0x100))

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
#define SENTINEL	(volatile unsigned long *) 4

/* This is in locore.S */
extern void secondary_start ( void );

static void test_one ( int );
static void launch_core ( int );

static void start_test1 ( void );
static void start_test2 ( void );

static void pulses ( int, int );
static void run_blink ( int, int, int );

/* This gets called by the test menu
 */
void
test_core ( void )
{
	int reg;

#ifdef notdef
	asm volatile ("mrs %0, cpsr\n" : "=r"(reg) : : "cc" );
	printf ( "CPSR  = %08x\n", reg );
	asm volatile ("mrs %0, cpsr\n" : "=r"(reg) : : "cc" );
	printf ( "CPSR  = %08x\n", reg );
	printf ( "CPSR  = %08x\n", get_cpsr() );
	printf ( "CPSR  = %08x\n", get_cpsr() );

	/* SCTRL */
        asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg) : : "cc");
	printf ( "SCTRL = %08x\n", reg );
#endif

	reg = spin_check ( 0 );
	printf ( "Spin: %d\n", reg );

	spin_lock ( 0 );

	reg = spin_check ( 0 );
	printf ( "Spin: %d\n", reg );

	// start_test1 ();
	start_test2 ();
}

/* -------------------------------------------------------------------------------- */
/* Demo 1, hard delay loop to blink LED patterns */

static void
start_test1 ( void )
{
	test_one ( 1 );
	thr_delay ( 1000 );
	test_one ( 2 );
	thr_delay ( 1000 );
	test_one ( 3 );
}

static void
core_demo1 ( int core )
{
	// printf ( "Core %d blinking\n", core );

	/* Blink red status light */
	run_blink ( core+1, 100, 4000 );
}

/* -------------------------------------------------------------------------------- */
/* Demo 2,
 *  cores all wait on spin lock
 *  "master" core signals them when to blink.
 */

static void
start_test2 ( void )
{
	/* acquire all the locks */
	spin_lock ( 1 );
	spin_lock ( 2 );
	spin_lock ( 3 );

	/* start all the cores */
	test_one ( 1 );
	test_one ( 2 );
	test_one ( 3 );

	for ( ;; ) {
	    thr_delay ( 2000 );
	    spin_unlock ( 1 );
	    thr_delay ( 1000 );
	    spin_unlock ( 2 );
	    thr_delay ( 1000 );
	    spin_unlock ( 3 );
	}
}

static void
core_demo2 ( int core )
{
	// printf ( "Core %d blinking\n", core );

	for ( ;; ) {
	    spin_lock ( core );
	    pulses ( core+1, 100 );
	}
}

/* Most of the time a core takes 30 counts to start */
#define MAX_CORE	100

int
wait_core ( void )
{
	volatile unsigned long *sent;
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

/* Start a single core */
static void
test_one ( int cpu )
{
	int stat;

	*SENTINEL = 0xdeadbeef;

	printf ( "Starting core %d ...\n", cpu );
	launch_core ( cpu );

	// watch_core ();
	stat = wait_core ();
	if ( stat )
	    printf ( " Core %d verified to start\n", cpu );
	else
	    printf ( "** Core %d failed to start\n", cpu );


	*ROM_START = 0;
}

/* Manipulate the hardware to power up a core
 */
static void
launch_core ( int cpu )
{
        volatile unsigned long *reset;
        unsigned long mask = 1 << cpu;

        reset = (volatile unsigned long *) ( CPUCFG_BASE + (cpu+1) * 0x40);
	// printf ( "-- reset = %08x\n", reset );

        *ROM_START = (unsigned long) secondary_start;  /* in locore.S */

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */
        *POWER_OFF &= ~mask;            /* power on */
	// thr_delay ( 2 );
	delay_ms ( 2 );

        *reset = 3;			/* take out of reset */
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

/* When a core comes alive, this is the first C code it runs.
 */
void
run_newcore ( int core )
{
	volatile unsigned long *sent;

	/* Clear flag to indicate we are running */
	sent = SENTINEL;
	*sent = 0;

	// core_demo1 ( core );
	core_demo2 ( core );
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
	volatile unsigned long *sent;

	/* Clear flag to indicate we are running */
	sent = SENTINEL;
	*sent = 0;

#ifdef notdef
	/* Read processor affinity register */
	asm volatile("mrc 15, 0, %0, cr0, cr0, 5" : "=r" (cpu) : : "cc");
	cpu &= 0x3;
#endif


#ifndef CORE_QUIET
	unsigned long sp;
	/* Get the value of sp */
	asm volatile ("add %0, sp, #0\n" :"=r"(sp));

	/* Makes a mess without synchronization */
	printf ( "Core %d running with sp = %08x\n", cpu, sp );
	printf ( "Core %d core (arg) = %08x\n", cpu, core );
	// printf ( "Core %d running\n", cpu );
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
	volatile unsigned long *sent;
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
test_reg ( volatile unsigned long *reg )
{
	unsigned long val;

	printf ( "Test REG, read %08x as %08x\n", reg, *reg );
	*reg = val = 0;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
	*reg = val = 0xdeadbeef;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
	*reg = val = 0;
	printf ( "Test REG, set %08x: read %08x as %08x\n", val, reg, *reg );
}

/* THE END */
