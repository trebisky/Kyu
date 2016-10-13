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

#include <kyulib.h>
#include <interrupts.h>
// #include <omap_mux.h>

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
 *  (which is fine by us).
 */
#define STEP_FIFO1			0x04000000

/* documentation numbers channels 1-8
 * VREF is "channel 9" and on.
 */
#define	STEP_CHAN(x)			((x)<<19)

/* "real" channels are 0-7
 * and on the BBB channel 8 comes from a fixed 1.65 volt divider.
 */
#define CHAN_VCC		7
#define CHAN_VREF		8

/* bits in the control register
 * We use ENA, and might use TAG
 */
#define CTRL_ENA		0x0001
#define CTRL_TAG		0x0002
#define CTRL_UNLOCK		0x0004
#define CTRL_BIAS		0x0008
#define CTRL_PWRDOWN		0x0010
/* more bits here (I skip) for tsc control */
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

static unsigned int step_enables;

/* ------------------------------ */
/* ------------------------------ */

/* Possible hardware trigger sources */
#define HWT_PRU		0
#define HWT_TIMER4	1
#define HWT_TIMER5	2
#define HWT_TIMER6	3
#define HWT_TIMER7	4

static void
hw_trigger ( void )
{
	cm_adc_mux ( HWT_TIMER4 );
}

/* ------------------------------ */
/* ------------------------------ */

void
adc_isr ( int xxx )
{
	printf ( "ADC interrupt\n" );
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
	// ap->steps[0].config = STEP_AVG_16 | STEP_MODE_SW_CONT | STEP_CHAN(chan);
	ap->steps[0].delay = MAGIC_DELAY;

	step_enables = 1<<1;
	// ap->step_enable = step_enables;
}

static void
setup_scan ( int num )
{
	struct adc *ap = ADC_BASE;
	int config;
	int step, chan;

	step_enables = 0;

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

/* This works, but perhaps it would be better to poll on the BUSY bit ?
 */
static void
wait_idle ( void )
{
	struct adc *ap = ADC_BASE;

	while ( (ap->status & STAT_MASK) == STAT_IDLE )
	    ;
	while ( (ap->status & STAT_MASK) != STAT_IDLE )
	    ;
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
	adc_trigger ();
	wait_idle ();

	if ( ap->fifo0_count < 1 )
	    return -1;

	return ap->fifo0_data;
}

/* Read a range of channels once */
void
adc_scan ( int *buf, int num )
{
	struct adc *ap = ADC_BASE;
	int i;

	setup_scan ( num );
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
test ( void )
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

void
adc_init ( void )
{
	struct adc *ap = ADC_BASE;

	/* yields 0x47300001, which is correct */
	// printf ( "ADC revision = %08x\n", ap->rev );

	ap->control |= CTRL_UNLOCK;
	ap->control |= CTRL_ENA;

	irq_hookup ( IRQ_ADC, adc_isr, 0 );
}

/* Called from IO test menu */
void
adc_test ( void )
{
	test ();
}

/* THE END */
