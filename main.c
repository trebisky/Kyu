/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* main.c
 * Tom Trebisky  9/16/2001
 */

#include "kyu.h"
#include "kyulib.h"
/*
#include "intel.h"
*/
#include "thread.h"

void kern_startup ( void );
void user_init ( int );
void net_init ( void );

extern char *kyu_version;

#ifdef notdef
extern char __bss_start__;
extern char __bss_end__;

/* This works, but we do it in assembly in locore.S */
static void
clear_bss ( void )
{
	long *p;

	printf ( "bss start: %08x\n", &__bss_start__ );
	printf ( "bss  end : %08x\n", &__bss_end__ );

	p = (long *) &__bss_start__;
	while ( p < (long *) &__bss_end__ ) {
	    *p++ = 0;
	}
}
#endif

#ifdef WANT_FLOAT
static int
sqrt_i ( int arg )
{
	float farg = arg;
	float root;

	asm volatile ("vsqrt.f32 %0, %1" : "=w" (root) : "w" (farg) );

	return 10000 * root;
}

static int
sqrt_d ( int arg )
{
	double farg = arg;
	double root;

	asm volatile ("vsqrt.f64 %0, %1" : "=w" (root) : "w" (farg) );

	return 10000 * root;
}

void
arm_float ( void )
{
	int val;
	int num = 2;

	val = sqrt_i ( num );
	printf ( "Square root of %d is %d\n", num, val );
	val = sqrt_d ( num );
	printf ( "Square root of %d is %d\n", num, val );
}
#endif

// #define TRY_IDIV
#ifdef TRY_IDIV
/* probe ARM signed and unsigned division.
 * with -mcpu=cortex-a8 this will not even compile, I get:
 *   "selected processor does not support `sdiv r3,r3,r2' in ARM mode"
 * with -mcpu=cortex-a7 (on the Orange Pi), it compiles and runs fine.
 */
void
arm_idiv ( void )
{
	int ans;
	int val = 33;
	int div = 3;

	asm volatile ("sdiv %0, %1, %2" : "=r" (ans) : "r" (val), "r" (div) );
	asm volatile ("udiv %0, %1, %2" : "=r" (ans) : "r" (val), "r" (div) );

	printf ( "Sdiv = %d\n", ans );
}
#endif

/* This is the first bit of C code that runs in 32 bit mode.
 *
 * It runs with whatever stack we inherit from U-boot.
 * We abandon this as soon as we can (namely when this routine exits).
 * We then do the rest of the initialization in the sys_init thread.
 */
void
kern_startup ( void )
{
	unsigned long malloc_base;
	int sp;

#ifdef notdef
	// ensure core stack pointer valid
	// this works !!
	preinit_core ();
	test_core ();
#endif

	asm volatile ("add %0, sp, #0\n" :"=r"(sp));
	printf ( "Kyu starting with stack: %08x\n",  sp );

	// fail ();

	// emac_probe (); /* XXX */

	board_hardware_init ();
	malloc_base = ram_alloc ( MALLOC_SIZE );
	mem_malloc_init ( malloc_base, MALLOC_SIZE );
	// mem_malloc_init ( MALLOC_BASE, MALLOC_SIZE );

	hardware_init ();
	console_initialize ();

#ifdef WANT_FLOAT
	/* XXX -- floating point hijinks */
	fp_enable ();
	arm_float ();
	// arm_idiv ();
#endif

	// test_core ();  works here

	// printf ( "Kyu starting with stack: %08x\n",  get_sp() );
	// printf ( "Kyu starting with cpsr: %08x\n",  get_cpsr() );

	//sanity ();
	/* A sanity thing in case printf is broken */
	puts ( "Kyu starting" );


	/* debug broken linux printf */
#ifdef notdef
	xprintf ( "blow up\n" );
	puts ( "Made it !!" );	/* XXX */

	spin ();
#endif

	/* print initial banner on console
	 */
	printf ( "\n" );
	printf ( "Kyu %s starting\n", kyu_version );

	/* This will get 0x8ffce on the x86 */
	/* Gets 0x9E72E768 on the ARM (from U-boot) */
	/*
	printf ( "Stack at %08x\n", get_sp() );
	*/

	thr_init ();	/* prep thread system */
	thr_sched ();	/* set threads running */

	/* Should set up and run sys thread,
	 * so if we get here, something is seriously
	 * wrong.
	 */
	panic ( "thr_sched" );
}

