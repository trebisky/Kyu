/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * adc.c -- 10-7-2016
 *
 * driver for the analog input section of the BBB
 *
 * This is called the "touchscreen controller" in the TRM
 *   and is described in section 12.
 * It is also referred to as the ADC_TSC
 * We neither use nor support any of the touch screen features here.
 *
 * It is an 8 channel, 12 bit adc capable of 125 ns conversions
 * The first 7 of the 8 are available on the BBB P9 connector as AIN 0-6
 * The "other" channel (AIN7) is fed by a resistor divider from 3.3 volts
 *   so it should yield 1.65 volts at all times.
 * These inputs have a 1.8 volt range, which should NOT be exceeded.
 *
 * This thing gets a 24 Mhz clock.
 *  this thing can sample every 15 clocks.
 *
 * The linux driver is worth studying:
 *    linux-3.8.13/kernel/include/linux/mfd/ti_am335x_tscadc.h
 *    linux-3.8.13/kernel/drivers/iio/adc/ti_am335x_adc.c
 *
 * This device can do DMA, as well as interrupt the PRU.
 *   AFE = "analog front end"
 *   FSM = "finite state machine??"
 */

#define ADC_BASE      ( (struct adc *) 0x44E0D000 )

#include <kyu.h>
#include <kyulib.h>
#include <omap_ints.h>
#include <thread.h>

#define NUM_STEPS	16
#define NUM_CHAN	8

struct step {
	volatile unsigned long config;
	volatile unsigned long delay;
};

/* The adc has registers like this:
 */
struct adc {
	volatile unsigned long rev;		/* 0x00 */
	long _pad0[3];
	volatile unsigned long sysconfig;	/* 0x10 */
	long _pad1[4];
	volatile unsigned long irqstat_raw;	/* 0x24 */
	volatile unsigned long irqstat;		/* 0x28 */
	volatile unsigned long irqena_set;	/* 0x2c */
	volatile unsigned long irqena_clr;	/* 0x30 */
	volatile unsigned long irq_wakeup;	/* 0x34 */

	volatile unsigned long dmaena_set;	/* 0x38 */
	volatile unsigned long dmaena_clr;	/* 0x3c */

	volatile unsigned long control;		/* 0x40 */
	volatile unsigned long status;		/* 0x44 */
	volatile unsigned long adc_range;	/* 0x48 */
	volatile unsigned long adc_clkdiv;	/* 0x4c */
	volatile unsigned long adc_misc;	/* 0x50 */

	volatile unsigned long step_enable;	/* 0x54 */
	volatile unsigned long idle_config;	/* 0x58 */

	struct step charge;			/* 0x5c */
	struct step steps[NUM_STEPS];

	volatile unsigned long fifo0_count;	/* 0xE4 */
	volatile unsigned long fifo0_thresh;
	volatile unsigned long dma0_req;

	volatile unsigned long fifo1_count;	/* 0xF0 */
	volatile unsigned long fifo1_thresh;
	volatile unsigned long dma1_req;

	long _pad2;
	volatile unsigned long fifo0_data;	/* 0x100 */

	long _pad3[63];
	volatile unsigned long fifo1_data;	/* 0x200 */
};

/* bits in step registers */
/* when the mode is "ONE", the step_enable bits get cleared
 * after each step and the sequencer goes idle after they
 * all end up cleared.  When the mode is CONT, the bits stay
 * set and the sequencer will run forever, or even longer.
 */

#define	STEP_MODE_SW_ONE		0x00
#define	STEP_MODE_SW_CONT		0x01
#define	STEP_MODE_HW_ONE		0x02
#define	STEP_MODE_HW_CONT		0x03

#define	STEP_AVG_1			0x00
#define	STEP_AVG_2			0x04
#define	STEP_AVG_4			0x08
#define	STEP_AVG_8			0x0C
#define	STEP_AVG_16			0x10

/* If we don't set this, results go to Fifo 0
 *  (which suits me fine).
 * Note that since this is set (or not) for each step,
 * some data could be directed to one fifo, and some to the other.
 */
#define STEP_FIFO1			0x04000000

/* documentation labels the channels 1-8
 * and calls VREF "channel 9" but we ignore all that
 * and count from 0 like any good C programmer.
 */
#define	STEP_CHAN(x)			((x)<<19)

