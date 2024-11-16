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
 * board.h for the Zynq
 *
 *  Kyu project  11-14-2024 Tom Trebisky
 *
 */

#define BOARD_ZYNQ

// #define WANT_NET

/* The Ebaz and Antminer S9 have the console on Uart-1
 * The Zedboard may have it on Uart-0
 */
#define CONSOLE_BAUD		115200
#define CONSOLE_UART		1

#define ARCH_ARM
#define ARCH_ARM32

/* The Zynq TRM has a nice chapter on interrupts (Ch-7)
 * pages 230-231 has the all important table showing
 * the various SPI interrupts.
 */
#define NUM_INTS	96

#define NUM_CORES	2

/* Allow 16K of stack per mode */
/* These things are used in locore.S */
#define MODE_STACK_SIZE         16*1024
#define NUM_MODES               6

#define STACK_PER_CORE    MODE_STACK_SIZE * NUM_MODES

/* Ebaz boards have 256M
 * Most Antminer S9 boards have 512M
 * I have one Antminer S9 board with 1024M (1G)
 */
// #define BOARD_RAM_SIZE	0x20000000	/* 512M */
#define BOARD_RAM_SIZE	0	/* Force probing */

#define BOARD_RAM_MAX	0x40000000	/* 1G */

/* Memory on the Zynq starts at 0 */
#define BOARD_RAM_START	0x0

#define THR_STACK_LIMIT	4096 * 128
#define MALLOC_SIZE	4 * 1024 * 1024

/* We add the 2 bytes on the ARM to get 4 byte alignment
 * of the IP addresses in the IP header.
 */
// #define NETBUF_PREPAD	2
#define NETBUF_PREPAD	0

/* THE END */
#endif /* _BOARD_H */
