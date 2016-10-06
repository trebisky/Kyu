/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* interrupts.c
 * Tom Trebisky  9/26/2001
 *
 * This is interrupt handling stuff for kyu.
 *
 * In skidoo this was trap.c
 *  (trap.c also contained timer.c)
 */

#include "kyu.h"
#include "thread.h"
#include "kyulib.h"
#include "intel.h"

typedef void (*irq_fptr) ( void * );

extern struct thread *cur_thread;
extern int thread_debug;

extern struct gate vector_table[];
extern char vector_jump_0;
extern char vector_jump_1;
extern char vector_jump_end;

static void pic_init ( void );
static void vector_init ( void );
static void irq_init ( void );

void bogus_int ( void );
void unini_int ( void );

void diverr_int ( void );
void debug_int ( void );
void nmi_int ( void );
void debug3_int ( void );
void overflow_int ( void );
void bounds_int ( void );
void invalid_int ( void );
void dna_int ( void );
void double_int ( void );
void overrun_int ( void );
void invtss_int ( void );
void segment_int ( void );
void stack_int ( void );
void gpf_int ( void );
void page_int ( void );
void numeric_int ( void );

/* A PIC is a programmable interrupt controller.
 * (the venerable Intel 8259A)
 *
 * Typically implemented these days as part of a
 * high integration chipset, but it still gets
 * programmed the same.  In any modern system,
 * there is a pair of them (and really modern systems
 * have an APIC).  I found my clues in
 * the "Intel Microsystems Components Handbook"
 * (1985, Volume 1 for the 8259A,
 *  pages 2-89 thru 2-106)
 *
 * A pair of these has 15 possible interrupt lines
 * (one line is used to cascade the slave PIC
 * into the master).
 *
 * Lines are connected as follows:
 *	0 = timer
 *	1 = keyboard
 *	2 = cascade to second PIC
 *	3 = COM2	(usually)
 *	4 = COM1	(usually)
 *	5 = open (maybe das16)
 *	6 = ???
 *	7 = ???
 *
 *	8 = real time clock
 *	9 = ???
 *	10 = open (maybe PCI network)
 *	11 = open (maybe PCI network)
 *	12 = ???
 *	13 = ???
 *	14 = primary IDE
 *	15 = secondary IDE
 */

/* The master, even and odd
 */
#define PIC_1_EVEN	0x20
#define PIC_1_ODD	0x21

/* The slave, even and odd
 */
#define PIC_2_EVEN	0xA0
#define PIC_2_ODD	0xA1

/* sending this to the even port is a
 * non-specific EOI (end of interrupt)
 * (if you don't send this, you get just
 *  one interrupt, then never any more.)
 */
#define OCW_EOI		0x20

/* We can tell the PIC to generate vectors for
 * IRQ's anwhere, but we would be fools to use
 * the first 32 vectors that are reserved for
 * cpu exceptions, and there is no reason to put the
 * 16 vectors the PIC will use further along,
 * so we do this:
 */
#define IRQ_BASE_VECTOR	0x20

static char irq_mask[] =
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/* enable interrupts from some IRQ
 * (this is an OCW thing using the ODD register).
 * We clear the bit in the mask for the IRQ
 *	we want to enable.
 */
void
pic_enable ( int irq )
{
	int m;

	if ( irq < 8 ) {
	    m = ~irq_mask[irq];
	    outb_p ( inb_p ( PIC_1_ODD ) & m, PIC_1_ODD );
	} else {
	    m = ~irq_mask[irq-8];
	    outb_p ( inb_p ( PIC_2_ODD ) & m, PIC_2_ODD );
	}
}

/* disable interrupts from some IRQ
 * We set the bit for the IRQ we want to disable.
 */
void
pic_disable ( int irq )
{
	int m;

	if ( irq < 8 ) {
	    m = irq_mask[irq];
	    outb_p ( inb_p ( PIC_1_ODD ) | m, PIC_1_ODD );
	} else {
	    m = irq_mask[irq-8];
	    outb_p ( inb_p ( PIC_2_ODD ) | m, PIC_2_ODD );
	}
}

/* ---------------------------------- */

long in_interrupt;
struct thread *in_newtp;

void
trap_init ( void )
{
	pic_init ();
	vector_init ();
	irq_init ();
	in_interrupt = 0;
	in_newtp = (struct thread *) 0;
}

/* ---------------------------------- */
/* nasty bit of hackery 3/31/2005 */

static vfptr diverr_hook;

void
diverr_hookup ( vfptr new )
{
	diverr_hook = new;
}

