/* main.c
 * Tom Trebisky  9/16/2001
 */

#include "kyu.h"
/*
#include "intel.h"
*/
#include "thread.h"

void kern_startup ( void );
void user_init ( int );
void net_init ( void );

extern char *kyu_version;

/* This is the first bit of C code that runs in 32 bit mode.
 * it runs with a provisional stack at 0x90000 (so it will
 * be down in 0x8ffe0 or so while this runs).  We abandon
 * this is soon as we can and do the rest of the initialization
 * from the sys_init thread.
 */
void
kern_startup ( void )
{
	console_init ();

	/* print initial banner on VGA console
	 * no matter what (in case we get hosed before
	 * we can set up a serial console.
	 */
	printf ( "Kyu %s running\n", kyu_version );

	/* This will get 0x8ffce */
	/*
	printf ( "Stack at %08x\n", getsp() );
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
	/* XXX - a race on the x86, the thread is
	 * launched with interrupts enabled.
	 */
	cpu_enter ();

	/*
	printf ( "sys_init thread running\n" );
	printf ( "Stack at %08x\n", getsp() );
	*/

	hardware_init ();

	/* enable interrupts */
	cpu_leave ();

	/* Cannot do this until the timer is ticking
	 */
	delay_init (1);

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

#ifdef notyet
	init_pcmcia_ds ();
#endif

	/* This is a nice idea, but doesn't always work
	 */
	printf ( "\n" );
	printf ( "Kyu %s running\n", kyu_version );

	(void) thr_new ( "user", user_init, (void *) 0, PRI_USER, 0 );
}

#ifndef WANT_USER
/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( int xx )
{
	/* XXX */
}
#endif

/* THE END */
