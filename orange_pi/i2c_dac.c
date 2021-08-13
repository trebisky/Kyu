/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* MCP4725 dac demo/driver
 * 11-13-2020
 * taken from bbb/i2c_demos.c
 */
#include <kyu.h>
#include "i2c.h"

#ifdef ARCH_ARM
#include "gpio.h"
#define os_printf	printf
#define ICACHE_FLASH_ATTR
#define os_delay_us(x)	delay_us ( (x) )
#endif

/* MCP4725 driver follows
 * This device is peculiar in that it does not have registers
 * You either read or write.
 * read always returns 3 bytes.
 * write has 4 flavors.
 */
#define DAC_ADDR	0x60

static void ICACHE_FLASH_ATTR
dac_write ( struct i2c *ip, unsigned int val )
{
	unsigned char iobuf[2];

	iobuf[0] = (val >> 8) & 0xf;
	iobuf[1] = val & 0xff;
	i2c_send ( ip, DAC_ADDR, iobuf, 2 );
}

static void ICACHE_FLASH_ATTR
dac_write_ee ( struct i2c *ip, unsigned int val )
{
	unsigned char iobuf[3];

	iobuf[0] = 0x60;
	iobuf[1] = val >> 4;
	// iobuf[2] = (val & 0xf) << 4;
	iobuf[2] = val << 4;
	i2c_send ( ip, DAC_ADDR, iobuf, 3 );
}

/* returns up to 5 bytes.
 * "status" byte
 * DAC value (2 bytes)
 * EEPROM value (2 bytes)
 */
static void ICACHE_FLASH_ATTR
dac_read ( struct i2c *ip, unsigned char *buf, int n )
{
	iic_recv ( ip, DAC_ADDR, buf, n );
}

static unsigned int ICACHE_FLASH_ATTR
dac_read_val ( struct i2c *ip )
{
	unsigned char iobuf[3];

	iic_recv ( ip, DAC_ADDR, iobuf, 3 );
	return (iobuf[1] << 4) | (iobuf[2] >> 4);
}

/* ----------------------------------------------------- */

static void
dac_show ( struct i2c *ip )
{
	unsigned char io[5];

	dac_read ( ip, io, 5 );

	os_printf ( "DAC status = %02x\n", io[0] );
	os_printf ( "DAC val = %02x %02x\n", io[1], io[2] );
	os_printf ( "DAC ee = %02x %02x\n", io[3], io[4] );
}

static unsigned int dac_val = 0;

static void
dac_once ( struct i2c *ip )
{
	dac_val += 16;
	if ( dac_val > 4096 ) dac_val = 0;
	dac_write ( ip, dac_val);
}

static void
dac_fast ( struct i2c *ip )
{
	dac_val = 0;

	for ( ;; ) {
	    dac_val += 16;
	    if ( dac_val > 4096 ) dac_val = 0;
	    dac_write ( ip, dac_val);
	    // os_delay_us ( 1 );
	}
}

/* This showed that with back to back writes like this,
 * I can update the dac every 319 microseconds.
 * This is a dac update rate of 3140 Hz.
 * And this is about right with a 100 kHz clock.
 * We send 3 bytes (address and two data) and if you
 * figure 10 bits on the wire for each byte this is
 * 30 bits so 100 khz / 30 = 3.3 khz
 */

static void
dac_pulse ( struct i2c *ip )
{
	for ( ;; ) {
	    dac_write ( ip, 0 );
	    dac_write ( ip, 0xfff );
	}
}


void
dac_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( 1 );

	dac_show ( ip );
	dac_show ( ip );
	dac_show ( ip );

	// thr_new_repeat ( "dac", dac_once, ip, 13, 0, 1 );
	// dac_fast ( ip );
	dac_pulse ( ip );
}

/* THE END */
