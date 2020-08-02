/*
 * Copyright (C) 2020  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* This is a driver for a 16x2 LCD display
 * The ones I have use a PCF8574 i2c chip.
 * The display itself is based on a Hitachi HD44780.
 * These gadgets are cheap, simple, and well known.
 *
 * It seems that you MUST run this from 5 volts and use a level shifter.
 */

#include <kyu.h>
#include "i2c.h"

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* i2c LCD driver
 */
#define LCD_ADDR	0x27

#define LCD_LIGHT	0x08	/* turn backlight LED on */
#define LCD_ENA		0x04	/* for strobe pulse */
#define LCD_READ	0x02	/* we never read */
#define LCD_DATA	0x01	/* 0 is command */

#define LCD_WIDTH	16

/* Base addresses for up to 4 lines (we have only 2)
 */
#define LCD_LINE_1	0x80
#define LCD_LINE_2	0xC0
// #define LCD_LINE_3	0x94
// #define LCD_LINE_4	0xD4

static int use_led = LCD_LIGHT;
// static int use_led = 0;

/* Commands to the HD44780 */

/* The following will get sent to put the device
 * into 4 bit mode.  We don't know what mode it is
 * in when we get hold of it, but sending 0011
 * three times will put it into 8 bit mode.
 * Then sending 0010 will put it into 4 bit mode,
 * which is what we want.
 */
#define INIT1		0x33
#define INIT2		0x32

/* This sets two bits (the low 2), which are "IS"
 * I = 1 says to increment by one in display ram when a char is written.
 * S = 0 says to NOT shift the display on a write.
 */
#define INIT_CURSOR	0x06

/* This sets three bits (the low three), which are "DCB"
 * D = 1 says to turn on the display
 * C = 0 says no cursor.
 * B = 0 says no blink.
 */
#define INIT_DISPLAY	0x0c

/* The two low bits are "don't care", then we have 3 bits "DNF"
 * D = 0  says to use 4 bit mode
 * N = 1  says there are 2 lines (rather than one)
 * F = 0  says use 5x8 font (rather than 5x10)
 */
#define INIT_FUNC	0x28

#define CLEAR_DISPLAY	0x01

#define DELAY_INIT	500
// #define DELAY_NORM	500	ok
// #define DELAY_NORM	400	ok
// #define DELAY_NORM	200	too short
// #define DELAY_NORM	250	too short
#define DELAY_NORM	300

/* ------------------------------------------------------- */

/* The 8574 is pleasantly simple.
 * no complex initialization or setup,
 * just write 8 bits to it and they appears on the output pins.
 */

static void
lcd_write ( struct i2c *ip, int data )
{
	unsigned char buf[2];

	buf[0] = data;
	i2c_send ( ip, LCD_ADDR, buf, 1 );
}

/* Send data, strobe the ENA line.
 * The shortest possible pulse works fine.
 * There is no need for extra delays.
 */
static void
lcd_send ( struct i2c *ip, int data )
{
	lcd_write ( ip, data );

	lcd_write ( ip, data | LCD_ENA );
	// delay_us ( 50 );

	lcd_write ( ip, data );
	delay_us ( DELAY_NORM );
}

/* Send data to the display */
static void
lcd_data ( struct i2c *ip, int data )
{
	int hi, lo;

	hi = use_led | LCD_DATA | (data & 0xf0);
	lo = use_led | LCD_DATA | ((data<<4) & 0xf0);

	lcd_send ( ip, hi );
	lcd_send ( ip, lo );
}

/* Send configuration commands to the unit */
static void
lcd_cmd ( struct i2c *ip, int data )
{
	int hi, lo;

	hi = use_led | (data & 0xf0);
	lo = use_led | ((data<<4) & 0xf0);

	lcd_send ( ip, hi );
	lcd_send ( ip, lo );
}

static void
lcd_string ( struct i2c *ip, char *msg )
{
	int i;

	for ( i=0; i<LCD_WIDTH; i++ ) {
	    if ( *msg )
		lcd_data ( ip, *msg++ );
	    else
		lcd_data ( ip, ' ' );
	}
}

void
lcd_msg ( struct i2c *ip, char *msg )
{
	lcd_cmd ( ip, LCD_LINE_1 );
	lcd_string ( ip, msg );
}

void
lcd_msg2 ( struct i2c *ip, char *msg )
{
	lcd_cmd ( ip, LCD_LINE_2 );
	lcd_string ( ip, msg );
}

#ifdef notdef
void
lcd_xxx ( struct i2c *ip )
{
	int i;

	lcd_cmd ( ip, LCD_LINE_1 );

	for ( i=0; i<LCD_WIDTH; i++ ) {
	    lcd_data ( ip, 'X' );
	}
}

/* Get it into 4 bit mode */
void
lcd_init_4X ( struct i2c *ip )
{
   delay_us (15000);             // wait 15msec
   lcd_write (ip,0b00110100); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=1
   lcd_write (ip,0b00110000); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=0
   delay_us (4100);              // wait 4.1msec
   lcd_write (ip,0b00110100); // 
   lcd_write (ip,0b00110000); // same
   delay_us (100);               // wait 100usec
   lcd_write (ip,0b00110100); //
   lcd_write (ip,0b00110000); // 8-bit mode init complete
   delay_us (4100);              // wait 4.1msec
   lcd_write (ip,0b00100100); //
   lcd_write (ip,0b00100000); // switched now to 4-bit mode
}
#endif

/* Get it into 4 bit mode */
void
lcd_init_4 ( struct i2c *ip )
{
	/* get into 4 bit mode */
	lcd_cmd ( ip, INIT1 );
	lcd_cmd ( ip, INIT2 );
}

void
lcd_init ( struct i2c *ip )
{
	lcd_init_4 ( ip );

	lcd_cmd ( ip, INIT_CURSOR );
	lcd_cmd ( ip, INIT_DISPLAY );
	lcd_cmd ( ip, INIT_FUNC );

	lcd_cmd ( ip, CLEAR_DISPLAY );

	// important
	delay_us ( DELAY_INIT );
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */

#define I2C_PORT	0

void
lcd_test ( void )
{
	struct i2c *ip;

	printf ( "Testing LCD\n" );
	ip = i2c_hw_new ( I2C_PORT );

	lcd_init ( ip );

	lcd_msg ( ip, "Eat more fish" );
	lcd_msg2 ( ip, "GPS 5244" );

	// lcd_xxx ( ip );
	printf ( "Done testing LCD\n" );
}

/* THE END */
