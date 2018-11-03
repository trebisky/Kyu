/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* gpio.c
 *
 * Simple driver for the GPIO
 * 
 */

#define GPIO0_BASE      ( (struct gpio *) 0x44E07000 )
#define GPIO1_BASE      ( (struct gpio *) 0x4804C000 )
#define GPIO2_BASE      ( (struct gpio *) 0x481AC000 )
#define GPIO3_BASE      ( (struct gpio *) 0x481AE000 )

#include <kyulib.h>
#include <gpio.h>
#include <omap_mux.h>
#include <arch/cpu.h>

/* Lookup table to map gpio number to mux index.
 */
static int gpio_to_mux[] = {
	MUX_GPIO_0_0, MUX_GPIO_0_1, MUX_GPIO_0_2, MUX_GPIO_0_3,
	MUX_GPIO_0_4, MUX_GPIO_0_5, MUX_GPIO_0_6, MUX_GPIO_0_7,
	MUX_GPIO_0_8, MUX_GPIO_0_9, MUX_GPIO_0_10, MUX_GPIO_0_11,
	MUX_GPIO_0_12, MUX_GPIO_0_13, MUX_GPIO_0_14, MUX_GPIO_0_15,
	MUX_GPIO_0_16, MUX_GPIO_0_17, MUX_GPIO_0_18, MUX_GPIO_0_19,
	MUX_GPIO_0_20, MUX_GPIO_0_21, MUX_GPIO_0_22, MUX_GPIO_0_23,
	-1, -1, MUX_GPIO_0_26, MUX_GPIO_0_27,
	MUX_GPIO_0_28, MUX_GPIO_0_29, MUX_GPIO_0_30, MUX_GPIO_0_31,
	MUX_GPIO_1_0, MUX_GPIO_1_1, MUX_GPIO_1_2, MUX_GPIO_1_3,
	MUX_GPIO_1_4, MUX_GPIO_1_5, MUX_GPIO_1_6, MUX_GPIO_1_7,
	MUX_GPIO_1_8, MUX_GPIO_1_9, MUX_GPIO_1_10, MUX_GPIO_1_11,
	MUX_GPIO_1_12, MUX_GPIO_1_13, MUX_GPIO_1_14, MUX_GPIO_1_15,
	MUX_GPIO_1_16, MUX_GPIO_1_17, MUX_GPIO_1_18, MUX_GPIO_1_19,
	MUX_GPIO_1_20, MUX_GPIO_1_21, MUX_GPIO_1_22, MUX_GPIO_1_23,
	MUX_GPIO_1_24, MUX_GPIO_1_25, MUX_GPIO_1_26, MUX_GPIO_1_27,
	MUX_GPIO_1_28, MUX_GPIO_1_29, MUX_GPIO_1_30, MUX_GPIO_1_31,
	MUX_GPIO_2_0, MUX_GPIO_2_1, MUX_GPIO_2_2, MUX_GPIO_2_3,
	MUX_GPIO_2_4, MUX_GPIO_2_5, MUX_GPIO_2_6, MUX_GPIO_2_7,
	MUX_GPIO_2_8, MUX_GPIO_2_9, MUX_GPIO_2_10, MUX_GPIO_2_11,
	MUX_GPIO_2_12, MUX_GPIO_2_13, MUX_GPIO_2_14, MUX_GPIO_2_15,
	MUX_GPIO_2_16, MUX_GPIO_2_17, MUX_GPIO_2_18, MUX_GPIO_2_19,
	MUX_GPIO_2_20, MUX_GPIO_2_21, MUX_GPIO_2_22, MUX_GPIO_2_23,
	MUX_GPIO_2_24, MUX_GPIO_2_25, MUX_GPIO_2_26, MUX_GPIO_2_27,
	MUX_GPIO_2_28, MUX_GPIO_2_29, MUX_GPIO_2_30, MUX_GPIO_2_31,
	MUX_GPIO_3_0, MUX_GPIO_3_1, MUX_GPIO_3_2, MUX_GPIO_3_3,
	MUX_GPIO_3_4, MUX_GPIO_3_5, MUX_GPIO_3_6, MUX_GPIO_3_7,
	MUX_GPIO_3_8, MUX_GPIO_3_9, MUX_GPIO_3_10, -1,
	-1, MUX_GPIO_3_13, MUX_GPIO_3_14, MUX_GPIO_3_15,
	MUX_GPIO_3_16, MUX_GPIO_3_17, MUX_GPIO_3_18, MUX_GPIO_3_19,
	MUX_GPIO_3_20, MUX_GPIO_3_21, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1
};

