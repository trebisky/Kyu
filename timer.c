/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * timer.c -- board independent timer routines
 *
 * Simple driver for the DM timer
 * see section 20 of the am3359 TRM
 * especially control register
 *   description on pg 4113
 * 
 */
#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

void thread_tick ( void );

extern struct thread *cur_thread;

static int timer_rate;

static volatile long timer_count_t;
static volatile long timer_count_s;

// XXX - gets corrupted when static
static vfptr timer_hook;
// vfptr timer_hook;

#ifdef WANT_NET_TIMER
static vfptr net_timer_hook;
#endif

#ifdef notdef
/* XXX bogus debug-- */
void
timer_bogus ( void )
{
	printf ( "timer_hook: %016x = %016lx\n", &timer_hook, timer_hook );
	printf ( "net_timer_hook: %016x = %016lx\n", &net_timer_hook, net_timer_hook );
}
#endif

/* XXX - needed by imported linux code.
 */
// volatile unsigned long jiffies;

void
timer_init ( int rate )
{
	timer_rate = rate;

	timer_count_t = 0;
	timer_count_s = 0;

	timer_hook = (vfptr) 0;
#ifdef WANT_NET_TIMER
	net_timer_hook = (vfptr) 0;
#endif

	board_timer_init ( rate );
}

void
timer_hookup ( vfptr new )
{
	timer_hook = new;
}

#ifdef WANT_NET_TIMER
void
net_timer_hookup ( vfptr new )
{
	net_timer_hook = new;
}
#endif

/* Called by the timer interrupt
 *  at interrupt level */
void
timer_tick ( void )
{
	static int subcount;
	reg_t sp;

	// ++jiffies;

	/* These counts are somewhat bogus,
	 * but handy when first bringing up the timer
	 * and interrupt system.
	 *
	 * We used to make then available as global variables,
	 * but that was tacky.  Now we provide access to them
	 * via function calls.
	 */
	++timer_count_t;

	++subcount;
	if ( (subcount % timer_rate) == 0 ) {
	    ++timer_count_s;
	}

	if ( ! cur_thread )
	    panic ( "timer, cur_thread" );

	++cur_thread->prof;

	thread_tick ();

	/*
	printf ( "timer_hook: %016lx\n", timer_hook );
	printf ( "timer_count_t: %016lx\n", timer_count_t );
	printf ( "timer_count_s: %016lx\n", timer_count_s );
	timer_bogus ();
	get_SP ( sp );
	printf ( "In timer_tick, SP = %016lx\n", sp );
	printf ( "timer_hook: %016x = %016lx\n", &timer_hook, timer_hook );
	*/

	if ( timer_hook ) {
	    (*timer_hook) ();
	}

#ifdef WANT_NET_TIMER
	if ( net_timer_hook ) {
	    (*net_timer_hook) ();
	}
#endif
}

/* Public entry point.
 */
int
timer_rate_get ( void )
{
	return timer_rate;
}

/* Public entry point.
 * Set a different timer rate
 */
void
timer_rate_set ( int rate )
{
	timer_rate = rate;
	board_timer_rate_set ( rate );
}

/* Count in ticks (milliseconds) */
/* TCP timing uses this */
int
get_timer_count_t ( void )
{
	return timer_count_t;
}

/* Count in seconds */
int
get_timer_count_s ( void )
{
	return timer_count_s;
}

/* THE END */
