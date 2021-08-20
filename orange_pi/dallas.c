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

/* Someday the following will go into gpio.h perhaps
 */
#define GPIO_PIN(x)	(x%32)
#define GPIO_MASK(p)	(1<<p)

void
dallas_read_pulse ( void )
{
	volatile unsigned int *dp;
	int p, m;
	unsigned int val_on, val_off;

	gpio_out_init ( dallas_pin );

	p = GPIO_PIN ( dallas_pin );
	m = GPIO_MASK ( p );

	dp = gpio_get_reg ( dallas_pin );
	val_on = *dp | m;
	val_off = *dp & ~m;

	/* generate the down pulse 1 us */
	/* The double write stretches it from 500 ns to 1 us */
	*dp = val_off;
	*dp = val_off;
	*dp = val_on;
}

void
clock_calib ( void )
{
	for ( ;; ) {
	    dallas_read_pulse ();
	    delay_us ( 2 );
	}
}

void
dallas_test ( void )
{
	int x;

	dallas_init ( GPIO_A_0 );

	// clock_calib ();

	x = dallas_reset ();
	printf ( "Dallas reset gave: %d\n", x );
}

/* ==================================================================================== */
/* ==================================================================================== */
/* ==================================================================================== */
/* ==================================================================================== */
/* experiments below here */

#ifdef notdef

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
 *
 * When I comment out the delay entirely, I get a 2.1 us pulse!
 * It is as though each function call takes 1.0 us, which hardly
 * seems right for a processor running at 408 Mhz.
 */
static void
clock_calib_ORIG1 ( void )
{
	// int d = 2;
	int d = 5;

	gpio_out_init ( dallas_pin );

	for ( ;; ) {
	    gpio_set_bit ( dallas_pin );
	    // delay_us ( d );
	    asm ( "nop" );
	    gpio_clear_bit ( dallas_pin );
	    delay_us ( d );
	}
}

/* Someday the following will go into gpio.h perhaps
 */
#define GPIO_PIN(x)	(x%32)
// #define GPIO_BASE(x)	gpio_get_base ( x )
#define GPIO_MASK(p)	(1<<p)
#define GPIO_SET(b,m)	b->data |= m;
#define GPIO_CLEAR(b,m)	b->data &= ~m;

static void
clock_calib_ORIG2 ( void )
{
	// int d = 2;
	int d = 5;
	int p, m;
	volatile unsigned int *dp;
	unsigned int val_on, val_off;

	// struct h3_gpio *gp;
	//gp = GPIO_BASE ( dallas_pin );

	gpio_out_init ( dallas_pin );
	p = GPIO_PIN ( dallas_pin );
	m = GPIO_MASK ( p );

	dp = gpio_get_reg ( dallas_pin );
	val_on = *dp | m;
	val_off = *dp & ~m;

#ifdef notthis
	for ( ;; ) {
	    // gpio_set_bit ( dallas_pin );
	    GPIO_SET ( gp, m );
	    // delay_us ( d );
	    // gpio_clear_bit ( dallas_pin );
	    GPIO_CLEAR ( gp, m );
	    delay_us ( d );
	}
#endif

	/* This gives us endless 500 ns pulses... almost.
	 * I see 8 nice pulses, then the low time gets
	 * stretched to about 700 ns, then we get 8 more.
	 * This pattern repeats with these 8 pulse groups
	 * being a definite pattern.
	 */
	for ( ;; ) {
	    *dp = val_on;
	    *dp = val_off;
	}

	/* Just for the record, the above loop yields the following
	 * compiled without optimization.
	 */

#ifdef notthis
400064b0:       e51b3018        ldr     r3, [fp, #-24]  ; 0xffffffe8
400064b4:       e51b201c        ldr     r2, [fp, #-28]  ; 0xffffffe4
400064b8:       e5832000        str     r2, [r3]
400064bc:       e51b3018        ldr     r3, [fp, #-24]  ; 0xffffffe8
400064c0:       e51b2020        ldr     r2, [fp, #-32]  ; 0xffffffe0
400064c4:       e5832000        str     r2, [r3]
400064c8:       eafffff8        b       400064b0 <clock_calib+0xa8>
#endif

	/* If we compile this code with optimization (using -Os)
	 * this entire routine gets inlined into dallas_test() and
	 * the loop itself becomes the instructions below.
	 * The waveform is surprising.
	 * It has a 100 ns period,
	 * so each pulse (2 instructions)is 50ns,
	 * i.e. each instruction seems to run in 25 ns,
	 * which would be a 40 Mhz clock.
	 * The waveform is an ugly thing with lots of ringing,
	 * not a square wave at all.
	 */

#ifdef notthis
4000625c:       e5801010        str     r1, [r0, #16]
40006260:       e5803010        str     r3, [r0, #16]
40006264:       eafffffc        b       4000625c <dallas_test+0x3c>
#endif


#ifdef notthis
	/* Without nops, this gives a 500 ns pulse.
	 * There are 3 instructions between high and low,
	 * so that would make you think the CPU runs an
	 * instruction every 500/3 ns, i.e. 6 instructions per us
	 * or is running at 6 Mhz.
	 *
	 * Adding 3 nops yields no discernable change
	 * Adding 500 nops gives a 1650 ns pulse.
	 * So the 500 nops add 1150 ns to the timing.
	 * This is about 2.3 ns per nop, which would indicate a
	 * 434 Mhz clock rate, which is believably close to 408.
	 */
	for ( ;; ) {
	    *dp = val_on;
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );
	    asm ( "nop" );

	    *dp = val_off;
	    delay_us ( d );
	}
#endif

}

#endif

/* THE END */