/* Each GPIO has registers like this:
 */
struct gpio {
	volatile unsigned long id;
	long _pad0[3];
	volatile unsigned long config;
	long _pad1[3];
	volatile unsigned long eoi;

	volatile unsigned long irqs0r;
	volatile unsigned long irqs1r;
	volatile unsigned long irqs0;
	volatile unsigned long irqs1;

	volatile unsigned long irqset0;
	volatile unsigned long irqset1;
	volatile unsigned long irqclear0;
	volatile unsigned long irqclear1;

	volatile unsigned long waken0;
	volatile unsigned long waken1;
	long _pad2[50];
	volatile unsigned long status;
	long _pad3[6];
	volatile unsigned long control;
	volatile unsigned long oe;
	volatile unsigned long datain;
	volatile unsigned long dataout;
	volatile unsigned long level0;
	volatile unsigned long level1;
	volatile unsigned long rising;
	volatile unsigned long falling;
	volatile unsigned long deb_ena;
	volatile unsigned long deb_time;
	long _pad4[14];
	volatile unsigned long clear_data;
	volatile unsigned long set_data;
};

static struct gpio *gpio_bases[] = {
    GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE
};

/* once you know that the user LED's are all in GPIO1
 * these bit definitions are what you want.
 */

#define LED0	1<<21
#define LED1	1<<22
#define LED2	1<<23
#define LED3	1<<24

#define ALL_LED (LED0 | LED1 | LED2 | LED3)

static int led_bits[] = { LED0, LED1, LED2, LED3 };

/* If you don't know jack, you can use these definitions
 * along with my header file and the gpio routines.
 * This is actually the better way.
 */
static int users[] = { USER_LED0, USER_LED1, USER_LED2, USER_LED3 };

static void
gpio_led_init ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	setup_led_mux ();

	/* Setting the direction is essential */
	p->oe &= ~ALL_LED;
	/*
	printf ( "gpio1 OE = %08x\n", p->oe );
	*/
	p->clear_data = ALL_LED;
}

void
gpio_dir_out ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->oe &= ~(1 << (bit % 32));
}

void
gpio_dir_in ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->oe |= (1 << (bit % 32));
}

void
gpio_set_bit ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->set_data = 1 << (bit % 32);
}

void
gpio_clear_bit ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->clear_data = 1 << (bit % 32);
}

int
gpio_read_bit ( int bit )
{
	int gpio = bit / 32;
	// return gpio_bases[gpio]->datain;
	return gpio_bases[gpio]->datain & (1 << (bit % 32));
}

/* For debugging */
int
gpio_read ( int gpio )
{
	return gpio_bases[gpio]->datain;
}

/* Call this before trying to set or clear a gpio */
void
gpio_out_init ( int bit )
{
	setup_gpio_out ( gpio_to_mux[bit] );
	gpio_dir_out ( bit );
}

/* Call this before trying to read from a bit.
 * This enables neither pullup or pulldown
 *   (see below for that)
 */
void
gpio_in_init ( int bit )
{
	setup_gpio_in ( gpio_to_mux[bit] );
	gpio_dir_in ( bit );
}

/* As above, but enable pullup */
void
gpio_in_up ( int bit )
{
	setup_gpio_in_up ( gpio_to_mux[bit] );
	gpio_dir_in ( bit );
}

