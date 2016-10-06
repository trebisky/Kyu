/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* das16.c
 * $Id: das16.c,v 1.10 2002/12/17 04:13:44 tom Exp $
 *
 * Skidoo driver for a Computer Boards Inc. CIO-AD16
 * also applies to a DAS-16 and various Metrabyte products.
 * Apparently the Metrabyte DAS-16 was the original design.
 * The Computer Boards documents I have, remark on numerous
 * superiorities of the CIO-AD16 design, so it clearly came
 * later.  Both are 8-bit ISA boards.
 *
 * Note that Keithley now owns Metrabyte and has nice manuals
 * on the web (2309.pdf) for this, at least as of 9/20/2002
 * 
 * Tom Trebisky
 * 7/13/1999 first cut.
 * 3/26/2002 made to compile under skidoo.
 * 9/08/2002 develop a real skidoo driver.
 * 9/19/2002 get interrupts on EOC to work.
 * 9/20/2002 get timer to trigger sequence.
 */

#include "skidoo.h"
#include "intel.h"
#include "thread.h"
#include "sklib.h"

/*
 * Apparently the CIO-AD16 can have two sections:
 *	A DAS-16 compatible section at the base address.
 * It may also have a second PIO section:
 *	A PIO-12 compatible section at base + 0x10.
 *		(the PIO-12 consists of an 8255 and its 4 ports)
 *	The PIO-12 can be disabled entirely, in which case
 *	the CIO-AD16 uses only 16 io ports.
 *	(one of my boards has no PIO-12 section).
 */

/* IO port offsets from base address
 */
#define CONVERT	0x000	/* Write: any data starts a conversion */
#define DATA_LO	0x000	/* Read: 4 low bits of ADC + address */
#define DATA_HI	0x001	/* RO: 8 high bits of ADC */
#define SCAN	0x002	/* R/W: High/Low scan limits */
#define DIO	0x003	/* Digital I/O */
#define DAC0_LO	0x004	/* Dac 0 */
#define DAC0_HI	0x005
#define DAC1_LO	0x006	/* Dac 1 */
#define DAC1_HI	0x007
#define STATUS	0x008	/* Read mostly */
#define CTL	0x009	/* DMA, IRQ, trigger control */
#define CLOCK	0x00A	/* external control of 8254 */
#define	GAIN	0x00B	/* programmable gain on newer boards */
#define TMR0	0x00C	/* 82C54 data 0 */
#define TMR1	0x00D	/* 82C54 data 1 */
#define TMR2	0x00E	/* 82C54 data 2 */
#define TMRCTL	0x00F	/* 82C54 control */

/* status register */
#define ST_EOC	0x80
#define ST_UNI	0x40
#define ST_16	0x20
#define ST_INT	0x10
#define ST_CHAN	0x0f

/* Notes on interrupts:
 * 2 is the cascade on modern x86 boards and should be avoided.
 * 3 and 4 are typically used by serial ports.
 * 6 is floppy, and 7 is parallel port.
 * 5 is really the only interrupt fully available.
 * 
 * Also note: it is the EOC on a conversions that causes
 * the interrupt, not the 8254 timer.  The timer can
 * be set up to trigger the conversions, but not to
 * interrupt.  And furthermore, it is necessary to clear
 * the interrupt flag before you ever get an interrupt,
 * this may be because the PIC is set up edge triggered.
 */

/* control register
 */
#define CTL_IE	0x80	/* interrupt enable */

#define CTL_IRQ_2	0x20	/* interrupt on IRQ 2 */
#define CTL_IRQ_3	0x30	/* interrupt on IRQ 3 */
#define CTL_IRQ_4	0x40	/* interrupt on IRQ 4 */
#define CTL_IRQ_5	0x50	/* interrupt on IRQ 5 */
#define CTL_IRQ_6	0x60	/* interrupt on IRQ 6 */
#define CTL_IRQ_7	0x70	/* interrupt on IRQ 7 */
	/* bit 0x08 not used */

#define CTL_DE	0x04	/* DMA enable */
#define CTL_ST	0x01	/* Software conversion Trigger */
#define CTL_ET	0x02	/* External pin 25 Trigger */
#define CTL_TT	0x03	/* Timer 2 out Trigger */

