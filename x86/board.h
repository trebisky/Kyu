#ifndef _BOARD_H
#define _BOARD_H

/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.h for the x86
 *
 * T. Trebisky  5/26/2015
 */

#define THR_STACK_BASE	0x70000
#define THR_STACK_LIMIT	4096 * 32 /* (0x20000) */

/* define this even if not using serial console. */
#define CONSOLE_BAUD		38400

/*
#define INITIAL_CONSOLE		VGA
#define INITIAL_CONSOLE		SIO_0
*/
#define INITIAL_CONSOLE		SERIAL

#define DEFAULT_TIMER_RATE	1000

/* The Tx descriptor for the ee100 driver requires
 * 16 bytes, keeping a hole at least that big at the start
 * of the buffer allows us to just put it into the netbuf.
 *
 * (This isn't necessary for other drivers, but doesn't
 * hurt anything and may let the ee100 driver do clever things.)
 *
 * If we add other drivers we want to be clever with, we may
 * need to make this the max of the requirements.
 */
#define NETBUF_PREPAD	(8 * sizeof(long))

/* THE END */
#endif /* _BOARD_H */