/* As above, but enable pulldown */
void
gpio_in_down ( int bit )
{
	setup_gpio_in_down ( gpio_to_mux[bit] );
	gpio_dir_in ( bit );
}

void
gpio_iic_init ( int bit )
{
	setup_iic_mux ( gpio_to_mux[bit] );
	gpio_dir_out ( bit );
}

/* --------------------- */

/* This is only for the BBB, supports a silly test */
/* The BBB has 4 LED, hence the loop */
void
gpio_led_set ( int val )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;

	for ( i=0; i<4; i++ ) {
	    if ( val & 0x01 )
		p->set_data = led_bits[i];
	    else
		p->clear_data = led_bits[i];
		val >>= 1;
	}
}

// Called from hardware.c
void
gpio_init ( void )
{
	gpio_led_init ();
}

/* Test 0 --
 * Cute light show.
 * Works fine 5-27-2016
 */

static int gpio_test_count = 0;

static void
gpio_test_init ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	p->clear_data = ALL_LED;

	p->set_data = led_bits[0];
}

void
gpio_test_run ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	p->clear_data = led_bits[gpio_test_count];
	gpio_test_count = (gpio_test_count+1) % 4;
	p->set_data = led_bits[gpio_test_count];
}

#define DELAY0	40

// XXX - runs forever
void
gpio_test0 ( void )
{
	gpio_test_init ();

	for ( ;; ) {
	    thr_delay ( DELAY0 );
	    gpio_test_run ();
	}
}

/* Test 1
 * Cute light show
 * works, slower and less cool than the above.
 * uses uncalibrated delay loop.
 */
void
gpio_test1 ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;
	int n;

	/*
	struct gpio *p = (struct gpio *) 0;
	printf ( "waken1 at %08x\n", &p->waken1 );
	printf ( "status at %08x\n", &p->status );
	printf ( "set_data at %08x\n", &p->set_data );
	*/

	p->clear_data = ALL_LED;
	i=0;
	p->set_data = led_bits[i];
	n = 1;

	for ( ;; ) {
	    //if ( i == 0 )
		//printf ( "GPIO test, pass %d\n", n++ );
	    p->clear_data = led_bits[i];
	    i = (i+1) % 4;
	    p->set_data = led_bits[i];
	    // delay10 ();
	    delay_ms ( 100 );
	}
}

/* Test 2 --
 * Cute light show.
 *  use timer interrupts
 *  works 5-27-2016
 */

#define DELAY2	40

void
gpio_test2 ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;
	int n;

	/*
	struct gpio *p = (struct gpio *) 0;
	printf ( "waken1 at %08x\n", &p->waken1 );
	printf ( "status at %08x\n", &p->status );
	printf ( "set_data at %08x\n", &p->set_data );
	*/

	p->clear_data = ALL_LED;
	i=0;
	p->set_data = led_bits[i];
	n = 1;

	for ( ;; ) {
	    // if ( i == 0 )
		// printf ( "GPIO test, pass %d\n", n++ );
	    p->clear_data = led_bits[i];
	    i = (i+1) % 4;
	    p->set_data = led_bits[i];
	    thr_delay ( DELAY2 );
	}
}

/* Test 3 - 
 * Cute light blinking
 *  use gpio routines and timer interrupts
 */
void
gpio_test3 ( void )
{
	int who = 0;

	for ( ;; ) {
	    gpio_clear_bit ( users[who] );
	    who = (who+1) % 4;
	    gpio_set_bit ( users[who] );
	    thr_delay ( 37 );
	}
}

/* Test 4 -
 *  first time turn all off.
 *  next time turn all on.
 *  toggle state like this on each call.
 * OK - 5-27-2016
 */
static int flip = 1;

void
gpio_test4 ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	if ( flip ) {
	    p->clear_data = ALL_LED;
	    flip = 0;
	} else {
	    p->set_data = ALL_LED;
	    flip = 1;
	}
}

/* For this to work, you must wire up an LED to
 * P8 pin 7.  I use a 470 ohm series resistor
 * and connect to ground on P9 pin 1
 */

