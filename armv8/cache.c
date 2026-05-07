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
 * Tom Trebisky  5/4/2026
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "types.h"
#include "cpu.h"
#include "board/board.h"

static void cache_diag ( void );

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

static void
cache_stuff ( u32 val, char *who )
{
	printf ( "--------   cache CCSIDR for %s = %08x\n", who, val );
	if ( val & 0x80000000 )
		printf ( "Write through\n" );
	else
		printf ( "NO Write through\n" );

	if ( val & 0x40000000 )
		printf ( "Write back\n" );
	else
		printf ( "NO Write back\n" );

	if ( val & 0x20000000 )
		printf ( "Read allocate\n" );
	else
		printf ( "NO Read allocate\n" );

	if ( val & 0x10000000 )
		printf ( "Write allocate\n" );
	else
		printf ( "NO Write allocate\n" );
}

// tjt 1-2-2026 -- needs work
/* Called from board_hardware_init()
 */
void
cache_init ( void )
{
	u32 val;
	int line_len;
	int asso;
	int sets;

	cache_diag ();

	val = 0;
	// asm volatile ("msr csselr_el1, %0" : "=r" (val) ); WRONG
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	// cache_stuff ( val, "L1 D" );

	/* line size gives log2(words)-2 */
	/* What is a word?  4 bytes apparently */
	// line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;
	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;

	/* Convert to bytes */
	// cache_line_size = 1 << (line_len + 2 );
	cache_line_size = 1 << (line_len + 2);

#ifdef NOISE
	printf ( "ARM L1 D cache line size: %d\n", cache_line_size );
	/* Bits 12:3 are associativity - 1 */
	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );
#endif

	/* ----------- */

	val = SEL_I_CACHE;
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	// cache_stuff ( val, "L1 I" );

	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;
	cache_line_size = 1 << (line_len + 2);

#ifdef NOISE
	printf ( "ARM L1 I cache line size: %d\n", cache_line_size );
	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );
#endif

	/* ----------- */

	val = SEL_L2_CACHE;
	asm volatile ("msr csselr_el1, %0" : : "r" (val) );

	/* Read current CP15 Cache Size ID Register */
	// asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (val));
	asm volatile ("mrs %0, ccsidr_el1" : "=r" (val) );

	// cache_stuff ( val, "L2" );

	line_len = (val & CCSIDR_LINE_SIZE_MASK) + 2;
	cache_line_size = 1 << (line_len + 2);

#ifdef NOISE
	printf ( "ARM L2 cache line size: %d\n", cache_line_size );
	asso = ((val>>3) & 0x3ff) + 1;
	sets = ((val>>13) & 0x7fff) + 1;

	printf ( " associativity: %d\n", asso );
	printf ( " sets: %d\n", sets );
#endif
}

/* We set this above in 3 places, which do we really want?
 * On the Allwinner h5, this is 64 in all 3 places, so
 * it doesn't matter.
 */
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

static void
flush_line ( u64 addr )
{
		/* clean & invalidate data or unified cache */
		asm volatile("dc civac, %0" : : "r" (addr) );
}

/* Thus far, we have a "bug" and this is identical to
 * the flush above
 */
static void
invalidate_line ( u64 addr )
{
		/* invalidate data or unified cache */
		asm volatile("dc ivac, %0" : : "r" (addr) );
}

/* ------------------------------------------------------------------------------------*/
/* ------------------------------------------------------------------------------------*/

/* From U-Boot sources, just a tiny extract from:
 *  u-boot-2018.09/arch/arm/cpu/armv8/cache_v8.c
 */

/* Showing each line produces an outrageous amount of output
 */
static void
flusher ( addr_t start, addr_t stop )
{
		u64 val;
		int line;
		u64 add;

		printf ( "FLUSH ------------------- %08x %08x\n", start, stop );
		asm volatile ("mrs %0, CTR_EL0": "=r" (val) );
		printf ( "CTR = %016x\n", val );
		line = (val >> 16) & 0xf;
		line = 4 << line;
		printf ( "cache line length = %d\n", line );
		add &= ~(line - 1);
		while ( add < stop ) {
			printf ( "Line: %08x\n", add );
			add += line;
		}
}

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void
flush_dcache_range ( addr_t start, addr_t stop )
{
		// printf ( "FLUSH ------------------- %08x %08x\n", start, stop );
		// flusher ( start, stop );
        __asm_flush_dcache_range ( start, stop );
}

/*
 * Invalidates range in all levels of D-cache/unified cache
 */
void
invalidate_dcache_range ( addr_t start, addr_t stop )
{
		// printf ( "INVALIDATE ------------------- %08x %08x\n", start, stop );
        __asm_invalidate_dcache_range(start, stop);
}

/* Works great */
void
dcache_disable ( void )
{
		u64 val;

		// printf ( "Disable D cache\n" );
		__asm_flush_dcache_all();

		get_SCTLR (val);
		val &= ~0x4;
		set_SCTLR (val);

		__asm_flush_dcache_all();
		__asm_invalidate_tlb_all();
}

void
dcache_enable ( void )
{
		u64 val;

		get_SCTLR (val);
		val |= 0x4;
		set_SCTLR (val);
}

static inline void
icache_invalidate ( void )
{
	asm volatile ("ic ialluis\n");
	asm volatile ("isb\n");
}

void
icache_enable ( void )
{
	u64 val;

	// printf ( "Enable I cache\n" );
	get_SCTLR (val);
	val |= (1<<12);
	set_SCTLR (val);
	icache_invalidate ();
}

