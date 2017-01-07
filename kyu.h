/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * kyu.h
 *
 * Parameters and definitions specific to Kyu
 *
 *	Tom Trebisky  11/25/2001
 *
 */

#define MAX_THREADS	32
#define MAX_SEM		64
#define MAX_CV		32

/* Set nonzero only when bringing the system up
 * from scratch and initial debug is needed.
 * Otherwise control debug level from test menu.
 */
#define DEBUG_THREADS	0

#define STACK_SIZE	4096	/* bytes */

/*
#define WANT_DELAY
#define WANT_PCI
#define WANT_NET
#define WANT_SLAB
#define WANT_BENCH
#define WANT_USER
*/
#define WANT_NET
#define WANT_USER
#define WANT_SHELL

/* XXX most of the following should migrate to board.h */

#include "board/board.h"

/* define this even if not using serial console.
#define CONSOLE_BAUD		38400
 */
#define CONSOLE_BAUD		115200

/*
#define INITIAL_CONSOLE		VGA
#define INITIAL_CONSOLE		SIO_0
*/
#define INITIAL_CONSOLE		SERIAL

/* This lets udelay work right */
#define DEFAULT_TIMER_RATE	1000

/* XXX there should be a better place for all this ..
 * maybe types.h ?
 */
#ifndef NULL
#define NULL (0)
#endif

typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef volatile unsigned long vu32;

/* THE END */
