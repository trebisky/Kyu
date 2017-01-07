/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * h3_ints.h
 *  Kyu project  1-6-2017  Tom Trebisky
 *
 */

/* List of interrupt numbers for the Allwinner H3
 */

/* The H3 defines 157 interrupts (0-156)
 * We treat this as 160, more or less
 */
#define IRQ_UART0	32
#define IRQ_TIMER0	50
#define IRQ_ETHER	114

/* THE END */