/* Bits in the clock control register
 */
#define CLK_1K		0x02	/* send 100 kHz to timer 0 */
#define CLK_GATE	0x01	/* enable timer 1 and 2 gate */

/* My das16 has a Harris HI-674AJD converter chip.
 * This is a "famous" industry standard part,
 * identical with the Burr Brown ADC674A.
 * It has a 15 microsecond conversion time (to 12 bits).
 *
 * It can do 8 bits conversions in 8 microseconds,
 * but it is not clear that this ability can be
 * accessed on the DAS16 board.
 *
 * The board documents state that the 674 can give
 * 50,000 conversions per second (20 microsecond
 * overall conversion time), but with a 774 chip,
 * the board can give 100,000 conversions per second.
 * No mention is made of short-cycling 8 bit conversions.
 */

/* ----------------------------------------------- */
/* ----------------------------------------------- */

/* My board has switches and jumpers set such that:
 * base is 0x300
 * DMA switch is 3
 * ADC is bipolar, 16 channels single ended.
 * Timer is 1 Mhz (not 10 Mhz as it could be).
 */

#define AD16_BASE 0x300

static int base = AD16_BASE;

static struct sem *das_sem;
static struct sem *das_mutex;

enum das16_state { INIT, RUN, HOLD };
static enum das16_state state = INIT;

static int burst_count;
static int burst_want;
static short * next;

#define MAX_BUF	2000
static short buffer[MAX_BUF];


static unsigned long long tsc_start;
static unsigned long long tsc_finish;

void das_all ( void );
void das_scan ( int, int );
void das_rate ( int, int );

#define MAXW	100000

static void
das_int ( int arg )
{
	int scan, data;
	unsigned long ticks;

	/* clear interrupt
	 */
	outb ( 0xff, base + STATUS );

	if ( state != RUN )
	    return;

#ifdef DAS_INT_DEBUG
	if ( state != RUN ) {
	    printf ( "das-16 spurious interrupt: %02x\n",
		inb ( base + STATUS ) );
	    return;
	}
	printf ( "das-16 interrupt: %02x ",
		inb ( base + STATUS ) );
	printf ( "Channel %2d: ", scan & 0x0f );
	printf ( " %04x\n", data );
#endif

	scan = inb ( base + DATA_LO );
	data = inb ( base + DATA_HI ) << 4;
	data |= (scan & 0xf0) >> 4;

	*next++ = data;
	++burst_count;

	if ( burst_count >= burst_want ) {
	    rdtscll ( tsc_finish );
	    ticks = tsc_finish - tsc_start;
	    /*
	    printf ( "Done, %d values (%d ticks)\n", burst_count, ticks );
	    */
	    state = HOLD;
	    cpu_signal ( das_sem );
	}
}

void
das_init ( void )
{
	das_sem = cpu_new ();
        das_mutex = sem_mutex_new ( SEM_PRIO );

	/* Turn off interrupts,
	 * specify software trigger
	 * (apparently if interrupts are on,
	 * this write generates an interrupt.)
	 */
	state = INIT;
	outb ( CTL_IRQ_5, base + CTL );

	irq_hookup ( 5, das_int, 0 );

	/* run with software trigger */
	outb ( CTL_IE | CTL_IRQ_5 | CTL_ST, base + CTL );

	/* any write should clear the
	 * interrupt request.
	 */
	outb ( ST_INT, base + STATUS );
	state = RUN;
}

void
das_start ( void )
{
	/* Start a conversion
	 */
	outb ( 0xff, base + CONVERT );
}

void
das_status ( void )
{
	printf ( "das#16 status: %02x\n", inb ( base + STATUS ) );
}

/* Called from the test thread.
 */
void
test_das ( int xx )
{
	das_init ();

	das_all ();
	das_scan ( 0, 0 );
	das_scan ( 3, 4 );
}

void
das_test ( int verbose )
{
	int data, stat;
	int scan;
	long timeout;

	/* Start a conversion
	 */
	outb ( 0xff, base + CONVERT );

	for ( timeout = MAXW; timeout; timeout-- ) {
	    if ( ! (inb ( base + STATUS ) & ST_EOC) )
	    	break;
	}

	stat = inb ( base + STATUS );

	if ( ! timeout ) {
	    printf("Timeout! Status = %02x\n", stat );
	    return;
	}

	scan = inb ( base + DATA_LO );
	data = inb ( base + DATA_HI ) << 4;
	data |= (scan & 0xf0) >> 4;

	printf ( "Channel %2d: ", scan & 0x0f );
	if ( verbose )
	    printf("(%d,%02x) ",MAXW-timeout,stat);
	printf ( " %04x\n", data );
}

