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

#include "protos.h"
#include "board/board.h"

// This is referenced in user.c when it launches the shell.
// #define PRI_SHELL	60
// this only works if we have a console uart
// that does input via interrupts
#define PRI_SHELL	11

#define MAX_THREADS	32
// #define MAX_SEM		64
#define MAX_SEM		512
#define MAX_CV		32

/* Set nonzero only when bringing the system up
 * from scratch and initial debug is needed.
 * Otherwise control debug level from test menu.
 */
#define DEBUG_THREADS	0

/* A 4K stack has been the norm since the early days
 * of Kyu.  In December of 2022 I tried some experiments
 * with a larger stack when I was getting hangs related to TCP
 */
// #define STACK_SIZE	4096	/* bytes */
#define STACK_SIZE	8192	/* bytes */
// #define STACK_SIZE	16384	/* bytes */

/* There will be trouble if we change this,
 * in particular, TCP timeouts expect the timer
 * to tick once per millisecond.
 */
#define DEFAULT_TIMER_RATE	1000

// We handle this in board/board.h now
// #define WANT_NET

#ifdef notdef
#define WANT_MMT_PADDLE
#define WANT_DELAY
#define WANT_PCI
#define WANT_SLAB
#define WANT_BENCH
#define WANT_USER
#define WANT_FLOAT
#define WANT_PUTS_LOCK		/* orange pi experimental */
#define WANT_SETJMP
#define WANT_DHRY
#endif

#define WANT_USER
#define WANT_SHELL

#define WANT_SMP

// #define WANT_TCP_XINU
#define WANT_TCP_BSD
// #define WANT_TCP_KYU

/* Xinu TCP needs the net timer */
#ifdef WANT_TCP_XINU
#define WANT_NET_TIMER
#endif

#ifndef WANT_TCP_XINU
#ifndef WANT_TCP_BSD
#define		WANT_TCP_KYU
#endif
#endif

#include "board/board.h"

/* XXX there should be a better place for all this ..
 * maybe types.h ?
 */

typedef void (*cfptr) ( int, void * );

#ifndef NULL
#define NULL (0)
#endif

#define __weak               __attribute__((weak))

/* THE END */
