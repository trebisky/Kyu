/*
 * Copyright (C) 2020  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* This is a little "demo driver" for the HDC1008.
 * I have one of these on a little Adafruit board.
 * It uses I2C address 0x40, 0x41, 0x42 or 0x43
 *  (without jumpers, it uses 0x40)
 * This is a Texas Instruments part.
 * It can be run from 3.3 or 5 volts.
 *
 *  HDC1008 temperature and humidity sensor
 */

#include <kyu.h>
#include "i2c.h"

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* HDC1008 driver
 */
#define HDC_ADDR	0x40

#define HDC_TEMP	0x00
#define HDC_HUM		0x01
#define HDC_CON		0x02
#define HDC_SN1		0xFB
#define HDC_SN2		0xFC
#define HDC_SN3		0xFD

/* bits/fields in config register */
#define HDC_RESET	0x8000
#define HDC_HEAT	0x2000
#define HDC_BOTH	0x1000
#define HDC_BSTAT	0x0800

#define HDC_TRES14	0x0000
#define HDC_TRES11	0x0400

#define HDC_HRES14	0x0000
#define HDC_HRES11	0x0100
#define HDC_HRES8	0x0200

/* Delays in microseconds */
#define CONV_8		2500
#define CONV_11		3650
#define CONV_14		6350

#define CONV_BOTH	12700

/* Read gets both T then H */
static void
hdc_con_both ( struct i2c *ip )
{
	unsigned char buf[3];

	//(void) iic_write16 ( HDC_ADDR, HDC_CON, HDC_BOTH );
	buf[0] = HDC_CON;
	buf[1] = HDC_BOTH >> 8;
	buf[2] = HDC_BOTH & 0xff;
	i2c_send ( ip, HDC_ADDR, buf, 3 );
}

/* Read gets either T or H */
static void
hdc_con_single ( struct i2c *ip )
{
	unsigned char buf[3];

	// (void) iic_write16 ( HDC_ADDR, HDC_CON, 0 );
	buf[0] = HDC_CON;
	buf[1] = 0;
	buf[2] = 0;
	i2c_send ( ip, HDC_ADDR, buf, 3 );
}

static void
hdc_read_both ( struct i2c *ip, unsigned short *buf )
{
	unsigned char bbuf[4];

	bbuf[0] = HDC_TEMP;
	i2c_send ( ip, HDC_ADDR, bbuf, 1 );

	delay_us ( CONV_BOTH );

	i2c_recv ( ip, HDC_ADDR, bbuf, 4 );
	buf[0] = bbuf[0] << 8 | bbuf[1];
	buf[1] = bbuf[2] << 8 | bbuf[3];
}

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

static void
hdc_once ( struct i2c *ip )
{
	unsigned short iobuf[2];
	int t, h;
	int tf;

	hdc_read_both ( ip, iobuf );

	/*
	t = 99;
	h = 23;
	tf = 0;
	printf ( " Bogus t, tf, h = %d  %d %d\n", t, tf, h );
	*/
	// printf ( " HDC raw t,h = %04x  %04x\n", iobuf[0], iobuf[1] );

	t = ((iobuf[0] * 165) / 65536)- 40;
	tf = t * 18 / 10 + 32;
	// printf ( " -- traw, t, tf = %04x %d   %d %d\n", iobuf[0], iobuf[0], t, tf );

	//h = ((iobuf[1] * 100) / 65536);
	//printf ( "HDC t, tf, h = %d  %d %d\n", t, tf, h );

	h = ((iobuf[1] * 100) / 65536);
	// printf ( " -- hraw, h = %04x %d    %d\n", iobuf[1], iobuf[1], h );
	printf ( "HDC t,f = %d %d\n", tf, h );
}

#define I2C_PORT	0

void
hdc_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( I2C_PORT );

	// thr_new_repeat ( "hdc", hdc_once, ip, 13, 0, 1000 );
	hdc_once ( ip );
}

/* The tests are run via the shell "x" command, i.e.
 *
 * x hdc_test
 */

/* THE END */
