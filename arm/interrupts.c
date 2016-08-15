/* interrupts.c
 * - was trap.c on the x86
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include "hardware.h"
#include "omap_ints.h"

extern struct thread *cur_thread;

void
interrupt_test_dabort ( void )
{
	/* generate a data abort */
	int *p = (int *) 0;
	volatile int x = *p;
}

/* Try to generate a divide by zero without the compiler
 * optimizing it away.  Apparently the ARM does not care?
 */
volatile int val;

void
zdiv_test ( void )
{
	int i;

	val = 9;
	for ( i=5; i > -10; i-- )
	    val /= i;
}

/* XXX - someday mark the offending thread and let it be.
 * To just return usually means the exception hits us again
 * immediately and we get a flood of stupid output.
 * Resetting the cpu (like U-boot) just puts us into a slower loop.
 * To spin is bad, but at least we get sensible output.
 */
void
evil_exception ( char *msg, int code )
{
	struct thread *tp;
	unsigned long oldsp;

	/* Tell thread system we are in an interrupt */
	start_interrupt ();

	if ( valid_ram_address ( get_sp() ) ) {
	    oldsp = get_sp();
	    set_sp ( EVIL_STACK_BASE );
	    printf ("Switched to evil stack from %08x\n", oldsp );
	}

	printf ( "\n" );
	printf ( "%s in thread %s\n", msg, cur_thread->name );
	show_thread_regs ( cur_thread );

	/*
	spin();
	*/

	thr_show ();

	/* There is no plan B if thr_suspend() returns */
	for ( ;; ) {
	    printf ( "Suspending thread: %s\n", cur_thread->name );
	    thr_suspend ( code );
	    printf ("Uh oh\n");
	    spin();
	}

	/*
	finish_interrupt ();
	*/
}

void do_undefined_instruction ( void )
{
	evil_exception ("undefined instruction", F_UNDEF);
}

void do_software_interrupt ( void )
{
	evil_exception ("software interrupt", F_SWI);
}

void do_prefetch_abort ( void )
{
	evil_exception ("prefetch abort", F_PABT);
}

void do_data_abort ( void )
{
	evil_exception ("data abort", F_DABT);
}

void do_not_used ( void )
{
	evil_exception ("not used", F_NU);
}

/* evil for now */
void do_fiq ( void )
{
	evil_exception ("fast interrupt request", F_FIQ);
}

/* -------------------------------------------- */
/* This is a pseudo exception to satisfy
 * linux code for a divide by zero.
 * It is called from:
 *  ./arch/arm/lib/lib1funcs.S:	bl	__div0
 *  ./arch/arm/lib/div64.S:	bl	__div0
 *
 * Note that there is no reason to expect this call
 * to happen at interrupt level.
 */
void __div0 ( void )
{
	printf ("divide by zero");
	thr_block ( FAULT );
	// evil_exception ("divide by zero", F_FIQ);
}

/* -------------------------------------------- */

typedef void (*irq_fptr) ( void * );

struct irq_info {
	irq_fptr func;
	void *	arg;
};

static struct irq_info irq_table[NUM_INTS];

/* Here is the user routine to connect/disconnect
 * a C routine to an interrupt.
 */
void
irq_hookup ( int irq, irq_fptr func, void *arg )
{
	if ( irq < 0 || irq >= NUM_INTS )
	    panic ( "irq_hookup: not available" );

	/*
	printf ( "irq_hookup: %d, %08x, %d\n", irq, func, (int) arg );
	*/

	if ( func ) {
	    irq_table[irq].func = func;
	    irq_table[irq].arg = arg;
	    intcon_ena ( irq );
	} else {
	    intcon_dis ( irq );
	    irq_table[irq].func = (irq_fptr) 0;
	    irq_table[irq].arg = (void *) 0;
	}
}

#ifdef notdef
static void
special_debug ( void )
{
	printf ( "timer_int sp = %08x\n", get_sp () );
	printf ( "timer_int sr = %08x\n", get_cpsr () );
	printf ( "timer_int ssp = %08x\n", get_ssp () );
	printf ( "timer_int sr = %08x\n", get_cpsr () );
	printf ( "timer_int sp = %08x\n", get_sp () );
	spin ();
}
#endif


/* Interrupt handler, called at interrupt level
 * when the IRQ line indicates an interrupt.
 */
void do_irq ( void )
{
	int nint;
	struct thread *tp;

	if ( ! cur_thread )
	    panic ( "irq int, cur_thread" );

#ifdef notdef
	/* XXX */
	printf ( "\n" );
	printf ("Interrupt debug, sp = %08x\n", get_sp() );

	show_thread_regs ( cur_thread );
	printf ( "\n" );
	show_stack ( get_sp () );
	spin ();
#endif

	/* Tell Kyu thread system we are in an interrupt */
	start_interrupt ();

	nint = intcon_irqwho ();

	/* Always swamps output, but useful in
	 * dire debugging situations.
	printf ( "Interrupt %d\n", nint );
	printf ( "#" );
	*/

	if ( ! irq_table[nint].func ) {
	    /* Probably unrelated to current thread.
	     * This is pretty severe - XXX
	     */
	    printf ("Unknown interrupt request: %d\n", nint );
	    show_thread_regs ( cur_thread );
	    spin ();
	}

	/* call the user handler
	 */
	(irq_table[nint].func)( irq_table[nint].arg );

	intcon_irqack ();

	/* Tell Kyu thread system we are done with an interrupt */
	finish_interrupt ();

	panic ( "do_irq, resume" );
}

/* THE END */
