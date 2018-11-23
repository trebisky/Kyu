/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * This is a driver for the H3 Allwinner timer module.
 * It includes two timers and the watchdog.
 * It also has the AVS (audio/video sync) timer,
 *  which might be useful for something, but
 *  I'm not sure what.
 *
 * Tom Trebisky  1-6-2017
 */
#include <arch/types.h>

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include "h3_ints.h"

struct h3_timer {
	vu32 irq_ena;		/* 00 */
	vu32 irq_status;	/* 04 */
	int __pad1[2];

	vu32 t0_ctrl;		/* 10 */
	vu32 t0_ival;		/* 14 - interval value */
	vu32 t0_cval;		/* 18 - current value */
	int __pad2;

	vu32 t1_ctrl;		/* 20 */
	vu32 t1_ival;		/* 24 */
	vu32 t1_cval;		/* 28 */
	int __pad3[21];

	vu32 avs_cnt_ctl;	/* 80 */
	vu32 avs_cnt0;		/* 84 */
	vu32 avs_cnt1;		/* 88 */
	vu32 avs_cnt_div;	/* 8c */
	int __pad4[4];

	vu32 wd_irq_ena;	/* a0 */
	vu32 wd_irq_stat;	/* a4 */
	int __pad5[2];
	vu32 wd_ctrl;		/* b0 */
	vu32 wd_cfg;		/* b4 */
	vu32 wd_mode;		/* b8 */
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
 *   the CPU so fretting over the precise value of these
 *   calibrations is meaningless.
 */

#define CLOCK_24M	24000000
#define CLOCK_24M_MS	24000

static void
timer_start ( int hz )
{
	struct h3_timer *hp = TIMER_BASE;

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
 * Set a different timer rate
 */
void
opi_timer_rate_set ( int hz )
{
	struct h3_timer *hp = TIMER_BASE;

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

void _udelay ( int );

#ifdef ARCH_ARM64
/* These are called from the IO test menu */

#define CORE_0  0
#define CORE_1  1
#define CORE_2  2
#define CORE_3  3

static void
test4_handler_1 ( int xxx )
{
        printf ( "BANG!\n" );
}

static void
test4_handler_2 ( int xxx )
{
        printf ( "BOOM!\n" );
}

void
timer_check1 ( void )
{
	reg_t el;
	reg_t sc;
	reg_t zero;
	char *p;
	long *lp;
	int i;

	irq_hookup ( IRQ_SGI_1, test4_handler_1, 0 );
        irq_hookup ( IRQ_SGI_2, test4_handler_2, 0 );

	get_EL(el);
	printf ( "Current EL = %d\n", el>>2 );

#ifdef CHECK_FAULT1
	/* This does NOT yield a fault */
	printf ( "Try to provoke a fault 1\n" );
#ifdef notdef
	/* This just locks things up.
	 * We really need to flush the cache first.
	 */
	// turn off D cache
	get_SCTLR(sc);
	sc &= ~4;
	set_SCTLR(sc);
	printf ( "D cache is now off\n" );
#endif

	p = (char *) 0x40000000;
	for ( i=0; i<8; i++ ) {
	    lp = (long *) p;
	    printf ( "Fetch long: %016x = %016x\n", lp, *lp );
	    p += 1;
	}

#ifdef notdef
	// turn D cache back on
	get_SCTLR(sc);
	sc |= 4;
	set_SCTLR(sc);
#endif
#endif /* CHECK_FAULT1 */

#ifdef CHECK_FAULT2
	/* This definitely yields a fault */
	printf ( "Try to provoke a fault 2\n" );
	/* Writing to a register at a higher EL should yield a fault */
	zero = 0;
	asm volatile ( "msr SCTLR_EL3, %0" : : "r" ( zero ) );
#endif /* CHECK_FAULT2 */

	printf ( "Try to interrupt ourself\n" );
        gic_soft_self ( SGI_1 );
        gic_soft ( SGI_2, CORE_0 );
	printf ( "Done\n" );
}

void
timer_check2 ( void )
{
	struct h3_timer *hp = TIMER_BASE;
	int i;

	for ( i=0; i < 10; i++ ) {
	    printf ( "Timer - current value: %08x\n", hp->t0_cval );
	    _udelay ( 10 );
	}
	printf ( "Timer - IRQ status: %08x\n", hp->irq_status );
	hp->irq_status = IE_T0;
	printf ( "Timer - IRQ status: %08x\n", hp->irq_status );

	// does nothing
	gic_bypass ();
}
#endif	/* ARCH_ARM64 */

/* H3 watchdog.
 * Note that there is another watchdog, not part of the timer module,
 * called the "trusted watchdog" that we have had no urge to play with.
 */

#define WD_RESET	1	/* Reset whole system */
#define WD_IRQ		2	/* Generate interrupt */

#define WD_HALF		0x00	/* 0.5 second */
#define WD_1		0x10	/* 1 second */
#define WD_2		0x20	/* 2 second */
#define WD_3		0x30	/* 3 second */
#define WD_4		0x40	/* 4 second */
#define WD_5		0x50	/* 5 second */
#define WD_6		0x60	/* 6 second */
#define WD_8		0x70	/* 8 second */
#define WD_10		0x80	/* 10 second */
#define WD_12		0x90	/* 12 second */
#define WD_14		0xA0	/* 14 second */
#define WD_16		0xB0	/* 16 second */

#define WD_RUN		1

#define WD_KEY		(0xA57 << 1)
#define WD_RESTART	1

/* initialize the Watchdog.
 * We set up the shortest possible interval since we
 * simply want to use the watchdog to generate a system reset.
 * This was suggested by  Arjan van Vught  and works great.
 * Implemented 6-7-2018
 */
void
wd_init ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	// printf ( "Watchdog: %08x\n", &hp->wd_mode );

	hp->wd_irq_ena = 0;
	hp->wd_cfg = WD_RESET;
	hp->wd_mode = WD_HALF;
}

void
wd_stop ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->wd_mode &= ~WD_RUN;
}

void
wd_start ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->wd_mode |= WD_RUN;
}

void
system_reset ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->wd_mode |= WD_RUN;
}

/* If we were using this as a true watchdog, this would be what we
 * would need to call regularly to keep it off our back.
 */
void
wd_restart ( void )
{
	struct h3_timer *hp = TIMER_BASE;

	hp->wd_ctrl = WD_KEY | WD_RESTART;
}

/* Called from test shell
 * Test will last 2 seconds
 * Test fails if we get reset.
 */
void
wd_test ( void )
{
	int i;

	wd_start ();
	for ( i=0; i< 500; i++ ) {
	    thr_delay ( 10 );
	    wd_restart ();
	}
	wd_stop ();
	printf ( "Done\n" );
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

/* Handle a timer interrupt */
/* Called at interrupt level */
static void
timer_handler ( int junk )
{
	timer_tick ();

	timer_ack ();
}

/* Called during Kyu startup */
void
opi_timer_init ( int rate )
{
	irq_hookup ( IRQ_TIMER0, timer_handler, 0 );

	timer_start ( rate );
}

/* THE END */
