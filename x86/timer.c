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

extern struct thread *cur_thread;
extern int thread_debug;

void timer_int ( void );

long in_interrupt;
struct thread *in_newtp;

/* go at 100 Hz.
 * (we actually get 100.0067052 or so)
#define TIMER_RATE	(1193180 / 100 )
 */

#define TIMER_INT	0
#define TIMER_CRYSTAL	1193180

long timer_count_t = 0;
long timer_count_s = 0;

/* a kindness to the linux emulator.
 */
volatile unsigned long jiffies;

static vfptr timer_hook;
static vfptr net_timer_hook;

int timer_rate;

struct thread *timer_wait;

static void
timer_rate_set ( int hz )
{
	int preload;

	timer_rate = hz;
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
timer_init ( void )
{
	timer_hook = (vfptr) 0;
	net_timer_hook = (vfptr) 0;
	timer_wait = (struct thread *) 0;

#ifdef notdef
	rate = TIMER_RATE;
	outb_p ( 0x36, 0x43 );
	outb_p ( rate & 0xff, 0x40 );
	outb_p ( rate >> 8, 0x40 );
#endif

	timer_rate_set ( DEFAULT_TIMER_RATE );

	irq_hookup_t ( TIMER_INT, timer_int );

	/* enable timer interrupts
	 */
	pic_enable ( TIMER_INT );
}

/* Public entry point.
 */
int
tmr_rate_get ( void )
{
	return timer_rate;
}

/* Public entry point.
 * Set a different timer rate
 * XXX - really should use pic_enable/disable
 */
void
tmr_rate_set ( int hz )
{
	pic_disable ( TIMER_INT );
	/*
	cpu_enter ();
	*/

	timer_rate_set ( hz );

	pic_enable ( TIMER_INT );
	/*
	cpu_leave ();
	*/

}

/* field a timer interrupt.
 */
void
timer_int ( void )
{
	static int subcount;
	struct thread *tp;

#ifdef WANT_BENCH
extern unsigned long long ts1;
extern unsigned long long ts2;
extern unsigned long ts_dt;
	rdtscll ( ts2 );
	ts_dt = ts2 - ts1;
#endif

	outb ( OCW_EOI, PIC_1_EVEN );

	++jiffies;

	/* old baloney - really the count
	 * would do (if we chose to export it).
	 * My view is that accessing this externally
	 * is tacky, and folks should make method calls
	 * for timer facilities....
	 */
	++timer_count_t;
	++subcount;
	if ( (subcount % 100) == 0 ) {
	    ++timer_count_s;
	}

	if ( ! cur_thread )
	    panic ( "timer, cur_thread" );

	++cur_thread->prof;

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

	if ( timer_hook ) {
	    (*timer_hook) ();
	}

	if ( net_timer_hook ) {
	    (*net_timer_hook) ();
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
}

void
tmr_hookup ( vfptr new )
{
	timer_hook = new;
}

void
net_tmr_hookup ( vfptr new )
{
	net_timer_hook = new;
}

/* maintain a linked list of folks waiting on
 * timer delay activations.
 * In time-honored fashion, the list is kept in
 * sorted order, with the soon to be scheduled
 * entries at the front.  Each tick then just
 * needs to decrement the leading entry, and
 * when it becomes zero, one or more entries
 * get launched.
 *
 * Important - this must be called with
 * interrupts disabled !
 */
void
timer_add_wait ( struct thread *tp, int delay )
{
	struct thread *p, *lp;

	p = timer_wait;

	while ( p && p->delay <= delay ) {
	    delay -= p->delay;
	    lp = p;
	    p = p->wnext;
	}

	if ( p )
	    p->delay -= delay;

	tp->delay = delay;
	tp->wnext = p;

	if ( p == timer_wait )
	    timer_wait = tp;
	else
	    lp->wnext = tp;
	/*
	printf ( "Add wait: %d\n", delay );
	*/
}

/* THE END */
