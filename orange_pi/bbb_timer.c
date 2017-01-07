/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* timer.c
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

#include <omap_ints.h>
// #include <cpu.h>

void prcm_timer1_mux ( void );

extern struct thread *cur_thread;

void thread_tick ( void );

/* This should get overwritten by the value
 * determined by calibration
 */
static int timer_hz = TIMER_CLOCK;

static volatile long timer_count_t;
static volatile long timer_count_s;

/* Needed by imported linux code.
 */
volatile unsigned long jiffies;

static vfptr timer_hook;
#ifdef NET_TIMER
static vfptr net_timer_hook;
#endif

static int timer_rate;

/* --------------------------------------------------- */
/* --------------------------------------------------- */

/* The following have been crudely calibrated
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

/* --------------------------------------------------- */
/* --------------------------------------------------- */

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

void
dmtimer_irqena ( void )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	tmr->ena = TIMER_OVF;
}

void
dmtimer_irqdis ( void )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	tmr->dis = TIMER_OVF;
}

void
dmtimer_rate_set ( int hz )
{
	dmtimer_irqdis ();
	dmtimer_rate_set_i ( hz );
	dmtimer_irqena ();
}

/* Handle a timer interrupt */
void
dmtimer_int ( int xxx )
{
	static int subcount;
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	/* Abandon bogus interrupts */
	if ( ! (tmr->irq_stat & TIMER_OVF) ) {
	    ++bogus_count;
	    return;
	}
	    
	++jiffies;

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

	/* ACK the interrupt */
	tmr->irq_stat = TIMER_OVF;
}

void
dmtimer_init ( int rate )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	/* This arrives set to 0x4 from U-boot (posted bit is set) */
	// printf ( " SIC == %08x\n", tmr->sic );
	tmr->sic = SIC_POSTED;

	timer_hz = dmtimer_checkrate ();

	/* Stop the timer */
	dmtimer_irqdis ();
	post_spin ( POST_CTRL );
	tmr->ctrl = 0;

	/* XXX not for us, but for the 1ms timer */
	/* will need something like this for timer 2 */
	// prcm_timer1_mux ();

	timer_count_t = 0;
	timer_count_s = 0;

	timer_hook = (vfptr) 0;
#ifdef NET_TIMER
	net_timer_hook = (vfptr) 0;
#endif

	// printf ( "Timer id: %08x\n", tmr->id );

	irq_hookup ( TIMER_IRQ, dmtimer_int, 0 );

	timer_rate = rate;
	dmtimer_rate_set_i ( rate );
	dmtimer_irqena ();

	/* start the timer with autoreload */
	post_spin ( POST_CTRL );
	tmr->ctrl = CTRL_START | CTRL_AUTO;
}

/* Called during Kyu startup */
void
timer_init ( int rate )
{
	/* XXX */
	timer_probe ();

	dmtimer_init ( rate );
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
	dmtimer_rate_set ( rate );
}
