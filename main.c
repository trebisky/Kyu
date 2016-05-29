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

/* This is the first bit of C code that runs in 32 bit mode.
 * it runs with a provisional stack at 0x90000 (so it will
 * be down in 0x8ffe0 or so while this runs).  We abandon
 * this is soon as we can and do the rest of the initialization
 * from the sys_init thread.
 */
void
kern_startup ( void )
{
	console_initialize ();

	/*
	printf ( "Kyu starting with stack: %08x\n",  get_sp() );
	printf ( "Kyu starting with cpsr: %08x\n",  get_cpsr() );
	*/

	/* print initial banner on VGA console
	 * no matter what (in case we get hosed before
	 * we can set up a serial console.
	 */
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
	/*
	printf ( "Sys thread starting with stack: %08x\n",  get_sp() );
	printf ( "Sys thread starting with cpsr: %08x\n",  get_cpsr() );
	*/

	/* XXX - a race!
	 * Threads are always launched with interrupts enabled.
	 * This won't matter unless some interrupt source is
	 * active on startup.  This certainly won't be so on
	 * the am3359 in the BBB.
	 */
	cpu_enter ();

	/*
	printf ( "sys_init thread running\n" );
	printf ( "Stack at %08x\n", get_sp() );
	*/

	console_init ();
	hardware_init ();

	/* enable interrupts */
	cpu_leave ();

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

#ifdef WANT_SHELL
	shell_init ();
#endif

#ifdef notyet
	init_pcmcia_ds ();
#endif

	/* This is a nice idea, but doesn't always work
	 */
	printf ( "Kyu %s running\n", kyu_version );
	/*
	printf ( "Sys thread finished with stack: %08x\n",  get_sp() );
	printf ( "Sys thread finished with cpsr: %08x\n",  get_cpsr() );
	*/

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
