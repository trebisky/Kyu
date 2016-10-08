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
 * and is described in section 12.
 * It is also referred to as the ADC_TSC
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
#include <omap_mux.h>

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

	struct step charge;			/* ox5c */
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

/* If we don't set this, results go to Fifo 0 */
#define STEP_FIFO1			0x04000000

/* documentation numbers channels 1-8
 * VREF is "channel 9" and on.
 */
#define	STEP_CHAN_1			0x00
#define	STEP_CHAN_2			0x00080000
#define	STEP_CHAN_8			0x00380000
#define	STEP_CHAN_VREF			0x00400000
#define	STEP_CHAN(x)			((x)<<19)

/* "real" channels are 0-7 */
#define CHAN_VREF		8

/* bits in the control register
 * We use ENA, and might use TAG
 */
#define CTRL_ENA		0x0001
#define CTRL_TAG		0x0002
#define CTRL_UNLOCK		0x0004
#define CTRL_BIAS		0x0008
#define CTRL_PWRDOWN		0x0010
/* bits here for tsc control */
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

void
adc_enable ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control |= CTRL_ENA;
}

void
adc_trigger ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control &= ~CTRL_ENA;
	ap->control |= CTRL_ENA;
}

void
adc_disable ( void )
{
	struct adc *ap = ADC_BASE;

	ap->control &= ~CTRL_ENA;
}


void
setup_once ( int chan )
{
	struct adc *ap = ADC_BASE;

	// ap->steps[0].config = STEP_AVG_16 | STEP_MODE_SW_ONE | STEP_CHAN(chan);
	ap->steps[0].config = STEP_AVG_16 | STEP_MODE_SW_CONT | STEP_CHAN(chan);
	ap->steps[0].delay = 152;
	ap->step_enable = 0x2;
}

void
show_fifo ( void )
{
	struct adc *ap = ADC_BASE;
	unsigned int data;

	printf ( "ADC fifo, se, control, status:  %d  %08x %08x %08x\n", ap->fifo0_count,
	    ap->step_enable, ap->control, ap->status ); 
	if ( ap->fifo0_count > 0 ) {
	    data = ap->fifo0_data;
	    printf ( "ADC data: %08x\n", data );
	}
}

void
watch_fifo ( void )
{
	struct adc *ap = ADC_BASE;
	int i;

	for ( i=0; i<10; i++ ) {
	    thr_delay ( 10 );
	    show_fifo ();
	    // adc_disable ();
	    // setup_once ( CHAN_VREF );
	    // adc_enable ();
	    ap->step_enable = 0x2;
	}
}

void
poll_data ( void )
{
	struct adc *ap = ADC_BASE;
	unsigned int data;

	while ( ap->fifo0_count <= 0 )
	    ;
	show_fifo ();
	data = ap->fifo0_data;
	show_fifo ();
	printf ( "ADC data: %08x\n", data );
}

void
adc_init ( void )
{
	struct adc *ap = ADC_BASE;
	int i;

	/* yields 0x47300001, which is correct */
	// printf ( "ADC revision = %08x\n", ap->rev );

	setup_once ( CHAN_VREF );
	show_fifo ();
	adc_enable ();
	show_fifo ();

	thr_delay ( 2 );
	show_fifo ();

	// adc_disable ();
	// show_fifo ();
	// printf ( "\n" );

	watch_fifo ();

#ifdef notdef
	for ( i=0; i<5; i++ ) {
	    adc_trigger ();
	    poll_data ();
	}
#endif

	/*
	show_fifo ();
	thr_delay ( 500 );
	show_fifo ();
	*/

}

/* THE END */
