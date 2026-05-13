/*
 * Copyright (C) 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * i2c.c for the Zynq
 *
 * Tom Trebisky  5-12-2026
 */

/* See the TRM page 793 under "register details, module summary"
 */
#define I2C0_BASE	0xE0004000
#define I2C1_BASE	0xE0005000

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#define BIT(x)  (1<<(x))

/* See the TRM page 1382 for these register details
 * While the TRM has a chapter (Ch. 20, page 610) about the
 * i2c controllers, it is more or less useless without the
 * register details on page 1382.
 */
struct iic {
	vu32	cr;		/* control register */
	vu32	sr;		/* status register - RO */
	vu32	addr;
	vu32	data;
	vu32	isr;
	vu32	xfer_size;
	vu32	slv_pause;;
	vu32	timeout;;
	vu32	imr;	/* interrupt mask - RO */
	vu32	ier;	/* interrupt enable*/
	vu32	idr;	/* interrupt disable*/
};

#define I2C_BASE	((struct iic *) I2C0_BASE)

/* See the TRM ppage 231 in Chapter 7
 */
#define IRQ_I2C0	57
#define IRQ_I2C1	80

void
i2c_init ( void )
{
}

/* THE END */
