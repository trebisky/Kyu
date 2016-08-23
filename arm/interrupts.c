/* interrupts.c
 * - was trap.c on the x86
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include "hardware.h"
#include "omap_ints.h"

extern struct thread *cur_thread;

static vfptr data_abort_hook;

/* mark the offending thread and abandon it.
 *
 * For a data abort (which is far and away the common
 * issue on the ARM, the stack is perfectly healthy
 * and pertinent in fact.
 *
 * What we want to do is: thr_block ( FAULT );
 * but from interrupt level.
 *
 * An interrupt (or exception) saves all the registers
 * into "iregs", conveniently right at the start of
 * the thread structure.  An array of 17 on ARM.
 *
 * -----------------------
 *
 * To just return usually means the exception hits us again
 * immediately and we get a flood of stupid output.
 * Resetting the cpu (like U-boot) just puts us into a slower loop.
 * To spin is bad, but at least we get sensible output.
 */

char *mk_symaddr(int);

void
evil_exception ( char *msg, int code )
{
	int pc;

	printf ( "%s in thread %s\n", msg, cur_thread->name );

	show_thread_regs ( cur_thread );

#define ARM_FP	11
#define ARM_PC	15
	pc = cur_thread->iregs.regs[ARM_PC];
	printf ( "PC = %08x ( %s )\n", pc, mk_symaddr(pc) );

	unroll_fp ( cur_thread->iregs.regs[ARM_FP] );

	/* Let code in thread.c handle this */
	thr_suspend ( code );
}

void
data_abort_hookup ( vfptr new )
{
	data_abort_hook = new;
}

static int data_abort_flag;

/* If we don't bump the PC, we just return to faulted instruction
 * and get into a vicious loop
 */
void
data_abort_handler ( void )
{
	data_abort_flag = 1;
	cur_thread->iregs.regs[ARM_PC] += 4;
}

int
data_abort_probe ( unsigned long *addr )
{
	int val;

	data_abort_hookup ( data_abort_handler );

	data_abort_flag = 0;
	val = *addr;

	data_abort_hookup ( (vfptr) 0 );

	return data_abort_flag;
}

void do_undefined_instruction ( void )
{
	evil_exception ("undefined instruction", F_UNDEF);

	/* NOTREACHED */
	finish_exception ();
}

void do_software_interrupt ( void )
{
	evil_exception ("software interrupt", F_SWI);

	/* NOTREACHED */
	finish_exception ();
}

void do_prefetch_abort ( void )
{
	evil_exception ("prefetch abort", F_PABT);

	/* NOTREACHED */
	finish_exception ();
}

void do_data_abort ( void )
{
	if ( data_abort_hook ) {
	    (*data_abort_hook) ();
	    finish_exception ();
	} else
	    evil_exception ("data abort", F_DABT);
}

void do_not_used ( void )
{
	evil_exception ("not used", F_NU);

	/* NOTREACHED */
	finish_exception ();
}

/* evil for now */
void do_fiq ( void )
{
	evil_exception ("fast interrupt request", F_FIQ);

	/* NOTREACHED */
	finish_exception ();
}

/* -------------------------------------------- */
/* This is a pseudo exception to satisfy
 * linux code for a divide by zero.
 * It is called from:
 *  ./arch/arm/lib/lib1funcs.S:	bl	__div0
 *  ./arch/arm/lib/div64.S:	bl	__div0
 *
 * Note that on the ARM this does NOT happen
 *  at interrupt level, so it is easy to handle.
 * This works great.  8-14-2016
 */
void __div0 ( void )
{
	printf ("divide by zero");
	thr_block ( FAULT );
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
