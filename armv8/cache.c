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

#define SEL_I_CACHE		0x001	/* zero is D cache */
#define SEL_L1_CACHE	0x000	/* bits 3:1 */
#define SEL_L2_CACHE	0x002	/* bits 3:1 */
#define SEL_L3_CACHE	0x004	/* bits 3:1 */

// tjt 1-2-2026 -- needs work
void
cache_init ( void )
{
	unsigned int val;
	int line_len;
	int asso;
	int sets;

	val = 0;
	// asm volatile ("msr csselr_el1, %0" : "=r" (val) ); WRONG
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	printf ( "cache CCSIDR for L1 D = %08x\n", val );

	/* line size gives log2(words)-2 */
	/* What is a word?  4 bytes apparently */
	// line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;
	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;

	/* Convert to bytes */
	// cache_line_size = 1 << (line_len + 2 );
	cache_line_size = 1 << (line_len + 2);

	/* Bits 12:3 are associativity - 1 */
	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( "ARM L1 D cache line size: %d\n", cache_line_size );
	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );

	/* ----------- */

	val = SEL_I_CACHE;;
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	printf ( "cache CCSIDR for L1 I = %08x\n", val );

	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;
	cache_line_size = 1 << (line_len + 2);

	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( "ARM L1 I cache line size: %d\n", cache_line_size );
	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );

	/* ----------- */

	val = SEL_L2_CACHE;;
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	printf ( "cache CCSIDR for L2 = %08x\n", val );

	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;
	cache_line_size = 1 << (line_len + 2);

	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( "ARM L2 cache line size: %d\n", cache_line_size );
	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );
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
