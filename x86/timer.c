/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* timer.c
 * Kyu project
 * Tom Trebisky  9/26/2001
 *
 * Driver for the x86 clock timer.
 * In skidoo this was part of trap.c
 */

#include "kyu.h"
#include "thread.h"
#include "kyulib.h"
#include "intel.h"

void timer_int ( void );

/*
long in_interrupt;
struct thread *in_newtp;
struct thread *timer_wait;
*/

/* go at 100 Hz.
 * (we actually get 100.0067052 or so)
#define TIMER_RATE	(1193180 / 100 )
 */

#define TIMER_INT	0
#define TIMER_CRYSTAL	1193180

static void
intel_timer_rate_set ( int hz )
{
	int preload;

	preload = TIMER_CRYSTAL / hz;

	/* The 8253 is a poor little 16 bit counter,
	 * so we cannot ask for rates slower than
	 * around 20 Hz or so.
	 * Also the primary clock is this weird
	 * 1.19318 Mhz thing, so we cannot get exact
	 * 100 Hz, or 1000 Hz (an exact 20 is about it).
	 */
	outb_p ( 0x36, 0x43 );
	outb_p ( preload & 0xff, 0x40 );
	outb_p ( preload >> 8, 0x40 );
}

/* initialize the timer.
 */
void
intel_timer_init ( void )
{
	timer_wait = (struct thread *) 0;

#ifdef notdef
	rate = TIMER_RATE;
	outb_p ( 0x36, 0x43 );
	outb_p ( rate & 0xff, 0x40 );
	outb_p ( rate >> 8, 0x40 );
#endif

	intel_timer_rate_set ( DEFAULT_TIMER_RATE );

	irq_hookup_t ( TIMER_INT, timer_int );

	/* enable timer interrupts
	 */
	pic_enable ( TIMER_INT );
}

/* field a timer interrupt.
 */
void
timer_int ( void )
{

#ifdef WANT_BENCH
extern unsigned long long ts1;
extern unsigned long long ts2;
extern unsigned long ts_dt;
	rdtscll ( ts2 );
	ts_dt = ts2 - ts1;
#endif

	outb ( OCW_EOI, PIC_1_EVEN );

	timer_tick ();

#ifdef not_any_more
	struct thread *tp;

	in_interrupt = 1;

	if ( timer_wait ) {
	    --timer_wait->delay;
	    while ( timer_wait->delay == 0 ) {
		tp = timer_wait;
		timer_wait = timer_wait->wnext;
		if ( thread_debug ) {
		    printf ( "Remove wait: %s\n", tp->name );
		}
	    	thr_unblock ( tp );
	    }
	}

	/* -------------------------------------------
	 * From here down should become the epilog
	 * of every interrupt routine that could
	 * possibly do a thr_unblock()
	 */

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
	 */

	if ( in_newtp ) {
	    tp = in_newtp;
	    in_newtp = (struct thread *) 0;
	    in_interrupt = 0;
	    change_thread_int ( tp );
	    /* NOTREACHED */
	    panic ( "timer, change_thread" );
	}

	in_interrupt = 0;
	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */
	panic ( "timer, resume" );
#endif
}

/* THE END */
