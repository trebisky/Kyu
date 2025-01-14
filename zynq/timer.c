/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 * Copyright (C) 2024  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * timer.c for the Zynq
 *
 * Tom Trebisky  1-13-2021
 * Tom Trebisky  11/14/2024
 *
 */

/* This was worked up for my Ebaz bare metal demos,
 * then moved into Kyu as a timer driver.
 * As noted below, there are other timer resources
 * available in each ARM core of the Zynq.
 * But we have this driver all written and debugged.
 * If someday we need all 6 TTC timers for something
 * else we can migrate the Kyu timer to one of those.
 *
 * Zynq-7000 TTC driver
 * The TTC is the "triple timer counter"
 * There are 2 of these, so we have 6 timers
 *
 */

#define TTC0_BASE	0xf8001000
#define TTC1_BASE	0xf8002000

/* We did a bunch of digging 1-12-2025 and learned that the timer is among
 * a special gang of peripherals that get fed the cpu_1x clock
 * In our system, where the CPU runs at 666 Mhz, this clock is 111 Mhz.
 */
#define PCLK_RATE 111000000L

#include "zynq_ints.h"

#ifdef not_any_more
#define IRQ_TTC0A	42
#define IRQ_TTC0B	43
#define IRQ_TTC0C	44

#define IRQ_TTC1A	69
#define IRQ_TTC1B	70
#define IRQ_TTC1C	71
#endif

/* The Zynq-7000 has a pair of ARM Cortex-A9 processors.
 * each has a private 32 bit timer and 32 bit watchdog
 * there is also a global 64 bit timer.
 * Then there is a 24 bit watchdog peripheral.
 * Maybe we would do better to use one of these to
 * generate our 1000 Hz tick interrupt?
 *
 * the global timer is IRQ 27 (and is a PPI)
 * the CPU private timer is IRQ 29 (also a PPI)
 * the CPU private watchdog is IRQ 30 (also a PPI)
 *
 * And then we have two of these TTC as peripherals
 * These are clocked at 1/4 or 1/6 of the CPU frequency.
 *
 * The TTC is described in section 8.5 of the TRM.
 * The register description is tucked away in Appendix B.32
 * These are 16 bit counters
 */

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#define BIT(x)  (1<<(x))

struct ttc {
	vu32	ccr1;		/* clock control */
	vu32	ccr2;
	vu32	ccr3;

	vu32	mode1;		/* mode and reset */
	vu32	mode2;
	vu32	mode3;

	vu32	val1;		/* value RO */
	vu32	val2;
	vu32	val3;

	vu32	interval1;
	vu32	interval2;
	vu32	interval3;

	vu32	match1_ctr1;
	vu32	match1_ctr2;
	vu32	match1_ctr3;

	vu32	match2_ctr1;
	vu32	match2_ctr2;
	vu32	match2_ctr3;

	vu32	match3_ctr1;
	vu32	match3_ctr2;
	vu32	match3_ctr3;

	vu32	is1;
	vu32	is2;
	vu32	is3;

	vu32	ie1;
	vu32	ie2;
	vu32	ie3;

	vu32	ec1;
	vu32	ec2;
	vu32	ec3;

	vu32	event1;
	vu32	event2;
	vu32	event3;
};

#define TIMER_BASE	((struct ttc *) TTC0_BASE)

/* Bits in the ccr register */
#define CCR_PRE_ENA	BIT(0)
#define CCR_PRE_SHIFT	1	/* 4 bit field */
#define CCR_EXT_SRC	BIT(5)
#define CCR_EXT_EDGE	BIT(6)

// Prescaler is 2^(N+1)
#define PRESCALE_16		3

/* Bits in the mode register */
#define MODE_DISABLE	BIT(0)
#define MODE_INTER	BIT(1)
#define MODE_RESET	BIT(4)

/* Bits in the interrupt register */
#define IE_INTERVAL	BIT(0)
#define IE_MATCH1	BIT(1)
#define IE_MATCH2	BIT(2)
#define IE_MATCH3	BIT(3)
#define IE_OVERFLOW	BIT(4)
#define IE_EVENT	BIT(5)

/* ===========================================================*/

void timer_handler ( int );

volatile int timer_count;

/* With a prescaler of 16, a preload of 10000 gave us 667 ticks per second */
/* So a preload of 6666 gives us a nice 1000 ticks per second */
// #define PRELOAD	6666

/* Should be correct with a 111 Mhz Pclk */
#define PRELOAD	6937

/* XXX - currently rate is ignored */
void
zynq_timer_init ( int rate )
{
	struct ttc *tp = TIMER_BASE;

	timer_count = 0;

	/* select pclk as source,
	 * prescaler of 16 (2^(3+1))
	 */
	tp->ccr1 = CCR_PRE_ENA | (PRESCALE_16<<CCR_PRE_SHIFT);

	tp->mode1 = MODE_INTER;
	tp->ie1 = IE_INTERVAL;

	tp->interval1 = PRELOAD;

	irq_hookup ( IRQ_TTC0A, timer_handler, 0 );

	tp->mode1 &= ~MODE_DISABLE;
	tp->mode1 |= MODE_RESET;
}

/* XXX - not yet */
void
zynq_timer_rate_set ( int rate )
{
}

void
zynq_timer_show ( void )
{
	printf ( "Timer count: %d\n", timer_count );
}

/* Called at interrupt level */
void
timer_handler ( int xxx )
{
	struct ttc *tp = TIMER_BASE;
	int s;

	/* This is cleared on read */
	s = tp->is1;

	// I see: Timer interrupt 1 0
	// printf ( "Timer interrupt %x %d\n", s, xxx );

	timer_count++;

	// Announce this to Kyu
	timer_tick (); 

    // timer_ack ();

}

/* THE END */
