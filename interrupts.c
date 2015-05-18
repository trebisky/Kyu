/* interrupts.c
 * - was trap.c on the x86
 */

#include <kyu.h>
#include <thread.h>

#include "omap_ints.h"

extern struct thread *cur_thread;

long in_interrupt;
struct thread *in_newtp;

void
interrupt_init ( void )
{
        in_interrupt = 0;
        in_newtp = (struct thread *) 0;
}

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

	if ( valid_ram_address ( get_sp() ) ) {
	    oldsp = get_sp();
	    set_sp ( EVIL_STACK_BASE );
	    printf ("Switched to evil stack from %08x\n", oldsp );
	}

	printf ( "\n" );
	printf ( "%s in thread %s\n", msg, cur_thread->name );
	show_thread_regs ( cur_thread );
	spin();

	thr_fault ( code );

	/* Should return with a different thread marked
	 * to be run
	 * XXX - move into thread.c
	 */
	if ( in_newtp ) {
	    tp = in_newtp;
	    in_newtp = (struct thread *) 0;
	    in_interrupt = 0;
	    change_thread_int ( tp );
	    /* NOTREACHED */

	    printf ( "evil_exception , change_thread" );
	    spin ();
	}

#ifdef notdef
	kyu_startup ();	/* XXX */
	spin ();	/* XXX */
#endif
}

void
do_undefined_instruction ( void )
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

	printf ( "irq_hookup: %d, %08x, %d\n", irq, func, (int) arg );

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

	in_interrupt = 1;

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

	/* By this time a couple of things could
	 * make us want to return to a different
	 * thread than what we were running when
	 * the interrupt happened.
	 *
	 * First, when we call the timer hook, the
	 * user routine could unblock a semaphore,
	 * or unblock a thread, or some such thing.
	 *
	 * Second, when we unblock a thread on the
	 * delay list, we may have a hot new candidate.
	 * XXX - move into thread.c
	 */
	if ( in_newtp ) {
	    tp = in_newtp;
	    in_newtp = (struct thread *) 0;
	    in_interrupt = 0;
	    change_thread_int ( tp );
	    /* NOTREACHED */

	    panic ( "irq int , change_thread" );
	}

	in_interrupt = 0;
	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */

	panic ( "do_irq, resume" );
}

/* THE END */
