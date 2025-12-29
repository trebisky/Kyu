/*
 * Copyright (C) 2020  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* spi.c
 *
 * Hardware level spi driver for the Allwiner H3 on the Orange Pi boards.
 *
 * Tom Trebisky  Kyu project  7-28-2020
 *
 * This is just a place holder for the time being.
 */

#include "kyu.h"
#include "thread.h"
#include "interrupts.h"

/* See p. 107 in the datasheet for gating */
/* See p. 135 in the datasheet for reset control */

/* Only SPI0 is routed to the Orange Pi connector */
#define SPI0_BASE	0x01c68000
#define SPI1_BASE	0x01c69000

/* i2s belongs elsewhere, but for now ... */
/* i2s is all about audio stuff */

#define I2S0_BASE	0x01c22000
#define I2S1_BASE	0x01c22400
#define I2S2_BASE	0x01c22800

/* pwm belongs elsewhere, but for now ... */

#define PWM_BASE	0x01c21400

/* THE END */