int
das_adc( int chan )
{
	int data, scan;
	long timeout;

	/* Set the scan range */
	scan = (chan & 0xf) << 4;
	scan |= chan & 0xf;
	outb ( scan, base + SCAN );

	/* Start a conversion
	 */
	outb ( 0xff, base + CONVERT );

	for ( timeout = MAXW; timeout; timeout-- )
	    if ( ! (inb ( base + STATUS ) & ST_EOC) )
	    	break;

	if ( ! timeout )
	    return -1;

	data = inb ( base + DATA_HI ) << 4;
	data |= ( inb ( base + DATA_LO ) & 0xf0) >> 4;
	return data;
}


void
das_scan( int first, int last )
{
	int scan;

	scan = (last & 0xf) << 4;
	scan |= first & 0xf;
	outb ( scan, base + SCAN );

	/*
	int i;
	for ( i=0; i<16; i++ )
	    das_test(1);
	*/
}

void
das_all( void )
{
	das_scan ( 0, 15 );
}

/* The board has 2 16-bit DAC's.
 * They normally get their reference voltage
 * from the ADC chip, and have a range of 0-5 volts.
 * output code = (volts/5.0) * 4095
 *
 * Should write LSB first,
 * the DAC actually updates when the MSB is written.
 */

void
das_dac0 ( int val )
{
	outb ( (val & 0xf) << 4, base + DAC0_LO );
	outb ( val >> 4, base + DAC0_HI );
}

void
das_dac1 ( int val )
{
	outb ( (val & 0xf) << 4, base + DAC1_LO );
	outb ( val >> 4, base + DAC1_HI );
}

#define TMR_BCD		0x01	/* else binary */
#define TMR_MODE_RATE	0x04	/* rate generator */
#define TMR_MODE_SW	0x06	/* square wave */
#define TMR_LOAD	0x30	/* LSB then MSB (normal) */
#define TMR_LOAD_LSB	0x10	/* LSB only */
#define TMR_LOAD_MSB	0x20	/* MSB only */
#define TMR_LOAD_LATCH	0x00	/* latch count value */
#define TMR_SEL_0	0x00	/* select counter 0 */
#define TMR_SEL_1	0x40	/* select counter 1 */
#define TMR_SEL_2	0x80	/* select counter 2 */
#define TMR_SEL_RB	0xC0	/* readback command */

short *
das_burst ( int chan, int num )
{
	state = INIT;
	outb ( CTL_IRQ_5, base + CTL );

	/* set channel to read */
	das_scan ( chan, chan );

	/* 10 Mhz / (100*100) is 1000 Hz sample rate.
	 */
	das_rate ( 100, 100 );

	irq_hookup ( 5, das_int, 0 );

	/* run with timer trigger */
	outb ( CTL_IE | CTL_IRQ_5 | CTL_TT, base + CTL );

	/* clear the interrupt flag.
	 */
	outb ( 0x00, base + STATUS );

	/* gate on the clock.
	 */
	outb ( CLK_GATE, base + CLOCK );

	burst_count = 0;
	burst_want = num;
	next = buffer;

	rdtscll ( tsc_start );

	cpu_enter ();
	state = RUN;
	cpu_wait ( das_sem );

	return buffer;
}

/* set up the pair of counter/timers that trigger
 * ADC conversions
 */
void
das_rate ( int count1, int count2 )
{
	outb ( TMR_MODE_RATE | TMR_LOAD | TMR_SEL_1, base + TMRCTL );
	outb ( count1 & 0xff, base + TMR1 );
	outb ( count1 >> 8, base + TMR1 );

	outb ( TMR_MODE_RATE | TMR_LOAD | TMR_SEL_2, base + TMRCTL );
	outb ( count2 & 0xff, base + TMR2 );
	outb ( count2 >> 8, base + TMR2 );
}

/* THE END */
