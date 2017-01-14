#ifndef _BOARD_H
#define _BOARD_H

/*
 * Copyright (C) 2017  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * board.h for the Orange Pi PC
 *
 *  Kyu project  1-6-2017  Tom Trebisky
 *
 */

#define BOARD_ORANGE_PI
#define BOARD_ORANGE_PI_PC
#define ARCH_ARM

#define NUM_INTS	157

#define THR_STACK_BASE	0x48000000
#define THR_STACK_LIMIT	4096 * 128

#define MALLOC_BASE	0x50000000
#define MALLOC_SIZE	4 * 1024 * 1024

/* XXX will need to be different for boards with
 * less than 1G of ram
 */
#define BOARD_RAM_START	0x40000000
#define BOARD_RAM_END	0x7FFFFFFF
#define BOARD_RAM_ENDP	0x80000000

#define CONSOLE_BAUD		115200

#define INITIAL_CONSOLE		SERIAL

#define DEFAULT_TIMER_RATE	1000

/* We add the 2 bytes on the ARM to get 4 byte alignment
 * of the IP addresses in the IP header.
 */
// #define NETBUF_PREPAD	2
#define NETBUF_PREPAD	0

/* THE END */
#endif /* _BOARD_H */
