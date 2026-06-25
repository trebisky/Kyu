/*
 * Copyright (C) 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* dallas.c
 *
 * Tom Trebisky  Kyu project  6-23-2026
 *
 * This is a driver for the "Dallas Semiconductor" one wire protocol.
 */

#include "kyu.h"
#include "gpio.h"

/* We tried A7 first and got a short burst of pulses,
 * then the line just stayed high.
 * We tried A10, which didn't seem to work at all.
 * We try C0 and get nothing.
 * A8 worked with thr_delay(), but when I try
 * 10 us delays I just get the short burst.
 */
#define OW_PIN	GPIO_A_8

/* From Maxim AN126 - delays in us */
#define DELAY_A		6
#define DELAY_B		64
#define DELAY_C		60
#define DELAY_D		10
#define DELAY_E		9
#define DELAY_F		55
#define DELAY_G		0
#define DELAY_H		480
#define DELAY_I		70
#define DELAY_J		410

#define ow_Delay(x)	delay_us ( x );

static int dallas_pin;

void
dallas_init ( int pin )
{
	dallas_pin = pin;
	gpio_one_wire ( dallas_pin );
}

#define owLow	gpio_dir_out ( dallas_pin )
#define owHigh	gpio_dir_in ( dallas_pin )

static inline int
owRead ( void )
{
	return gpio_read_bit ( dallas_pin );
}

int
ow_reset ( void )
{
	int rv;

	ow_Delay ( DELAY_G );
	owLow;
	ow_Delay ( DELAY_H );
	owHigh;
	ow_Delay ( DELAY_I );
	rv = owRead ();
	ow_Delay ( DELAY_J );
	return rv;
}

void
ow_write_bit ( int bit )
{
	if ( bit ) {
		owLow;
		ow_Delay ( DELAY_A );
		owHigh;
		ow_Delay ( DELAY_B );
	} else {
		owLow;
		ow_Delay ( DELAY_C );
		owHigh;
		ow_Delay ( DELAY_D );
	}
}

int
ow_read_bit ( void )
{
	int rv;

	owLow;
	ow_Delay ( DELAY_A );
	owHigh;
	ow_Delay ( DELAY_E );
	rv = owRead ();
	ow_Delay ( DELAY_F );
	return rv;
}

// Write a byte
//  it goes out lsb first
void
ow_write ( int data )
{
	int i;

	for ( i=0; i<8; i++ ) {
		ow_write_bit ( data & 0x01 );
		data >>= 1;
	}
}

// Read a byte
//  we get lsb first
int
ow_read ( void )
{
	int i;
	int rv = 0;

	for ( i=0; i<8; i++ ) {
		rv >>= 1;
		if ( ow_read_bit () )
			rv |= 0x80;
	}
	return rv;
}

/* ======================================================================== */
/* common routines */

/* Read 8 byte ROM code
 */
void
ow_rom_read ( void )
{
	int val;
	int i;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	ow_write ( 0x33 );	// read ROM

	for ( i=0; i<8; i++ ) {
		val = ow_read ();
		printf ( "Rom %d: %02x\n", i, val );
	}
}

/* ======================================================================== */
/* DS1994 routines */
/* This is the giant "coin cell" with battery backed nvram and clock */

/* ======================================================================== */
/* DS18B20 routines */


/* Read from a DS18B20
 */
void
ow_temp_read ( void )
{
	int val;
	int i;
	int raw;
	int tf;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0x44 );	// start conversion

	// ow_Delay ( 50 );
	// ow_Delay ( 500 );
	for ( i=0; i<100000; i++ ) {
		val = ow_read_bit ();
		if ( val == 1 )
			break;
	}
	printf ( "Wait done after %d -- %d\n", i, val );
	// I get: Wait done after 11984 -- 1
	// ow_read_bit() takes 55 microseconds, so
	// 11984*70 = 838,880 us, i.e. 0.8 seconds!
	// Almost a 1 second conversion time.
	// The datasheet gives a 750ms conversion
	// time when in 12 bit mode (the power up).

	(void) ow_reset ();
	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xBE );	// read scratchpad
	for ( i=0; i<9; i++ ) {
		val = ow_read ();
		printf ( "Val %d: %02x\n", i, val );
		if ( i == 0 )
			raw = val;
		if ( i == 1 )
			raw = val<<8 | raw;
	}
	printf ( " Raw temp = %04x\n", raw );
	raw >>= 4;
	tf = 32 + (raw*9) / 5;
	printf ( " Temp = %dC %dF\n", raw, tf );
}

void
ow_test ( void )
{
	int val;

	val = ow_reset ();
	printf ( "Dallas reset returns: %d\n", val );
	// ow_temp_read ();
	ow_rom_read ();
}

/* ======================================================================== */
/* ======================================================================== */
/* Test stuff follows */

static int xxx;

/* Don't expect to see anything without an external pullup.
 * This will also keep the processor busy.
 * (The test runs at priority 14, so the shell still responds)
 */
static void
scope_slow ( void )
{
	for ( ;; ) {
		gpio_dir_out ( dallas_pin );
		thr_delay ( 10 );
		gpio_dir_in ( dallas_pin );
		thr_delay ( 10 );
	}
}
static void
scope_fast ( void )
{
	xxx = 0;

	for ( ;; ) {
		xxx++;
		// gpio_one_wire ();
		gpio_dir_out ( dallas_pin );
		delay_us ( 10 );
		// gpio_one_wire ();
		gpio_dir_in ( dallas_pin );
		delay_us ( 10 );
		// if ( xxx % 50000 == 0 )
		//		printf ( "A" );
	}
}

static void
down ( void )
{
	gpio_dir_out ( dallas_pin );
}


/* Called from board.c
 * Use "i 19" from the Kyu prompt
 */
void
dallas_test ( void )
{
	printf ( "Dallas one-wire test begins\n" );
	dallas_init ( OW_PIN );
	// down ();
	// scope_fast ();
	ow_test ();
	printf ( "Dallas one-wire test done\n" );
}
/* THE END */