/* "real" channels are 0-6
 * and on the BBB channel 7 comes from a fixed 1.65 volt divider.
 * channel 8 is the ADC system Vref, which has a bunch of bits in
 * the step config register to allow it to be dorked with.
 */
#define CHAN_VCC		7
#define CHAN_VREF		8

/* bits in the control register
 * We use ENA, and might use TAG someday.
 * We learned the hard way that you better set UNLOCK.
 */
#define CTRL_ENA		0x0001
#define CTRL_TAG		0x0002
#define CTRL_UNLOCK		0x0004
#define CTRL_BIAS		0x0008
#define CTRL_PWRDOWN		0x0010
/* bits here (that I skip) for tsc control */
#define CTRL_TSCENA		0x0080
#define CTRL_HWEVENT		0x0100
#define CTRL_PREEMPT		0x0200

/* Bits in the status register (FSM status)
 */
#define STAT_PEN1_IRQ		0x80
#define STAT_PEN0_IRQ		0x40
#define STAT_BUSY		0x20

#define STAT_MASK		0x1f
#define STAT_IDLE		0x10
#define STAT_CHARGE		0x11

/* Bits in the interrupt registers
 */
#define INT_PEN_AS		0x01
#define INT_EOS			0x02
#define INT_THR0		0x04
#define INT_OR0			0x08
#define INT_UF0			0x10
#define INT_THR1		0x20
#define INT_OR1			0x40
#define INT_UF1			0x80
#define INT_OOR			0x100
#define INT_PENUP		0x200
#define INT_PEN_SY		0x400

/* ------------------------------ */

extern struct thread *cur_thread;

static unsigned int step_enables;
static struct thread *waiting_thread;

#define MODE_SINGLE	1
#define MODE_RUN	2

static int adc_mode;

static void wait_idle ( void );

/* ------------------------------ */
/* ------------------------------ */

/* Possible hardware trigger sources */
#define HWT_PRU		0
#define HWT_TIMER4	1
#define HWT_TIMER5	2
#define HWT_TIMER6	3
#define HWT_TIMER7	4

static void
select_hw_trigger ( void )
{
	cm_adc_mux ( HWT_TIMER4 );
}

/* ------------------------------ */
/* ------------------------------ */

static int data_count;

void
adc_isr ( int xxx )
{
	struct adc *ap = ADC_BASE;
	int junk;

	// printf ( "ADC interrupt: %08x\n", ap->irqstat_raw );

	if ( adc_mode == MODE_RUN ) {
	    while ( ap->fifo0_count > 0 ) {
		junk = ap->fifo0_data;
		++data_count;
	    }
	    /*
	    if ( data_count > 1000 ) {
		printf ( "ADC run finished\n" );
		thr_unblock ( waiting_thread );
	    }
	    */
	    ap->irqstat = INT_THR0;
	    return;
	}

	/* MODE_SINGLE */
	/* Ack the interrupt */
	ap->irqstat = INT_EOS;

	if ( waiting_thread )
	    thr_unblock ( waiting_thread );
}

static void
flush_fifo ( void )
{
	struct adc *ap = ADC_BASE;
	int junk;

	while ( ap->fifo0_count > 0 )
	    junk = ap->fifo0_data;
}

static void
adc_start ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control |= CTRL_ENA;
}

/* XXX - do we need this for anything ?? */
static void
adc_pulse ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control &= ~CTRL_ENA;
	ap->control |= CTRL_ENA;
}

static void
adc_stop ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control &= ~CTRL_ENA;
}

/* Heaven knows what this really should be or how it should change
 * if we don't average 16 readings.
 * XXX - I need to experiment with different delays along
 * with seeing how fast this thing free-runs as currently set up.
 * I currently set the 16 averaging.  How fast is it with other settings?
 */
#define MAGIC_DELAY	152

static void
setup_single ( int chan )
{
	struct adc *ap = ADC_BASE;

	ap->steps[0].config = STEP_AVG_16 | STEP_MODE_SW_ONE | STEP_CHAN(chan);
	ap->steps[0].delay = MAGIC_DELAY;

	step_enables = 1<<1;
	// ap->step_enable = step_enables;
}