/* Now for some monkey business.
 * We are patching into a table of instructions
 * generated in locore.S to install interrupt
 * handling routines.
 * We do a lot of sanity checking here in
 * vector_init to make sure our assumptions
 * are not being violated, but even then this
 * seems a bit iffy.
 * Before this is called, the routine "unini_int"
 * is installed as a handler.
 */

#define VJUMP_ENTRY	10
#define VJUMP_SIZE	48
#define VJUMP_OPCODE	0x68
#define VJUMP_PANIC

void
vector_hookup ( int vec, vfptr func )
{
	vfptr *vp;
	void *jp = (void *) &vector_jump_0;

	if ( vec < 0 || vec >= VJUMP_SIZE )
	    return;

	jp += vec * VJUMP_ENTRY;
	vp = (vfptr *) (jp+1);
	*vp = func;
}

static void
irq_hookup_t ( int vec, vfptr func )
{
	vector_hookup ( vec + IRQ_BASE_VECTOR, func );
}

static void
vector_init ( void )
{
	void *jp0 = (void *) &vector_jump_0;
	void *jp1 = (void *) &vector_jump_1;
	void *jpe = (void *) &vector_jump_end;
	int ok = 1;

	if ( (jp1 - jp0) != VJUMP_ENTRY ) {
	    printf ( "vector jump table entry\n" );
	    ok = 0;
	}
	if ( (jpe - jp0 ) != VJUMP_SIZE*VJUMP_ENTRY ) {
	    printf ( "vector jump table size\n" );
	    ok = 0;
	}
	if ( *((char *) jp0) != VJUMP_OPCODE ) {
	    printf ( "vector jump table opcode\n" );
	    ok = 0;
	}

	if ( ! ok ) {
	    /*
	    show_vtable_1 ( 32, 4 );
	    show_vtable_2 ( 32, 4 );
	    show_vtable_2 ( 44, 4 );
	    */
	    panic ( "vector_init" );
	}

	diverr_hook = (vfptr) 0;

	/* Hookup handlers for all the
	 * exceptions.
	 * (XXX - Currently these use interrupt gates,
	 *  but we could fix that someday.
	 *  The only difference would be that if
	 *  we use trap gates, we would handle
	 *  the exception with interrupts enabled.)
	 */
	vector_hookup ( 0, diverr_int );
	vector_hookup ( 1, debug_int );
	vector_hookup ( 2, nmi_int );
	vector_hookup ( 3, debug3_int );
	vector_hookup ( 4, overflow_int );
	vector_hookup ( 5, bounds_int );
	vector_hookup ( 6, invalid_int );
	vector_hookup ( 7, dna_int );
	vector_hookup ( 8, double_int );
	vector_hookup ( 9, overrun_int );
	vector_hookup ( 10, invtss_int );
	vector_hookup ( 11, segment_int );
	vector_hookup ( 12, stack_int );
	vector_hookup ( 13, gpf_int );
	vector_hookup ( 14, page_int );
	/* XXX - no 15 */
	vector_hookup ( 16, numeric_int );
}

/* The PIC's are manipulated by ICW's and OCW's
 * Each PIC needs to get a 4 byte ICW sequence
 *	(initialization command words)
 * It should get this with interrupts masked.
 *
 * Once initialized, the PIC is controlled by
 * OCW's (operation command words)
 */

#define ICW1	0x11		/* Edge triggered (as per PC/AT) */
#define ICW2	IRQ_BASE_VECTOR	/* Vector base (upper 5 bits) */
#define ICW3M	0x04		/* Has Slave on IRQ2 */
#define ICW3S	0x04		/* Is Slave on IRQ2 */
#define ICW4	0x01		/* */

/* A note on vector numbers.
 * The first 32 vectors (0-31) are reserved by
 * the x86 processor for traps and exceptions.
 * The 8259 will then generate 16 vectors at the
 * base address given by ICW2, in this case
 * beginning with vector 32.
 * Unless we add some truly weird hardware,
 * that is all we get: 48 vectors out of
 * the possible 256 supported by the x86.
 * These means we can be somewhat frugal
 * with our interrupt gate allocation.
 */

static void
pic_init ( void )
{
	/* Send OCW's to mask off all
	 * interrupts.
	 */
	outb ( 0xff, PIC_1_ODD );
	outb ( 0xff, PIC_2_ODD );

	/* Here are ICW1, 2, 3, 4 */
	outb_p ( ICW1,  PIC_1_EVEN );
	outb_p ( ICW2,  PIC_1_ODD );
	outb_p ( ICW3M, PIC_1_ODD );
	outb_p ( ICW4,  PIC_1_ODD );

	outb_p ( ICW1,    PIC_2_EVEN );
	outb_p ( ICW2+8,  PIC_2_ODD );
	outb_p ( ICW3S,   PIC_2_ODD );
	outb_p ( ICW4,    PIC_2_ODD );
}

