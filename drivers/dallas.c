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

/* I thought I could use strncmp for this, but strncmp
 * will stop prematurely on a 0 byte.
 */
static int
verify ( char a[], char b[], int n )
{
	int i;

	for ( i=0; i<n; i++ )
		if ( a[i] != b[i] )
			return 0;
	return 1;
}

void
ow_read_page ( int page, char *buf )
{
	int i;
	int val;
	int offset;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	offset = page * 0x20;

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xf0 );	// read memory
	ow_write ( offset & 0xff );		// TA1
	ow_write ( offset >> 8 );		// TA2

	for ( i=0; i<32; i++ ) {
		buf[i] = ow_read ();
	}
}

/* write to scratchpad
 */
static void
ow_sp_write ( int offset, char *buf, int n )
{
	int val;
	int i;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home 1\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0x0f );	// write scratchpad
	ow_write ( offset & 0xff );	// TA1
	ow_write ( offset >> 8 );	// TA2

	for ( i=0; i<n; i++ )
		ow_write ( buf[i] );
}

static int
ow_sp_check ( char *buf, int n )
{
	int val;
	int i;
	int es;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home - check\n" );
		return 0;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xaa );	// read scratchpad

	val = ow_read ();
	// printf ( "Check 1 = %02x\n", val );
	val = ow_read ();
	// printf ( "Check 2 = %02x\n", val );
	es = ow_read ();
	// printf ( "Check 3 = %02x\n", val );

	for ( i=0; i<n; i++ )
		buf[i] = ow_read ();

	return es;
}

static void
ow_sp_copy ( int offset, int es )
{
	int val;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home 1\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0x55 );	// copy scratchpad
	ow_write ( offset & 0xff );	// TA1
	ow_write ( offset >> 8 );	// TA2
	ow_write ( es );
}

void
ow_write_page ( int pg, char *buf )
{
	int offset;
	int es;
	char buf2[32];

	offset = 0x20 * pg;
	ow_sp_write ( offset, buf, 32 );
	es = ow_sp_check ( buf2, 32 );

	if ( verify ( buf, buf2, 32 ) )
		ow_sp_copy ( offset, es );
	else
		printf ( "Trouble writing page %d\n", pg );
}

/* DS1994 - my experimenta below here */

void
ow_read_all ( void )
{
	int val;
	int i;
	int num;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xf0 );	// read memory
	ow_write ( 0 );		// TA1
	ow_write ( 0 );		// TA2

	num = 512 + 30 + 4;
	for ( i=0; i<num; i++ ) {
		val = ow_read ();
		printf ( "Data %d %04x: %02x\n", i, i, val );
	}
}

/* A special twist on the above.
 * Just read the first 32 bytes (page 0)
 *  and display as an * ascii string.
 */

void
ow_laser_read ( void )
{
	char laser[33];

	ow_read_page ( 0, laser );

	laser[32] = '\0';
	printf ( "String: %s\n", laser );
}

#ifdef notdef
void
ow_laser_read_OLD ( void )
{
	int val;
	int i;
	int num;
	char laser[33];

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xf0 );	// read memory
	ow_write ( 0 );		// TA1
	ow_write ( 0 );		// TA2

	num = 32;
	for ( i=0; i<num; i++ ) {
		val = ow_read ();
		// printf ( "Data %d: %02x\n", i, val );
		laser[i] = val;
	}
	laser[i] = '\0';
	printf ( "String: %s\n", laser );
}

/* Read scratchpad.
 * This works fine, but is useless except immediately
 * after a write
 */
void
ow_sp_read_OLD ( void )
{
	int val;
	int i;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xaa );	// read scratchpad

	for ( i=0; i<36; i++ ) {
		val = ow_read ();
		printf ( "SP %d %04x: %02x\n", i, i, val );
	}
}
#endif

void
ow_test2 ( void )
{
	int offset;
	char buf1[32], buf2[32];
	int es;
	int i;

	for ( i=0; i<32; i++ )
		buf1[i] = i;

	offset = 0x20;
	ow_sp_write ( offset, buf1, 32 );
	es = ow_sp_check ( buf2, 32 );

	for ( i=0; i<32; i++ )
		printf ( "Buf2[%2d] = %02x\n", i, buf2[i] );

	ow_sp_copy ( offset, es );
}

