/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* i2c.c
 *
 * Tom Trebisky  Kyu project  6-21-2016
 *
 * This is a "high level" i2c module.
 * This is the "public" i2c interface.
 *
 * It sits above i2c_hw.c (the driver fo hardware device)
 * and iic.c (the gpio driver)
 */

#include "kyu.h"
#include "i2c.h"

struct i2c *
i2c_hw_new ( int num )
{
	struct i2c *ip;
	void *hwp;

	hwp = i2c_hw_init ( num );
	if ( ! hwp )
	    return (struct i2c *) 0;

	ip = (struct i2c *) malloc ( sizeof(struct i2c) );
	ip->type = I2C_HW;
	ip->hw = hwp;
	return ip;
}

/* XXX - At present we only allow one of these.
 */
struct i2c *
i2c_gpio_new ( int sda_pin, int scl_pin )
{
	struct i2c *ip;

	iic_init ( sda_pin, scl_pin );
	ip = (struct i2c *) malloc ( sizeof(struct i2c) );
	ip->type = I2C_GPIO;
	return ip;
}

/* Send and Receive are the fundamental i2c primitives
 * that everything else gets routed through.
 * Return 0 if OK, 1 if trouble (usually a NACK).
 */

int
i2c_send ( struct i2c *ip, int addr, char *buf, int count )
{
	if ( ip->type == I2C_HW ) {
	    return i2c_hw_send ( ip->hw, addr, buf, count );
	} else {
	    return iic_send ( addr, buf, count );
	}
}

int
i2c_recv ( struct i2c *ip, int addr, char *buf, int count )
{
	if ( ip->type == I2C_HW ) {
	    return i2c_hw_recv ( ip->hw, addr, buf, count );
	} else {
	    return iic_recv ( addr, buf, count );
	}
}

/* ----------------------------------------------------- */
/* Handy routines */

void
i2c_write_8 ( struct i2c *ip, int addr, int reg, int val )
{
	char buf[2];

	buf[0] = reg;
	buf[1] = val;

	if ( i2c_send ( ip, addr, buf, 2 ) )
	    printf ( "i2c_write, send trouble\n" );
}

void
i2c_read_reg_n ( struct i2c *ip, int addr, int reg, unsigned char *buf, int n )
{
	buf[0] = reg;

	if ( i2c_send ( ip, addr, buf, 1 ) )
	    printf ( "send Trouble\n" );

	if ( i2c_recv ( ip, addr, buf, n ) )
	    printf ( "recv Trouble\n" );
}

#ifdef notdef
/* rather than provide this with an internal limit,
 * let the caller read the buffer of bytes and
 * repack it themselves..
 */
void
i2c_read_16n ( int reg, unsigned short *sbuf, int n )
{
	unsigned char buf[64];	/* XXX */
	int i;
	int j;

	i2c_read_reg_n ( ip, addr, reg, buf, n * 2 );

	for ( i=0; i<n; i++ ) {
	    j = i + i;
	    sbuf[i] = buf[j]<<8 | buf[j+1];
	}
}
#endif

int
i2c_read_8 ( struct i2c *ip, int addr, int reg )
{
	char buf[1];

	i2c_read_reg_n ( ip, addr, reg, buf, 1 );

	return buf[0] & 0xff;
}

int
i2c_read_16 ( struct i2c *ip, int addr, int reg )
{
	int rv;
	unsigned char buf[2];

	buf[0] = reg;

	i2c_read_reg_n ( ip, addr, reg, buf, 2 );

        rv = buf[0] << 8;
        rv |= buf[1];

	return rv;
}

int
i2c_read_24 ( struct i2c *ip, int addr, int reg )
{
	int rv;
	unsigned char buf[3];

	buf[0] = reg;

	i2c_read_reg_n ( ip, addr, reg, buf, 3 );

	rv =  buf[0] << 16;
        rv |= buf[1] << 8;
        rv |= buf[2];

	return rv;
}

/* THE END */
