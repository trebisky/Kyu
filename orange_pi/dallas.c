/*
 * Copyright (C) 2021  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* dallas.c
 *
 * Tom Trebisky  Kyu project  8-14-2021
 *
 * This is a driver for the old "Dallas Semiconductor" one wire protocol.
 * Dallas was bought by Maxim in 2002, so this is a bit of an anachronism,
 *  but what the heck.
 */

#include "kyu.h"
#include "gpio.h"

/* The way this is currently handled, we can only have one "bus" with
 * one wire devices hanging on it, which doesn't sound like a real
 * limitation frankly.
 */
static int dallas_pin;

void
dallas_init ( int gpio_pin )
{
	dallas_pin = gpio_pin;
}

int
dallas_reset ( void )
{
	int i;
	int bit;

	gpio_out_init ( dallas_pin );
	gpio_clear_bit ( dallas_pin );

	/* Must keep low for 480 us minimum */
	delay_us ( 500 );

	gpio_set_bit ( dallas_pin );
	gpio_in_init ( dallas_pin );

	/* Wait up to 100 us for line to go low */
	for ( i=0; i<20; i++ ) {
	    delay_us ( 5 );
	    bit = gpio_read_bit ( dallas_pin );
	    if ( bit == 0 )
		break;
	}

	/* never went low */
	if ( bit != 0 )
	    return 0;

	/* Wait up to 300 us for line to go high */
	for ( i=0; i<60; i++ ) {
	    delay_us ( 5 );
	    bit = gpio_read_bit ( dallas_pin );
	    if ( bit == 1 )
		break;
	}

	/* never went high */
	if ( bit != 1 )
	    return 0;

	/* good ! */
	return 1;
}

/* XXX */
void
dallas_poke ( void )
{
}

static int levels[100];

/* With my generic DS18B20, I see the following
 * using 5 us delays.
 * 111110000000000000000000000011111111 .....
 * So, I wait 5*5 = 25 us for the presense pulse to start.
 * It lasts for 23*5 = 115 us
 */

void
dallas_reset_1 ( void )
{
	int i;
	int x;

	gpio_out_init ( dallas_pin );
	gpio_clear_bit ( dallas_pin );

	/* Must keep low for 480 us minimum */
	delay_us ( 500 );

	gpio_set_bit ( dallas_pin );
	gpio_in_init ( dallas_pin );

	/* Expect to see "presence" pulse after 15-60 us
	 * may last from 60 to 240 us
	 */
	for ( i=0; i<100; i++ ) {
	    delay_us ( 5 );
	    levels[i] = gpio_read_bit ( dallas_pin );
	}

	for ( i=0; i<100; i++ ) {
	    if ( levels[i] )
		printf ( "1" );
	    else
		printf ( "0" );
	}
	printf ( "\n" );
}

/* Set up scope loop.
 * My 500 us reset pulse looks just right.
 * Then the line goes high for 32 us (15 to 60 is the spec).
 * Then the line goes low for 150 us (60 to 240 is the spec).
 */
void
dallas_reset_2 ( void )
{
	for ( ;; ) {
	    gpio_out_init ( dallas_pin );
	    gpio_clear_bit ( dallas_pin );

	    /* Must keep low for 480 us minimum */
	    delay_us ( 500 );

	    gpio_set_bit ( dallas_pin );
	    gpio_in_init ( dallas_pin );

	    delay_us ( 1500 );
	}
}

/* Our delay_us() doesn't seem right, so lets set up
 * a scope waveform and see what is up.
 * Actually pretty close.  I see 12.5 us rather than 10.0.
 * I used this to hand tweak the clock delay stuff in board.c
 * I did this for the NanoPi Neo, which runs at 408 Mhz.
 *
 *  -- see board.c, before I accounted for the 3 us overhead,
 *     I got the following.
 *     So I can never get less that a 3 us delay!
 * When I ask for 0 us delay I get 3.1 us
 * When I ask for 2 us delay I get 4.6 us
 * When I ask for 5 us delay I get 7.4 us
 * When I ask for 10 us delay, I get 12 us
 *
 *  -- after accounting for the 3 us overhead, I get:
 * When I ask for 5 us delay, I get 5.1
 * When I ask for 10 us delay, I get 10.7 us
 */
static void
clock_calib ( void )
{
	// int d = 2;
	int d = 5;

	gpio_out_init ( dallas_pin );

	for ( ;; ) {
	    gpio_set_bit ( dallas_pin );
	    delay_us ( d );
	    gpio_clear_bit ( dallas_pin );
	    delay_us ( d );
	}
}

void
dallas_test ( void )
{
	int x;

	dallas_init ( GPIO_A_0 );
	// clock_calib ();
	// dallas_reset_2 ();
	// dallas_reset_1 ();
	x = dallas_reset ();
	printf ( "Dallas reset gave: %d\n", x );
}

/* THE END */
