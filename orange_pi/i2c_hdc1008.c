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

/* These are the registers in the device */
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

/* We either read or write some register on the device.
 * to write, there is one i2c transaction
 *  we send 3 bytes (one is the register, then next two the value)
 * to read, there are two i2c transations
 *  The first sends one byte (the register number)
 *  The second reads two bytes (the register value)
 */

/* Write to the config register -- tell it to do both conversions */
static void
hdc_con_both ( struct i2c *ip )
{
	unsigned char buf[3];

	//(void) iic_write16 ( HDC_ADDR, HDC_CON, HDC_BOTH );
	buf[0] = HDC_CON;
	buf[1] = HDC_BOTH >> 8;
	buf[2] = HDC_BOTH & 0xff;
	i2c_send ( ip, HDC_ADDR, buf, 3 );

	/* unfinished */
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

	/* unfinished */
}

/* Get the contents of the config register
 * After reset, the value seems to be 0x1000
 *  which is HDC_BOTH
 */
static void
hdc_con_get ( struct i2c *ip )
{
	unsigned char buf[2];
	int val;

	buf[0] = HDC_CON;
	i2c_send ( ip, HDC_ADDR, buf, 1 );

	i2c_recv ( ip, HDC_ADDR, buf, 2 );
	val = buf[0] << 8 | buf[1];

	printf ( "HDC1008 config = %04x\n", val );
}

#ifdef notdef
/* Let's see if this autoincrements across 3 registers
 * it does not:
 * I get: 023c ffff ffff
 */
static void
hdc_xserial_get ( struct i2c *ip )
{
	unsigned char buf[6];
	int s1, s2, s3;

	buf[0] = HDC_SN1;
	i2c_send ( ip, HDC_ADDR, buf, 1 );

	i2c_recv ( ip, HDC_ADDR, buf, 6 );

	s1 = buf[0] << 8 | buf[1];
	s2 = buf[2] << 8 | buf[3];
	s3 = buf[4] << 8 | buf[5];

	printf ( "HDC1008 X serial = %04x %04x %04x\n", s1, s2, s3 );
}
#endif

/* Get the 3 bytes of serial information.
 * I get: 023c ecab ee00
 */
static void
hdc_serial_get ( struct i2c *ip )
{
	unsigned char buf[2];
	int s1, s2, s3;

	buf[0] = HDC_SN1;
	i2c_send ( ip, HDC_ADDR, buf, 1 );
	i2c_recv ( ip, HDC_ADDR, buf, 6 );
	s1 = buf[0] << 8 | buf[1];

	buf[0] = HDC_SN2;
	i2c_send ( ip, HDC_ADDR, buf, 1 );
	i2c_recv ( ip, HDC_ADDR, buf, 6 );
	s2 = buf[0] << 8 | buf[1];

	buf[0] = HDC_SN3;
	i2c_send ( ip, HDC_ADDR, buf, 1 );
	i2c_recv ( ip, HDC_ADDR, buf, 6 );
	s3 = buf[0] << 8 | buf[1];

	printf ( "HDC1008 serial = %04x %04x %04x\n", s1, s2, s3 );
}

/* Read both T and H */
/* Here we write the register pointer to 0 (HDC_TEMP)
 * then read 4 bytes (two registers).  This back to back read
 * is described in the datasheet.  Apparently the internal
 * register pointer increments automatically to the next
 * register.  Perhaps we could read 6 bytes and also read
 * the config register, which comes next.
 *
 * The device seems to power up in HDC_BOTH mode.
 */

static void
hdc_read_both ( struct i2c *ip, unsigned int *buf )
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
	unsigned int iobuf[2];
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

	printf ( " -- traw, t, tf = %04x %d   %d %d\n", iobuf[0], iobuf[0], t, tf );

	//h = ((iobuf[1] * 100) / 65536);
	//printf ( "HDC t, tf, h = %d  %d %d\n", t, tf, h );

	h = ((iobuf[1] * 100) / 65536);
	// printf ( " -- hraw, h = %04x %d    %d\n", iobuf[1], iobuf[1], h );

	printf ( "HDC temp(F), humid = %d %d\n", tf, h );
}

#define I2C_PORT	0

void
hdc_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( I2C_PORT );

	// thr_new_repeat ( "hdc", hdc_once, ip, 13, 0, 1000 );
	hdc_once ( ip );
	hdc_con_get ( ip );
	hdc_serial_get ( ip );
}

/* The tests are run via the shell "x" command, i.e.
 *
 * x hdc_test
 */

/* THE END */
