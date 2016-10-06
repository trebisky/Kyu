/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* wdt.c
 *
 * Tom Trebisky  Kyu project  5-1-2015
 *
 * Simple driver for the watchdog timer
 *  they call this WDT1, but there is only
 *  one, there is no WDT0
 * 
 * There are two ways to use this.
 * 1) Disable it altogether by either:
 *	wdt_disable();
 *	wdt_init ( 0 );
 *
 * 2) Enable it and keep resetting it
 *     before it gets a chance to overflow.
 *	wdt_init ( 10 );
 *	for ( ;; ) wdt_reset ();
 *
 * Note that it is essential to at least disable it,
 *  as U-boot typically leaves a 60 second timer
 *  running when it transfers control to us.
 */
#include <kyulib.h>

#define WDT_BASE      0x44E35000

struct wdt {
	volatile unsigned long id;
	long _pad0[3];
	volatile unsigned long syscon;
	volatile unsigned long status;
	volatile unsigned long isr;
	volatile unsigned long ie;
	long _pad1;
	volatile unsigned long control;
	volatile unsigned long count;
	volatile unsigned long load;
	volatile unsigned long trigger;
	volatile unsigned long wwps;
	long _pad2[3];
	volatile unsigned long delay;
	volatile unsigned long wspr;
	long _pad3[2];
	volatile unsigned long irqstatr;
	volatile unsigned long irqstat;
	volatile unsigned long irqen_set;
	volatile unsigned long irqen_clear;
};

/* bits that we can poll to wait for a
 * write pending register.
 */
#define W_PEND_WDLY	0x20
#define W_PEND_WSPR	0x10
#define W_PEND_WTGR	0x08
#define W_PEND_WLDR	0x04
#define W_PEND_WCRR	0x02
#define W_PEND_WCLR	0x01

/* Bit in the control register.
 * The only thing the control register does is to
 * set up the prescaler.
 */
#define PRE_ENABLE	0x20

#define PRE_1	0
#define PRE_2	1
#define PRE_4	2
#define PRE_8	3
#define PRE_16	4
#define PRE_32	5
#define PRE_64	6
#define PRE_128 7

/* with the prescaler set to 1, we count up at the full
 * clock rate.  It is not clear to me that there is any
 * difference between enabling the prescaler and setting
 * it to "1" like this, or just not enabling it at all.
 */
#define WDT_CLOCK	32768

#define DEFAULT_PRE	PRE_1

/* Turn it off altogether */
void
wdt_disable ( void )
{
	struct wdt *wp = (struct wdt *) WDT_BASE;

	wp->wspr = 0xAAAA;
	while ( wp->wwps & W_PEND_WSPR )
	    ;
	wp->wspr = 0x5555;
	while ( wp->wwps & W_PEND_WSPR )
	    ;
}

void
wdt_enable ( void )
{
	struct wdt *wp = (struct wdt *) WDT_BASE;

	wp->wspr = 0xBBBB;
	while ( wp->wwps & W_PEND_WSPR )
	    ;
	wp->wspr = 0x4444;
	while ( wp->wwps & W_PEND_WSPR )
	    ;
}

static unsigned int last_trigger = 0xbeef;

/* If it is running, whack it so it restarts.
 * The manual says any value different from
 * what is currently in the register does the job.
 *
 * It seems unlikely that any of this polling is
 *  really needed.  It is all about waiting for
 *  posted writes to be done, and we are not
 *  likely to be calling this routine often
 *  enough for this to be an issue, especially
 *  on entry.
 */
void
wdt_reset(void)
{
        struct wdt *wp = (struct wdt *)WDT_BASE;

        while ( wp->wwps & W_PEND_WTGR )
                ;

	wp->trigger = last_trigger = ~last_trigger;

        while ( wp->wwps & W_PEND_WTGR )
                ;
}

/* Argument is timeout in seconds */
static void
wdt_set_timeout(unsigned int timeout)
{
        struct wdt *wp = (struct wdt *)WDT_BASE;

	unsigned long load_val = 
	  (0xffffffff - ((timeout) * (WDT_CLOCK/(1<<DEFAULT_PRE))) + 1);

        printf ( "WDT loading: %08x\n", load_val );

        while ( wp->wwps & W_PEND_WLDR )
                ;

	wp->load = load_val;

        while ( wp->wwps & W_PEND_WLDR )
                ;
}

static void
wdt_set_prescale ( void )
{
        struct wdt *wp = (struct wdt *)WDT_BASE;

	printf (" prescale (start): %08x\n", wp->control );

        while ( wp->wwps & W_PEND_WCLR )
                ;

	wp->control = PRE_ENABLE | (DEFAULT_PRE<<2);

        while ( wp->wwps & W_PEND_WCLR )
                ;
	printf (" prescale (done): %08x\n", wp->control );
}

/* Don't initialize it if you don't want to use it.
 * Well, OK, if you pass a zero argument, we will
 *  ensure that it is disabled  altogether.
 * You could call the disable function for that,
 *  but we provide the zero argument as a convenience.
 *
 * Argument is timeout in seconds.
 */
void
wdt_init ( unsigned int timeout )
{
	struct wdt *wp = (struct wdt *) WDT_BASE;

	wdt_disable ();

	if ( timeout <= 0 )
	    return;

	wdt_set_prescale ();
        wdt_set_timeout ( timeout );

	/* wdt_reset (); Does nothing here */

	wdt_enable ();

	/* This is essential, and you must do it after
	 * the watchdog is enabled. This causes the load
	 * value to get copied to the count register.
	 * And it causes the preload to get set.
	 */
	wdt_reset ();
}

/* THE END */
