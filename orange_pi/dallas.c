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

void
dallas_test ( void )
{
	dallas_init ( GPIO_A_0 );
}

/* THE END */
