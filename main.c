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
#include "thread.h"

// #include "intel.h"
#include "arch/cpu.h"

void kern_startup ( void );
void user_init ( long );
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

/* When additional cores (if there are any) start up,
 * they come here instead of kyu_startup() below.
 */
void
kyu_newcore ( int core )
{
	board_core_startup ( core );
}

/* This is the standard entry point to start a new core.
 */
void
new_core ( int core, cfptr func, void *arg )
{
	board_new_core ( core, func, arg );
}

#ifdef BOARD_FIRE3
extern char __bss_start__;
extern char __bss_end__;

/* Nanopi Fire3 */
struct sc_reset {
	vu32	reg0;
	vu32	reg1;
	vu32	reg2;
};

#define SC_RESET_BASE	((struct sc_reset *) 0xC0012000)

#define SC_GMAC_RESET	0x80000000	/* in reg1 */

void
check_bss ( void )
{
	char *p;
	int bad;
	struct sc_reset *scp = SC_RESET_BASE;

	printf ( "BSS start: %016x\n", &__bss_start__ );
	printf ( "BSS end: %016x\n", &__bss_end__ );

	printf ( "SC reset reg1 = %08x\n", scp->reg1 );
	/* We see 0x0c0402b0 -- so the GMAC is indeed reset */

	bad = 0;
	p = &__bss_start__;
	for ( ; p < &__bss_end__; p++ ) {
	    if ( *p ) {
		printf ( "BSS bad: %016x %x\n", p, *p );
		*p = 0;
		printf ( "BSS verify: %016x %x\n", p, *p );
		bad++;
	    }
	}

	if ( bad ) {
	    printf ( "%d BSS bytes corrupted\n", bad );
	    printf ( " -- GAME over\n" );
	    printf ( " *******************\n" );
	    printf ( " *******************\n" );
	    printf ( " *******************\n" );
	    printf ( " *******************\n" );
	    printf ( " *******************\n" );
	    printf ( " *******************\n" );
	    for ( ;; )
		;
	}

}
#endif

#ifdef notdef
void
tjt_startup ( void )
{
	puts ( "Zorro!" );
	for ( ;; ) ;
}
#endif

/* This is --almost-- the first bit of C code that runs in 32 bit mode.
 *  almost, because we call board_mmu_init() before calling this.
 *
 * It runs with whatever stack we inherit from U-boot.
 * We abandon this as soon as we can (namely when this routine exits).
 * We then do the rest of the initialization in the sys_init thread.
 */
void
kyu_startup ( void )
{
	unsigned long malloc_base;
	reg_t val;

#ifdef notdef
	// ensure core stack pointer valid
	// this works !!
	preinit_core ();
	test_core ();
#endif

	// check_bss ();
	// timer_bogus ();

// #define ARM64_TEST
#ifdef ARM64_TEST
	/* The following yields this:
	    Kyu starting with stack: 7b63bab0
	    Kyu starting with DAIF: 000003c0
	    So DAIF all are set to "1" (everything masked off)
	    DAIF is shifted left 6 bits.
	*/

	get_SP ( val );
	printf ( "Kyu starting with stack: %08x\n",  val );
	get_DAIF ( val );
	printf ( "Kyu starting with DAIF: %08x\n",  val );
	/*
	INT_unlock;
	get_DAIF ( val );
	printf ( "Kyu starting with DAIF: %08x\n",  val );
	*/
#endif

#ifdef ARCH_ARM32
	// asm volatile ("add %0, sp, #0\n" :"=r"(sp));
	get_SP ( val );
	printf ( "Kyu starting with stack: %08x\n",  val );
	get_CPSR ( val );
	printf ( "Kyu starting with cpsr: %08x\n",  val );
#endif

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

	/*
	get_SP ( val );
	printf ( "Kyu starting with stack: %08x\n",  val );
	get_CPSR ( val );
	printf ( "Kyu starting with cpsr: %08x\n",  val );
	*/

	/* A sanity thing in case printf is broken */
	// puts ( "Kyu starting" );


	/* debug broken linux printf */
#ifdef notdef
	xprintf ( "blow up\n" );
	puts ( "Made it !!" );	/* XXX */

	for ( ;; ) ;
#endif

	/* print initial banner on console
	 */
	// printf ( "\n" );
	printf ( "Kyu %s starting\n", kyu_version );
	// puts ( "Marco 3 !!" );
	// tjt_startup ();

	/* This will get 0x8ffce on the x86 */
	/* Gets 0x9E72E768 on the ARM (from U-boot) */
	/*
	printf ( "Stack at %08x\n", get_sp() );
	*/
	// show_my_regs ();

	// timer_bogus (); OK

	thr_init ();	/* prep thread system */
	thr_sched ();	/* set threads running */

	/* Should set up and run sys thread,
	 * so if we get here, something is seriously
	 * wrong.
	 */
	panic ( "thr_sched" );
}

#define BASIC_CHECKOUT_J
#ifdef BASIC_CHECKOUT_J

static void 
j_spin ( char *msg )
{
	printf ( " -- spinning: %s\n", msg );

	for ( ;; )
	    ;
}

