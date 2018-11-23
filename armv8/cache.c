/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * cache.c for Kyu for the ARM v8 processor
 *
 * Tom Trebisky  5/30/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

/* CCSIDR */
#define CCSIDR_LINE_SIZE_OFFSET         0
#define CCSIDR_LINE_SIZE_MASK           0x7
// #define CCSIDR_ASSOCIATIVITY_OFFSET     3
// #define CCSIDR_ASSOCIATIVITY_MASK       (0x3FF << 3)
// #define CCSIDR_NUM_SETS_OFFSET          13
// #define CCSIDR_NUM_SETS_MASK            (0x7FFF << 13)

static int cache_line_size;

void
cache_init ( void )
{
	unsigned int val;
	int line_len;

	val = 0;
        asm volatile ("msr csselr_el1, %0" : "=r" (val) );
        /* Read current CP15 Cache Size ID Register */
        // asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
        asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

        // line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;
        line_len = (val & CCSIDR_LINE_SIZE_MASK) + 4;

	/* Convert to bytes */
        // cache_line_size = 1 << (line_len + 2 );
        cache_line_size = 1 << line_len;

	printf ( "ARM cache line size: %d\n", cache_line_size );
}

int
get_cache_line_size ( void )
{
	return cache_line_size;
}

/* Called from IO test menu */
void
arch_cache_test ( void )
{
	printf ( "Not yet implemented for armv8\n" );
}

/* THE END */