/* This is the first thread.
 * (many things expect to run with a valid current_thread).
 *
 * Be careful about the order in which things get initialized.
 * for example, we must have the thread system set up before
 * we turn on interrupts.
 */
void
sys_init ( int xxx )
{
	// works here, but two changes needed.
	// must use _udelay() not thr_delay()
	// printf in core is not synchronized, so avoid
	// test_core ();

	/*
	printf ( "Sys thread starting with stack: %08x\n",  get_sp() );
	printf ( "Sys thread starting with cpsr: %08x\n",  get_cpsr() );
	*/

	/* XXX - a race! (maybe?)
	 * On the x86 threads are always launched with interrupts enabled.
	 * We should probably fix this in locore.S
	 * This won't matter unless some interrupt source is
	 * active on startup.
	 * This is not the case on the ARM
	 */
	cpu_enter ();

	/*
	printf ( "sys_init thread running\n" );
	printf ( "Stack at %08x\n", get_sp() );
	*/

	console_init ();
	// hardware_debug ();
	board_init ();
	timer_init ( DEFAULT_TIMER_RATE );

	/* enable interrupts */
	cpu_leave ();

	/* display the MMU setup handed us by U-Boot */
	// mmu_scan ( "From U-Boot " );

	/* Does this belong here ?? XXX */
	mmu_initialize ();
	// mmu_show ();

	gb_init_rand ( 0x163389 );

#ifdef WANT_DELAY
	/* Cannot do this until the timer is ticking
	 */
	delay_init (1);
#endif

#ifdef WANT_SLAB
	pork();
	mem_init ();
#endif

#ifdef WANT_LINUX_PCI
	pci_init ();
#endif

#ifdef notyet
	isapnp_init ();
#endif

#ifdef WANT_NET
	net_init ();
#endif

/* These things must be after net_init() because
 * they will try to do tftp.
 */
#ifdef WANT_SHELL
	shell_init ();
#endif

	/* allow initialization of things that
	 * require the network to be alive.
	 */
	board_after_net ();

#ifdef notyet
	init_pcmcia_ds ();
#endif

	ram_show ();

	/* This is a nice idea, but doesn't always work
	 */
	printf ( "Kyu %s running\n", kyu_version );
	/*
	printf ( "Sys thread finished with stack: %08x\n",  get_sp() );
	printf ( "Sys thread finished with cpsr: %08x\n",  get_cpsr() );
	*/

	// test_core (); works here

	(void) thr_new ( "user", user_init, (void *) 0, PRI_USER, 0 );
}

/* The usual case is that this is not defined and
 * user_init() is provided by user.c
 */

#ifndef WANT_USER

static int dregs[17];

void
XXuser_init ( int xx )
{
	get_regs ( dregs );
	dregs[16] = get_cpsr ();
	show_regs ( dregs );

	printf ( "User thread running with stack: %08x\n",  get_sp() );
	printf ( "User thread running with cpsr: %08x\n",  get_cpsr() );
	enable_irq ();

	    delay1 ();

	get_regs ( dregs );
	dregs[16] = get_cpsr ();
	show_regs ( dregs );

	printf ( "User thread running with stack: %08x\n",  get_sp() );
	printf ( "User thread running with cpsr: %08x\n",  get_cpsr() );

	printf ( "First Ticks: %d\n", get_timer_count_t() );
	delay1 ();
	printf ( "More Ticks: %d\n", get_timer_count_t() );

#ifdef notdef
	for ( ;; ) {
	    delay1 ();
	    printf ( "Ticks: %d\n", get_timer_count_t() );
	    /*
	    spin ();
	    printf ( "Escape 1\n" );
	    spin ();
	    printf ( "Escape 2\n" );
	    spin ();
	    printf ( "Escape 3\n" );
	    spin ();
	    printf ( "Escape 4\n" );
	    spin ();
	    printf ( "Escape 5\n" );
	    */
	}
#endif

#ifdef notdef
	serial_int_setup ();

        for ( ;; ) {
            serial_int_test ();
            delay1 ();
        }

	/* Generate data abort */
	interrupt_test_dabort ();
#endif

	/*
	gpio_test1 ();
	*/
	gpio_test2 ();
}

/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( int xx )
{
	/* XXX */
	printf ( "Stub thread: user_init\n" );
}
#endif

/* THE END */
