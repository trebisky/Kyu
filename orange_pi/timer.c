/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* This is a driver for the H3 Allwinner timer module.
 *
 * This is also a Kyu timer driver, which makes this a two
 * part affair.  The hardware specific part in the first
 * part of the file, the general interface and facilities
 * in the second part.  Perhaps someday this should be
 * formally partitioned. but there are advantages to
 * keeping all the code together
 *
 * Tom Trebisky  1-6-2017
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include "h3_ints.h"

struct h3_timer {
	volatile unsigned int irq_ena;		/* 00 */
	volatile unsigned int irq_status;	/* 04 */
	int __pad1[2];
	volatile unsigned int t0_ctrl;		/* 10 */
	volatile unsigned int t0_ival;		/* 14 */
	volatile unsigned int t0_cval;		/* 18 */
	int __pad2;
	volatile unsigned int t1_ctrl;		/* 20 */
	volatile unsigned int t1_ival;		/* 24 */
	volatile unsigned int t1_cval;		/* 28 */
};

#define TIMER_BASE	( (struct h3_timer *) 0x01c20c00)

#define	CTL_ENABLE		0x01
#define	CTL_RELOAD		0x02		/* reload ival */
#define	CTL_SRC_32K		0x00
#define	CTL_SRC_24M		0x04

#define	CTL_PRE_1		0x00
#define	CTL_PRE_2		0x10
#define	CTL_PRE_4		0x20
#define	CTL_PRE_8		0x30
#define	CTL_PRE_16		0x40
#define	CTL_PRE_32		0x50
#define	CTL_PRE_64		0x60
#define	CTL_PRE_128		0x70

#define	CTL_SINGLE		0x80
#define	CTL_AUTO		0x00

#define IE_T0			0x01
#define IE_T1			0x02

void timer_handler ( int );

/* The timer is a down counter,
 *  intended to generate periodic interrupts
 * There are two of these.
 * There is also a watchdog and
 * an "AVS" timer (Audio/Video Synchronization)
 *  neither of which are supported here.
 *
 * The datasheet says they are 24 bit counters, but
 *  experiment clearly shows T0 and T1 are 32 bit,
 *  so the 24 bit claim perhaps/probably applies
 *  to the watchdog, who can say?
 *  Foggy documentation at best.
 * 24 bits can hold values up to 16,777,215
 */

/* All indications are that this is a 32 bit counter running
 *  -- at 30,384 Hz when we ask for 32K
 *  -- at 23897172 when we ask for 24M
 *  (note that this is off by .0045 percent, but in
 *   actual fact we are using the same crystal to run
 *   the CPU so this is meaningless.
 */

#define CLOCK_24M	24000000
#define CLOCK_24M_MS	24000

static int timer_rate;

void
timer_start ( int hz )
{
	struct h3_timer *hp = TIMER_BASE;

	timer_rate = hz;

	hp->t0_ival = CLOCK_24M / hz;

	hp->t0_ctrl = 0;	/* stop the timer */
	hp->irq_ena = IE_T0;

	hp->t0_ctrl = CTL_SRC_24M;
	hp->t0_ctrl |= CTL_RELOAD;
	while ( hp->t0_ctrl & CTL_RELOAD )
	    ;

	hp->t0_ctrl |= CTL_ENABLE;
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
timer_rate_set ( int hz )
{
	struct h3_timer *hp = TIMER_BASE;

	timer_rate = hz;

	hp->irq_ena &= ~IE_T0;
	hp->t0_ctrl = 0;	/* stop the timer */

	hp->t0_ival = CLOCK_24M / hz;

	hp->t0_ctrl = CTL_SRC_24M;
	hp->t0_ctrl |= CTL_RELOAD;
	while ( hp->t0_ctrl & CTL_RELOAD )
	    ;

	hp->t0_ctrl |= CTL_ENABLE;

	hp->irq_ena = IE_T0;
}

#ifdef notdef
/* One shot, delay in milliseconds */
void
timer_one ( int delay )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->t0_ival = CLOCK_24M_MS * delay;

	hp->t0_ctrl = 0;	/* stop the timer */
	hp->irq_ena = IE_T0;

	hp->t0_ctrl = CTL_SRC_24M | CTL_SINGLE;
	hp->t0_ctrl |= CTL_RELOAD;
	while ( hp->t0_ctrl & CTL_RELOAD )
	    ;
	hp->t0_ctrl |= CTL_ENABLE;
}
#endif

void
timer_ack ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->irq_status = IE_T0;
}

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

/* The following have been crudely calibrated (on the BBB)
 * by guessing and using a stopwatch.
 */

/* Close to a 1 second delay */
void
delay1 ( void )
{
    volatile long x = 250000000;

    while ( x-- > 0 )
	;
}

/* About a 0.1 second delay */
void
delay10 ( void )
{
    volatile long x = 25000000;

    while ( x-- > 0 )
	;
}

/* Use for sub millisecond delays
 * argument is microseconds.
 */
void
_udelay ( int n )
{
    volatile long x = 250 * n;

    while ( x-- > 0 )
	;
}

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

extern struct thread *cur_thread;

static volatile long timer_count_t;
static volatile long timer_count_s;

#ifdef notdef
/* Needed by imported linux code.
 */
volatile unsigned long jiffies;
#endif

static vfptr timer_hook;

#ifdef NET_TIMER
static vfptr net_timer_hook;
#endif

void thread_tick ( void );

void
timer_hookup ( vfptr new )
{
	timer_hook = new;
}

#ifdef NET_TIMER
void
net_timer_hookup ( vfptr new )
{
	net_timer_hook = new;
}
#endif

/* Count in ticks */
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

/* Handle a timer interrupt */
/* Called at interrupt level */
void
timer_handler ( int junk )
{
	static int subcount;

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

	if ( timer_hook ) {
	    (*timer_hook) ();
	}

#ifdef NET_TIMER
	if ( net_timer_hook ) {
	    (*net_timer_hook) ();
	}
#endif

	timer_ack ();
}

/* Called during Kyu startup */
void
timer_init ( int rate )
{
	timer_count_t = 0;
	timer_count_s = 0;

	timer_hook = (vfptr) 0;

#ifdef NET_TIMER
	net_timer_hook = (vfptr) 0;
#endif

	irq_hookup ( IRQ_TIMER0, timer_handler, 0 );

	timer_start ( rate );
}

/* THE END */