static void
basic_checkout_j ( void )
{
	reg_t j_regs[NUM_IREGS];
	struct thread *ztp;

	ztp = (struct thread *) 0;
	printf ( "I regs: %016lx\n", &ztp->iregs );
	printf ( "J regs: %016lx\n", &ztp->jregs );
	printf ( "C regs: %016lx\n", &ztp->cregs );

	if ( save_j ( j_regs ) ) {
	    printf ( "J regs resumed\n" );
	    j_spin ( "J resumed" );
	    // return;
	}

	printf ( "J regs saved\n" );
	show_regs ( j_regs );

	// resume_jj ( j_regs );
	resume_j ( j_regs );
	/* Should not get here */

	j_spin ( "J saved" );
}

#endif /* BASIC_CHECKOUT_J */

// #define BASIC_CHECKOUT_C
#ifdef BASIC_CHECKOUT_C

void
thr_checkout ( int arg )
{
	// INT_lock;
	printf ( "Checkout thread: %d\n", arg );
	// timer_bogus ();
}

// extern vfptr timer_hook;

static unsigned long last_addr;
void
thr_cexit ( void )
{
	printf ( "Checkout thread exit, spinning\n" );
	// timer_bogus ();
	// printf ( "timer_hook: %016x = %016x\n", &timer_hook, timer_hook );
	// last_addr = timer_hook;
	for ( ;; )
	    ;
}

extern long vectors[16];

void
show_vectors ( void )
{
	printf ( "Vectors\n" );
	dump_ln ( vectors, 64 );
}

extern struct thread *cur_thread;

#define BASIC_STACK_SIZE 1024
static int basic_stack[BASIC_STACK_SIZE];

static void
basic_checkout_c ( void )
{
	reg_t cregs[4];
	reg_t val;

	get_SP ( val );
	printf ( "in basic_checkout_c with stack: %016x\n",  val );

	printf ( "Start basic checkout_c\n" );
	printf ( "  cur_thread = %08x\n", cur_thread );

	// show_vectors ();
	// timer_bogus ();

	cregs[0] = (long) 7;
	cregs[1] = (long) thr_checkout;
	cregs[2] = (unsigned long) &basic_stack[BASIC_STACK_SIZE];
	cregs[3] = (long) thr_cexit;

	printf ( "Launch basic checkout_c\n" );
	// timer_bogus ();

	resume_c ( cregs );
}
#endif

/* This is the first thread.
 * (many things expect to run with a valid current_thread).
 *
 * Be careful about the order in which things get initialized.
 * for example, we must have the thread system set up before
 * we turn on interrupts.
 */
void
sys_init ( long xxx )
{
	reg_t val;

	// works here, but two changes needed.
	// must use _udelay() not thr_delay()
	// printf in core is not synchronized, so avoid
	// test_core ();

#ifdef ARCH_ARM32
	get_SP ( val );
	printf ( "Sys thread starting with stack: %08x\n",  val );
	get_CPSR ( val );
	printf ( "Sys thread starting with cpsr: %08x\n",  val );
#endif

	/* XXX - a race! (maybe?)
	 * On the x86 threads are always launched with interrupts enabled.
	 * We should probably fix this in locore.S
	 * This won't matter unless some interrupt source is
	 * active on startup.
	 * This is not the case on the ARM, and interrupts should
	 * be disabled anway.
	 */
	// cpu_enter ();
	INT_lock;

	/*
	printf ( "sys_init thread running\n" );
	printf ( "Stack at %08x\n", get_sp() );
	*/
	console_init ();
	// hardware_debug ();

	board_init ();

	// timer_bogus (); OK to here

	// now done in board_init();
	// timer_init ( DEFAULT_TIMER_RATE );

	// See if this fixes our interrupt race on startup
	// (it does NOT)
	// delay_ms ( 2000 );

	/* enable interrupts */

	/* Added 9-28-2018 -- not interesting */
	// check_bss ();

	// validate resume_c
	// basic_checkout_c ();

	// validate save_j / resume_j 
	// basic_checkout_j ();

	// Actually this is bogus because every thread is
	//   launched with interrupts enabled.
	//   i.e. interrupts will already be on just
	//   by virtue of this thread being launched.
	printf ( "Enabling interrupts\n" );
	INT_unlock;

	// printf ( "BOGUS: Interrupts NOT enabled\n");

	/* display the MMU setup handed us by U-Boot */
	// mmu_scan ( "From U-Boot " );

	/* This displays MMU setup and runs some tests */
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
	puts ( "TJT - start net_init" );
	net_init ();
	puts ( "TJT - finished net_init" );
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

	//thr_show ();
	//printf ( "TJT done with sys_init\n" );
	// thr_debug ( 1 );
}

/* The usual case is that this is not defined and
 * user_init() is provided by user.c
 */

#ifndef WANT_USER

static int dregs[17];

void
XXuser_init ( int xx )
{
	int val;

	get_regs ( dregs );
	// dregs[16] = get_cpsr ();
#ifdef ARCH_ARM32
	get_CPSR ( val )
#endif
	dregs[16] = val;

	show_regs ( dregs );

	printf ( "User thread running with stack: %08x\n",  get_sp() );
	printf ( "User thread running with cpsr: %08x\n",  get_cpsr() );
	enable_irq ();

	    delay1 ();

	get_regs ( dregs );
	// dregs[16] = get_cpsr ();
#ifdef ARCH_ARM32
	get_CPSR ( val )
#endif
	dregs[16] = val;
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
