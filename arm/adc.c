/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * adc.c
 *
 * driver for the analog input section of the BBB
 *
 * This is called the "touchscreen controller" in the TRM
 * and is described in section 12.
 * It is also referred to as the ADC_TSC
 *
 * It is an 8 channel, 12 bit adc capable of 125 ns conversions
 * 
 */

#define ADC_BASE      ( (struct adc *) 0x44E0D000 )

#include <kyulib.h>
#include <omap_mux.h>

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

	volatile unsigned long ctrl;		/* 0x40 */
	volatile unsigned long adc_stat;	/* 0x44 */
	volatile unsigned long adc_range;	/* 0x48 */
	volatile unsigned long adc_clkdiv;	/* 0x4c */
	volatile unsigned long adc_misc;	/* 0x50 */

	volatile unsigned long step_enable;	/* 0x54 */
	volatile unsigned long idle_conf;	/* 0x58 */
	volatile unsigned long ts_ch_stepconf;	/* 0x5c */
	volatile unsigned long ts_ch_delay;	/* 0x60 */

	struct step steps[16];

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

void
adc_init ( void )
{
	struct adc *ap = ADC_BASE;

	printf ( "ADC revision = %08x\n", ap->rev );
}

/* THE END */