#define HALF_SEC	500

#define TEST5_BIT	GPIO_2_2

void
gpio_test5 ( void )
{
	gpio_out_init ( TEST5_BIT );

	// printf ( "Ticking\n" );

	for ( ;; ) {
	    delay_ms ( HALF_SEC );
	    gpio_clear_bit ( TEST5_BIT );
	    delay_ms ( HALF_SEC );
	    gpio_set_bit ( TEST5_BIT );
	    // printf ( "Tick\n" );
	}
}

#define TEST6_BIT	GPIO_2_2

/* Used to test a single "pushbutton" input.
 */
void
gpio_test6 ( void )
{
	int val;

	gpio_in_init ( TEST6_BIT );
	printf ( "bit: %d\n", TEST6_BIT );

	for ( ;; ) {
	    delay_ms ( HALF_SEC );
	    val = gpio_read_bit ( TEST6_BIT );
	    printf ( "Read: %08x\n", val );
	}
}

/* Used with a 555 sending a square wave input
 * With a particular set of components, I get:
 * 5-29-2016
High: 727
Low : 463
High: 726
Low : 463
*/
#define TEST7_BIT	GPIO_2_2

#define MILLI_SEC	1000
void
gpio_test7 ( void )
{
	int val;
	int nval;
	int count;

	gpio_in_init ( TEST7_BIT );
	val = gpio_read_bit ( TEST7_BIT );
	count = 0;

	for ( ;; ) {
	    count++;
	    delay_us ( MILLI_SEC );
	    nval = gpio_read_bit ( TEST7_BIT );
	    if ( val != nval ) {
		if ( val )
		    printf ( "High: %d\n", count );
		else
		    printf ( "Low : %d\n", count );
		count = 0;
		val = nval;
	    }
	}
}

#ifdef TIMER_DEBUG
/* A goofy place for this ... */
#define TEST8_BIT	GPIO_2_2
void
gpio_test8 ( void )
{
	unsigned long t1, t2;

	thr_delay ( 2 );
	reset_ccnt ();
	xxx_arm ();

	t1 = r_CCNT ();
	thr_delay ( 5 );
	t2 = r_CCNT ();

	printf ( "DELAY done: %d %d %d\n", t1, t2, t2-t1 );
	show_xxx ();

}
#endif

/* Generate pulses to test my Rigol DS1054Z oscilloscope.
 * This is the test that exposed my even/odd timer bug
 *  in July of 2016
 */
#define TEST9_BIT	GPIO_2_2
void
gpio_test9 ( void )
{
	int n;
	gpio_out_init ( TEST9_BIT );

	for ( ;; ) {
	    for ( n=0; n<10; n++ ) {
		gpio_clear_bit ( TEST9_BIT );
		gpio_set_bit ( TEST9_BIT );
	    }
	    thr_delay ( 1 );
	}
}

void
gpio_test_ccnt ( void )
{
	unsigned long val;
	int i;

	// gpio_test3 ();

	/*
	unsigned long val2;
	val = get_pmcr ();
	printf ( "PMCR = %d %08x\n", val, val );

	reset_ccnt ();
	val = r_CCNT  ();
	printf ( "CCNT = %d %08x\n", val, val );
	thr_delay ( 2 );
	val2 = r_CCNT  ();
	printf ( "CCNT = %d %08x\n", val2, val2 );
	printf ( " elapsed = %d\n", val2 - val );
	*/

	for ( i=0; i < 5; i++ ) {
	    // delay_ns ( 2000000 - 50 );
	    delay_us ( 2000 );
	    val = r_CCNT  ();
	    printf ( "CCNT = %d %08x\n", val, val );
	}
}

/* Called from test menu in tests.c */
void
gpio_test ( void )
{
	// gpio_test7 ();
	// iic_test ();
	// i2c_test ();
	// gpio_test8 ();
	gpio_test9 ();
}

/* THE END */