void
icache_disable ( void )
{
	u64 val;

	// printf ( "Disable I cache\n" );
	get_SCTLR (val);
	val &= ~(1<<12);
	set_SCTLR (val);
}

/* ------------------------------------------------------------------------------------*/
/* ------------------------------------------------------------------------------------*/

/* It was a long and painful process, but we eventually figured out
 * that dc ivac was acting the same as dc civac and performing a flush.
 * The first evidence of this was in the emac driver where receive
 * buffers were getting corrupted (as an attempted invalidate was
 * incorrectly forcing stale cache data onto a buffer that had just
 * been filled by network DMA.
 *
 * This code was written to pinpoint and understand the problem,
 * and the intent now is that it serve as a diagnostic to indicate
 * when the problem exists on a new board and/or chip.
 *
 * The test requires a cached and uncached
 * VA mapped to the same physical memory.
 * We fiddled with the MMU to make this so,
 *  but now for the h5, we always initialize the MMU
 * with a second set of uncached addresses.
 * A new chip will need to revisit this whole issue.
 *    see mmu_setup.c
 */

/* Here are the addresses used.
 * EXTRA is entirely superfluous.
 * (and will now cause a synch abort)
 */
#ifdef notdef
/* These were originally used for the PC2 with 1G */
#define NORMAL		0x70000000
#define UNCACHED	0xb0000000
#define EXTRA		0xf0000000
#endif

/* These will work for both the PC2 and the NEO */
/* This pokes around at the 256M boundary */
#define NORMAL		0x50000000
#define UNCACHED	0x90000000

/* We know this will fail on the H5 (at least on the Opi PC2,
 * but we keep it in the startup in case someday we fix the
 * problem.
 */
static void
cache_diag ( void )
{
		u32 *test = (u32 *) NORMAL;
		u32 *test_x = (u32 *) UNCACHED;
		// u32 *test_y = (u32 *) EXTRA;
		u32 val1, val2;
		int fail = 0;
		int noisy = 0;

#ifndef BOARD_H5
		/* This panic is to remind me to take
		 * a look at this when I develop code for
		 * new boards.
		 */
		panic ( "cache_diag not yet available\n" );
#endif

		/* Install values to get started.
		 */
		*test = 0xdeadbeef;
		*test_x = 0xdeadbeef;

		/* We check to see if things look
		 * sane to get started.  We get:
		Peek at 0000000070000000 = deadbeef
		Peek at 00000000b0000000 = deadbeef
		Peek at 00000000f0000000 = deadbeef
		*/
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			// printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		}

		/* Next we write via an uncached address.
		 * Indeed the cached value still remains.
		Peek at 0000000070000000 = deadbeef
		Peek at 00000000b0000000 = 0001abcd
		Peek at 00000000f0000000 = 0001abcd
		 */

		*test_x = 0x0001abcd;
		if ( noisy ) {
			printf ( "\n" );
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			// printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		}

		/* Next we write via the cached address.
		 * Indeed the cached value does not penetrate
		 * to RAM.
		Peek at 0000000070000000 = 1111abcd
		Peek at 00000000b0000000 = 0001abcd
		Peek at 00000000f0000000 = 0001abcd
		 */
		*test = 0x1111abcd;
		if ( noisy ) {
			printf ( "\n" );
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			// printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		}

		/* Now simulate a write */
		*test_x = 0xeeeeffff;	/* trash */
		*test = 0xaaaabbbb;		/* good data */
		if ( noisy ) {
			printf ( "Write --\n" );
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			printf ( "Flush\n" );
		}
		flush_line ( (u64) test );
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		}
		/* Seems to work just fine */

		/* Now simulate a read */
		*test_x = 0xccccdddd;	/* data into buffer */
		if ( noisy ) {
			printf ( "Read 1 --\n" );
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			printf ( "Invalidate\n" );
		}
		invalidate_line ( (u64) test );
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		}
		/* seems to work */

		/* Now simulate a read again */
		*test = 0x0000aaaa;
		*test_x = 0xccccdddd;	/* data read */
		if ( noisy ) {
			printf ( "Read 2 --\n" );
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			printf ( "Invalidate\n" );
		}
		invalidate_line ( (u64) test );
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		}

		if ( *test_x != 0xccccdddd )
			printf ( "cache_diag Fails !!\n" );
		if ( *test == 0xccccdddd )
			printf ( "cache_diag OK !!\n" );
		/* FAILS - the invalidate pushed data from the cache */
		/* It looks like ISW acts like CISW */

#ifndef BOARD_H5
		if ( fail ) {
			panic ( "cache_diag fails" );
		}
#endif

		/* Now simulate a read yet a 3rd way */
		/* flushing prior to the "DMA" fixes the
		 * issue -- a workaround we should not need
		 */
		*test = 0x0000aaaa;
		if ( noisy ) {
			printf ( "Read 3 --\n" );
			printf ( "Invalidate (preemptory)\n" );
		}
		invalidate_line ( (u64) test );	/* <<< add this */
		*test_x = 0xccccdddd;	/* data read */
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
			/* Correct here */
			printf ( "Invalidate\n" );
		}
		invalidate_line ( (u64) test );
		if ( noisy ) {
			printf ( " Peek at %016lx = %08x\n", test, *test );
			printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		}
		/* And works here as well */
}

/* THE END */
