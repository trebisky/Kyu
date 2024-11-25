/*
 * Copyright (C) 2024  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * led.c for the Zynq
 *
 * taken from my "mio_blink" bare metal demo
 *
 * This blinks the red and green LED in alternation on the Antminer S9.
 * Unlike the Ebaz, on the Antminer these LED are available via mio/gpio
 *  and don't require some FPGA setup to access them.
 *
 * Tom Trebisky  11/14/2024
 *
 */

#define MIO_RED     37
#define MIO_GREEN   38

/* We "discovered" this 11-24-2024 when making an inventory of
 * MIO pins and then looking at the Antminer schematic.
 * This is D2 on the board with R27 nearby.
 * It is an SMT diode near the Zynq on the side towards the
 * ethernet.
 */
#define MIO_EXTRA   15

// #define EXTRA 1

static void
mio_setup ( void )
{
    mio_init ();
    mio_gpio ( MIO_RED );
    mio_gpio ( MIO_GREEN );

// either way works.
//    mio_extra ( MIO_EXTRA );
    mio_gpio ( MIO_EXTRA );
}

static int state = 0;

/* runs ever 200 ms */
static void
blinker ( int xx )
{
	if ( state ) {
        // printf ( "Blink off\n" );
        gpio_write ( MIO_RED, 0 );
        gpio_write ( MIO_GREEN, 1 );
	} else {
        // printf ( "Blink on\n" );
        gpio_write ( MIO_RED, 1 );
        gpio_write ( MIO_GREEN, 0 );
	}

#ifdef EXTRA
	if ( state ) {
        gpio_write ( MIO_EXTRA, 0 );
	} else {
        gpio_write ( MIO_EXTRA, 1 );
	}
#endif

	state = (state + 1) % 2;
}

void
led_demo ( void )
{
    // apparently this is not needed
    // slcr_unlock ();

    mio_setup ();

    gpio_init ();
    gpio_config_output ( MIO_RED );
    gpio_config_output ( MIO_GREEN );
    gpio_config_output ( MIO_EXTRA );

	// Works, but nothing else runs
	// (a topic for investigation someday - XXX
	// thr_repeat_c ( 500, blinker, 0 );

	// This works just fine
	// (void) thr_new_repeat ( char *name, tfptr func, void *arg, int prio, int flags, int nticks )
	(void) thr_new_repeat ( "led", blinker, 0, 25, 0, 500 );
}

/* THE END */