/* The example (in part) from the datasheet
 */
void
ow_sp_test ( void )
{
	int val;
	int i;

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home 1\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0x0f );	// write scratchpad
	ow_write ( 0x26 );	// TA1, offset 6 in page 1
	ow_write ( 0x0 );	// TA2
	ow_write ( 0xab );	// first byte
	ow_write ( 0xcd );	// second byte

	val = ow_reset ();
	if ( val ) {
		printf ( "Nobody home 2\n" );
		return;
	}

	ow_write ( 0xcc );	// skip ROM
	ow_write ( 0xaa );	// read scratchpad

	for ( i=0; i<16; i++ ) {
		val = ow_read ();
		printf ( "SP %d %04x: %02x\n", i, i, val );
	}
}

static void
show_pg ( char *buf, char *ref )
{
	int i;

	for ( i=0; i<32; i++ )
		printf ( "V %2d %02x %02x\n", i, buf[i], ref[i] );
}

/* Here are the 32 bytes we found in page 0 */
static char *ds_original = "Laser BAB2110006 785 5 1131 30 1";

/* My first test for the DS1994 coin cell I have
 * It reads all pages and verifies that they are as expected
 */
void
ow_coin_diag_A ( void )
{
	char buf[32];
	char ref[32];
	int val;
	int i;
	int pg;

	// printf ( "Len = %d\n", strlen(ds_original) );

	ow_laser_read ();

	ow_read_page ( 0, buf );
	val = verify ( buf, ds_original, 32 );
	if ( val == 0 ) {
		printf ( "Trouble\n" );
		show_pg ( buf, ds_original );
	} else
		printf ( "OK\n" );

	for ( i=0; i<32; i++ )
		ref[i] = 0x55;

	// only needed once to fix a prior experiment
	// ow_write_page ( 1, ref );

	for ( pg=1; pg<16; pg++ ) {
		ow_read_page ( pg, buf );
		val = verify ( buf, ref, 32 );
		if ( val == 0 ) {
			printf ( "pg %d Trouble\n", pg );
			show_pg ( buf, ref );
		} else
			printf ( "pg %d OK\n", pg );
	}
}

static void
check_page ( int pg, char *ref, char *msg )
{
	char buf[32];
	int val;

	ow_read_page ( pg, buf );

	val = verify ( buf, ref, 32 );
	if ( val == 0 ) {
		printf ( "pg %d Trouble %s\n", pg, msg );
		show_pg ( buf, ref );
	} else
		printf ( "pg %d OK %s\n", pg, msg );
}

static void
ow_test_page ( int pg )
{
	char orig[32];
	char buf[32];
	int i;

	printf ( "Test page %d\n", pg );
	ow_read_page ( pg, orig );	// save

	memset ( buf, 0xaa, 32 );
	ow_write_page ( pg, buf );
	check_page ( pg, buf, "aa" );

	memset ( buf, 0x55, 32 );
	ow_write_page ( pg, buf );
	check_page ( pg, buf, "55" );

	ow_write_page ( pg, orig );	// restore
	check_page ( pg, orig, "orig" );
}

/* My second diagnostic.  For each page, read and
 *  save the original contents, then try writing
 *  two patterns, then restore.
 */
void
ow_coin_diag_B ( void )
{
	int pg;

	for ( pg=0; pg<16; pg++ )
		ow_test_page ( pg );
}

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

static void
ow_test ( void )
{
	int val;

	val = ow_reset ();
	if ( val )
		printf ( "Dallas reset returns: %d (BAD)\n", val );
	else
		printf ( "Dallas reset returns: %d (good)\n", val );

	// ow_temp_read ();
	// ow_rom_read ();

	// ow_read_all ();
	// ow_laser_read ();
	// ow_sp_read ();
	// ow_sp_test ();

	// ow_test2 (); -- does write
	// ow_read_all ();

	ow_coin_diag_B ();
	ow_coin_diag_A ();
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