static void
setup_run ( int chan )
{
	struct adc *ap = ADC_BASE;

	ap->steps[0].config = STEP_AVG_16 | STEP_MODE_SW_CONT | STEP_CHAN(chan);
	// ap->steps[0].config = STEP_AVG_1 | STEP_MODE_SW_CONT | STEP_CHAN(chan);
	// ap->steps[0].delay = MAGIC_DELAY;
	ap->steps[0].delay = 0;

	step_enables = 1<<1;
}

static void
setup_scan ( int num )
{
	struct adc *ap = ADC_BASE;
	int config;
	int step, chan;

	step_enables = 0;

	// At 0x44E0D064, which is correct.
	// printf ( "First config register at: %08x\n", &ap->steps[0].config );

	/* it is entirely coincidence if chan and step index match here */
	for ( chan=0; chan < num; chan++ ) {
	    step = chan;
	    config = STEP_AVG_16 | STEP_MODE_SW_ONE | STEP_CHAN(chan);
	    // printf ( "Step %d = %08x\n", step, config );
	    ap->steps[step].config = config;
	    ap->steps[step].delay = MAGIC_DELAY;
	    // printf ( "Step %d > %08x\n", step, ap->steps[step].config );
	    step_enables |= 1<<(step+1);
	}

	// printf ( "Step enables = %08x\n", step_enables );
}

static void
adc_trigger ( void )
{
	struct adc *ap = ADC_BASE;

	ap->step_enable = step_enables;
}

static void
show_data ( void )
{
	struct adc *ap = ADC_BASE;
	unsigned int data;
	unsigned int val;

	while ( ap->fifo0_count > 0 ) {
	    data = ap->fifo0_data;
	    val = 180 * data;
	    val /= 4095;
	    printf ( " ADC data: %08x %d  %d\n", data, data, val );
	}
}

static void
show_fifo ( void )
{
	struct adc *ap = ADC_BASE;

	printf ( "ADC fifo, se, control, status:  %d  %08x %08x %08x\n", ap->fifo0_count,
	    ap->step_enable, ap->control, ap->status ); 
}

static void
watch_adc ( int num )
{
	struct adc *ap = ADC_BASE;
	int i;

	for ( i=0; i<num; i++ ) {
	    // thr_delay ( 10 );
	    adc_trigger ();
	    wait_idle ();
	    show_fifo ();
	    show_data ();
	}
}

/* XXX - get rid of this.
 */
static int
adc_read_fifo ( unsigned int *buf )
{
	struct adc *ap = ADC_BASE;
	int rv = 0;

	adc_trigger ();
	wait_idle ();

	while ( ap->fifo0_count > 0 ) {
	    *buf = ap->fifo0_data;
	    rv++;
	}

	return rv;
}

/* wait for completion interrupt.
 * the ADC is very fast.  We might as well just poll.
 * We use thr_block/unblock as they are very light weight.
 */
static void
wait_idle ( void )
{
	struct adc *ap = ADC_BASE;

	waiting_thread = cur_thread;
	ap->irqena_set = INT_EOS;

	thr_block ( WAIT );
}

/* This works, but perhaps it would be better to poll on the BUSY bit ?
 */
static void
wait_idle_poll ( void )
{
	struct adc *ap = ADC_BASE;

	while ( (ap->status & STAT_MASK) == STAT_IDLE )
	    ;
	while ( (ap->status & STAT_MASK) != STAT_IDLE )
	    ;
}

/* ------------------------------------------- */
/* ------------------------------------------- */
/* A lot of what is above here is for test and
 *  development and will get cleaned up (or deleted)
 *   when I am finished with this driver
 */
/* ------------------------------------------- */

/* Read a specified channel once */
int
adc_read ( int chan )
{
	struct adc *ap = ADC_BASE;

	setup_single ( chan );
	adc_mode = MODE_SINGLE;
	adc_trigger ();
	wait_idle ();

	if ( ap->fifo0_count < 1 )
	    return -1;

	return ap->fifo0_data;
}

/* Read a single channel willy-nilly */
void
adc_run ( int chan )
{
	struct adc *ap = ADC_BASE;

	data_count = 0;
	adc_mode = MODE_RUN;
	ap->fifo0_thresh = 20;
	setup_run ( chan );
	ap->irqena_set = INT_THR0;
	adc_trigger ();

#ifdef notdef
	// wait_idle ();
	waiting_thread = cur_thread;
	// ap->irqena_set = INT_THR0;
	thr_block ( WAIT );
	ap->irqena_clr = INT_THR0;
	ap->step_enable = 0;

	printf ( "Run finished with %d points\n", data_count );
	flush_fifo ();
#endif
}

