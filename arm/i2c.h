/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* i2c.h
 *
 * Tom Trebisky  Kyu project  6-21-2016
 */

#define I2C_HW		1
#define I2C_GPIO	2

/* From i2c_hw.c */
void * i2c_hw_init ( int );

struct i2c *i2c_hw_new ( int );
struct i2c *i2c_gpio_new ( int, int );

int i2c_send ( struct i2c *, int, char *, int );
int i2c_recv ( struct i2c *, int, char *, int );

void i2c_read_reg_n ( struct i2c *, int, int, unsigned char *, int );
void i2c_write_8 ( struct i2c *, int, int, int );

int i2c_read_8  ( struct i2c *, int, int );
int i2c_read_16 ( struct i2c *, int, int );
int i2c_read_24 ( struct i2c *, int, int );

struct i2c {
	int type;
	void *hw;
};

/* THE END */