/* a divide error
 */
void
diverr_int ( void )
{
	/*
	printf ( "divide error\n" );
	*/

    	/*
	dump_l ( 0, 8 );
	printf ( "%08x: %08x\n", &diverr_hook, diverr_hook );
	*/

	if ( diverr_hook ) {
	    (*diverr_hook) ();
	} else {
	    panic ( "divide error" );
	}

	/* XXX - doesn't work to resume
	 * (now gets endless sequence of
	 * bogus interrupts).
	 */
	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
debug_int ( void )
{
	panic ( "debug trap" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
debug3_int ( void )
{
	panic ( "debug trap" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
nmi_int ( void )
{
	panic ( "nmi trap" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
overflow_int ( void )
{
	panic ( "overflow error" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
bounds_int ( void )
{
	panic ( "bounds error" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
invalid_int ( void )
{
	panic ( "invalid opcode" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
dna_int ( void )
{
	panic ( "device not available trap" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
double_int ( void )
{
	panic ( "double fault" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
overrun_int ( void )
{
	panic ( "coprocessor segment overrun" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
invtss_int ( void )
{
	panic ( "invalid TSS trap" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
segment_int ( void )
{
	panic ( "segment not present" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
stack_int ( void )
{
	panic ( "stack segment fault" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
gpf_int ( void )
{
	panic ( "general protection fault" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
page_int ( void )
{
	panic ( "page fault" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

void
numeric_int ( void )
{
	panic ( "numeric coprocessor fault" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

#ifdef notdef
/* ---------------------------------------------
 * Handler for divide by zero exception.
 * much of this is experimentation with
 * trap handling.
 * What we ultimately will want to do is to
 * suspend a thread that does a divide by zero
 * (unless we can think of something more clever),
 * and if we get an exception from the kernel
 * itself, enter the debugger.
 */

#define R_AX	3
#define R_DX	6
#define R_IP	7

void
divide_error ( long regs[] )
{

#ifdef DEBUG_DIV
	printf ("Trap: divide error\n");
	printf (" bp: %08x\n", regs[0] );
	printf (" SI: %08x\n", regs[1] );
	printf (" DI: %08x\n", regs[2] );
	printf (" ax: %08x\n", regs[3] );
	printf (" bx: %08x\n", regs[4] );
	printf (" cx: %08x\n", regs[5] );
	printf (" dx: %08x\n", regs[6] );
	printf (" ip: %08x\n", regs[7] );
	printf (" cs: %08x\n", regs[8] );
	printf (" fl: %08x\n", regs[9] );
	printf (" ??: %08x\n", regs[10] );

/* 0xFBF7 is	idivl %ebx	(idiv %ebx, %eax)
 * 0xF9F7 is	idivl %ecx	(idiv %ecx, %eax)
 * notice that the above are actually byte swapped,
 * but that is how they appear dumped as a short.
 * results always go to: %eax, %edx
 * always preceded by cltd instruction
 *	(CWD in the 80386 book)
 * (not that all of this is relevant)
 */
	dump_l ( regs[R_IP]-16, 4 );

#endif

#ifdef CONTINUE_DIV
	{
	short *sp;
	sp = (short *) regs[R_IP];
	if ( (*sp & 0xff) == 0xf7 ) {
	    regs[R_IP] += 2;
	    regs[R_AX] = 0;
	    regs[R_DX] = 0;
	    /* XXX */
	    return;
	}
	}
#endif

	dpanic ( "divide by zero" );
}
#endif

/* ---------------------------------------------
 */

long bogus_count = 0;

/* a bogus interrupt.
 * These days, this is something coming from a high vector
 * (something above 0x2f), that we never expect to see.
 */
void
bogus_int ( void )
{
	printf ( "bogus interrupt %d\n", bogus_count );
	panic ( "bogus interrupt" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

/* an uninitialized interrupt.
 * Something coming from vector range 0 - 0x2f
 * It could be a trap that I don't have documentation on
 * (likely on late model Pentia, since there are a bunch
 *  of reserved/undocumented traps in the 0x10 - 0x1f range).
 * Or it could be an IRQ that we have somehow enabled,
 * but not hooked a handler up for.
 */
void
unini_int ( void )
{
	printf ( "uninitialized interrupt\n" );

	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
}

/* -------------------------------------------- */

struct irq_info {
	irq_fptr func;
	void *	arg;
};

#define NUM_IRQ	16

static struct irq_info irq_table[NUM_IRQ];

/* field an irq interrupt.
 */
static void
irq_int_handler ( int irq )
{
	struct thread *tp;

	/* This really needs to be done this way.
	 * An interrupt from the first PIC needs to
	 * be acknowledged to that chip only, and
	 * an interrupt from the second PIC needs
	 * to be acked to both chips, or else you
	 * cease getting interrupts from some sources.
	 */
	if ( irq <= 7 ) {
	    outb ( OCW_EOI, PIC_1_EVEN );
	} else {
	    outb ( OCW_EOI, PIC_2_EVEN );
	    outb ( OCW_EOI, PIC_1_EVEN );
	}

#ifdef notdef
	/* Linux does it this way, but I never
	 * did try it.  It does sound better to
	 * do a specific EOI.
	 */
	if ( irq <= 7 ) {
	    outb ( 0x60 + irq, PIC_1_EVEN );
	} else {
	    outb ( 0x60 + (irq&7), PIC_2_EVEN );
	    outb ( 0x62, PIC_1_EVEN );
	}
#endif

	if ( ! cur_thread )
	    panic ( "irq int, cur_thread" );

	in_interrupt = 1;

	/* call the user handler
	 */
	(irq_table[irq].func)( irq_table[irq].arg );

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
	panic ( "irq int, resume" );
}

/* XXX - someday maybe I will think up something better.
 */
static void irq_1_int  ( void ) { irq_int_handler ( 1 ); }
static void irq_2_int  ( void ) { irq_int_handler ( 2 ); }
static void irq_3_int  ( void ) { irq_int_handler ( 3 ); }
static void irq_4_int  ( void ) { irq_int_handler ( 4 ); }
static void irq_5_int  ( void ) { irq_int_handler ( 5 ); }
static void irq_6_int  ( void ) { irq_int_handler ( 6 ); }
static void irq_7_int  ( void ) { irq_int_handler ( 7 ); }
static void irq_8_int  ( void ) { irq_int_handler ( 8 ); }
static void irq_9_int  ( void ) { irq_int_handler ( 9 ); }
static void irq_10_int ( void ) { irq_int_handler ( 10 ); }
static void irq_11_int ( void ) { irq_int_handler ( 11 ); }
static void irq_12_int ( void ) { irq_int_handler ( 12 ); }
static void irq_13_int ( void ) { irq_int_handler ( 13 ); }
static void irq_14_int ( void ) { irq_int_handler ( 14 ); }
static void irq_15_int ( void ) { irq_int_handler ( 15 ); }

/* function to handle uninitialized IRQ's
 */
void null_func ( void *arg )
{
	printf ("Unexpected IRQ %d interrupt\n", (int) arg );
}

static void
irq_init ( void )
{
	int i;

	for ( i=0; i<NUM_IRQ; i++ ) {
	    irq_table[i].func = null_func;
	    irq_table[i].arg = (void *)i;
	}

	/* irq 0 is timer (special) */
	irq_hookup_t ( 1, irq_1_int );
	irq_hookup_t ( 2, irq_2_int );
	irq_hookup_t ( 3, irq_3_int );
	irq_hookup_t ( 4, irq_4_int );
	irq_hookup_t ( 5, irq_5_int );
	irq_hookup_t ( 6, irq_6_int );
	irq_hookup_t ( 7, irq_7_int );
	irq_hookup_t ( 8, irq_8_int );
	irq_hookup_t ( 9, irq_9_int );
	irq_hookup_t ( 10, irq_10_int );
	irq_hookup_t ( 11, irq_11_int );
	irq_hookup_t ( 12, irq_12_int );
	irq_hookup_t ( 13, irq_13_int );
	irq_hookup_t ( 14, irq_14_int );
	irq_hookup_t ( 15, irq_15_int );

	/* It doesn't hurt to hook these up once
	 * and forever.  When we unhook the user
	 * handler, we also disable that level,
	 * so these routines don't get needlessly
	 * called.
	 */
}

/* Here is the user routine to connect/disconnect
 * a C routine to an interrupt.
 */
void
irq_hookup ( int irq, irq_fptr func, void *arg )
{

	/* exclude 1 (timer is special)
	 */
	if ( irq < 1 || irq > 15 )
	    panic ( "irq_hookup: not available" );

	if ( func ) {
	    irq_table[irq].func = func;
	    irq_table[irq].arg = arg;
	    pic_enable ( irq );
	} else {
	    pic_disable ( irq );
	    irq_table[irq].func = null_func;
	    irq_table[irq].arg = (void *)0;
	}
}

/* THE END */