/* Read a range of channels once */
void
adc_scan ( int *buf, int num )
{
	struct adc *ap = ADC_BASE;
	int i;

	setup_scan ( num );
	adc_mode = MODE_SINGLE;
	adc_trigger ();
	wait_idle ();

	if ( ap->fifo0_count < num ) {
	    printf ( "ADC fifo underrun %d %d\n", ap->fifo0_count, num );
	    return;
	}

	for ( i=0; i<num; i++ )
	    *buf++ = ap->fifo0_data;
}

static void
test1 ( void )
{
	int i, j;
	int data;
	int val;
	int buf[8];

	for ( i=0; i<5; i++ ) {
	    data = adc_read ( 0 );
	    val = (180 * data) / 4095;
	    printf ( " ADC data: %08x %d  %d\n", data, data, val );
	}

	for ( i=0; i<4; i++ ) {
	    adc_scan ( buf, 8 );
	    for ( j=0; j<8; j++ )
		printf ( " %5d", buf[j] );
	    printf ( "\n" );
	}

	/* VCC with 0.5 divider */
	for ( i=0; i<5; i++ ) {
	    data = adc_read ( CHAN_VCC );
	    val = (2 * 180 * data) / 4095;
	    printf ( " ADC vcc: %08x %d  %d\n", data, data, val );
	}

	/* reference */
	for ( i=0; i<5; i++ ) {
	    data = adc_read ( CHAN_VREF );
	    val = (180 * data) / 4095;
	    printf ( " ADC ref: %08x %d  %d\n", data, data, val );
	}
}

/* This is not entirely clean, we should maybe poll till idle */
static void
stop_run ( void )
{
	struct adc *ap = ADC_BASE;

	ap->step_enable = 0;
	ap->irqena_clr = INT_THR0;
	flush_fifo ();
}

/* The following shows we get 61290 samples per second.
 * This is with 16x averaging and the MAGIC_DELAY of 152.
 * So this is a sample every 16.316 microseconds.
 * So with 16x averaging, this means we sample every microsecond.
 *
 * Indeed, if we change to 1x averaging, we get 143858 samples,
 * which is not 16x faster (but is every 6.95 microseconds).
 *
 * With 1x averaging, we get 143858 samples per second.
 * With 2x averaging, we get 132000 samples per second.
 * With 4x averaging, we get 113322 samples per second.
 * With 8x averaging, we get 88326 samples per second.
 * With 16x averaging, we get 61290 samples per second.
 *
 * The TRM says that the ADC gets a 24 Mhz master clock and
 * that it can sample as fast as every 15 clock cycles.
 * The 3359 datasheet says it can sample at 200 Khz
 * I don't know how to reconcile these two statements.
 *
 * When I set the MAGIC_DELAY to 0 and use 1x averaging,
 *  I get 1601670 samples per second.
 * This is 0.624 microseconds per sample.
 * Note that 15/24 is 0.625 microseconds,
 * so this is exactly as it should be!
 * Just what the data looks like, who can say.
 *
 * With Delay = 0 and Avg = 16,
 *  I get 100107 samples per second.
 *  Note that 0.625 * 16 = 10, so this is correct.
 */

static void
test2 ( void )
{
	int i;
	int count;
	int last;

	adc_run ( 0 );
	last = data_count;
	printf ( "Data count: %d\n", last );


	for ( i=0; i<8; i++ ) {
	    thr_delay ( 1000 );
	    count = data_count;
	    printf ( "Data count: %d\n", count-last );
	    last = count;
	}

	stop_run ();
}

void
adc_init ( void )
{
	struct adc *ap = ADC_BASE;

	/* yields 0x47300001, which is correct */
	// printf ( "ADC revision = %08x\n", ap->rev );

	/* Not actually using this yet */
	select_hw_trigger ();

	waiting_thread = NULL;

	ap->control |= CTRL_UNLOCK;
	ap->control |= CTRL_ENA;

	irq_hookup ( IRQ_ADC, adc_isr, 0 );
}

/* Called from IO test menu */
void
adc_test ( void )
{
	// test1 ();
	test2 ();
}

/* THE END */
