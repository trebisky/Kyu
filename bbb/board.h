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
 * board.h for the BBB
 *
 *  Kyu project  1-6-2017  Tom Trebisky
 *
 */

#define BOARD_BBB
#define ARCH_ARM

/* The am3359 defines 128 interrupts.
 *
 * Interrupts are chapter 6 in the TRM, table on pages 212-215
 */
#define NUM_INTS	128

#define NUM_CORES       1

/* Allow 16K of stack per mode */
/* These things are used in locore.S */
#define MODE_STACK_SIZE         32*1024
#define NUM_MODES               6

#define STACK_PER_CORE    MODE_STACK_SIZE * NUM_MODES

/* The BBB has 1/2 G of RAM always
 */
#define BOARD_RAM_SIZE	0x20000000
#define BOARD_RAM_MAX	0x20000000
#define BOARD_RAM_START	0x80000000

// #define BOARD_RAM_ENDP	(BOARD_RAM_START + BOARD_RAM_SIZE)
// #define BOARD_RAM_END	(BOARD_RAM_ENDP - 1)
// #define BOARD_RAM_END	0x9FFFFFFF

// #define THR_STACK_BASE	0x98000000
#define THR_STACK_LIMIT	4096 * 128

/*
#define EVIL_STACK_BASE 0x9a000000
*/

// #define MALLOC_BASE	0x90000000
#define MALLOC_SIZE	4 * 1024 * 1024


#define CONSOLE_BAUD		115200

#define INITIAL_CONSOLE		SERIAL

/* We add the 2 bytes on the ARM to get 4 byte alignment
 * of the IP addresses in the IP header.
 */
// #define NETBUF_PREPAD	2
#define NETBUF_PREPAD	0

/* THE END */
#endif /* _BOARD_H */
