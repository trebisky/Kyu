/*
 * Copyright (C) 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * mmu_xx.c for the ARM v8
 *
 * Tom Trebisky  5/4/2026
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"
#include "cpu.h"

/* This is by and large a big hack and experiment.
 * After the setup, the page tables are:
 * PTE-7fff0000  0000000000000401
 * PTE-7fff0008  0000000040000711
 * PTE-7fff0010  0000000040000401
 * PTE-7fff0018  0000000040000401
 */

#define MMU_BASE	0x7fff0000
// #define RAM_BASE	0x40000000

/* For this to work, we need both a cached and uncached
 * VA mapped to the same physical memory
 */

#ifdef notdef
static void
mmu_setup ( void )
{
		u64 *mmu;
		u64 mmu0, mmu1;
		u64 val;
		u64 attr;

		mmu = (u64 *) MMU_BASE;

		mmu0 = mmu[0];
		mmu1 = mmu[1];

		attr = mmu0 & 0xfff;
		val = mmu1 & ~0xfff;
		mmu[2] = val | attr;
		mmu[3] = val | attr;

		asm volatile ( "tlbi alle2" );
		asm volatile ( "dsb sy" );
		asm volatile ( "isb" );
}
#endif

static void
flush_line ( u64 addr )
{
		/* clean & invalidate data or unified cache */
		asm volatile("dc civac, %0" : : "r" (addr) );
}

static void
invalidate_line ( u64 addr )
{
		/* invalidate data or unified cache */
		asm volatile("dc ivac, %0" : : "r" (addr) );
}

void
mmu_xx ( void )
{
		u32 *test = (u32 *) 0x70000000;
		u32 *test_x = (u32 *) 0xb0000000;
		u32 *test_y = (u32 *) 0xf0000000;
		u32 val1, val2;

		printf ( "mmu_xx (diag) not available\n" );
		return;

		// mmu_setup ();

		/* Prior experiment tells us that this area
		 * never gets initialized by a reboot,
		 * so we need to put values there.
		 */
		*test = 0xdeadbeef;
		*test_x = 0xdeadbeef;

		/* We check to see if things look
		 * sane to get started.  We get:
		Peek at 0000000070000000 = deadbeef
		Peek at 00000000b0000000 = deadbeef
		Peek at 00000000f0000000 = deadbeef
		*/
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );

		/* Next we write via an uncached address.
		 * Indeed the cached value still remains.
		Peek at 0000000070000000 = deadbeef
		Peek at 00000000b0000000 = 0001abcd
		Peek at 00000000f0000000 = 0001abcd
		 */

		printf ( "\n" );
		*test_y = 0x0001abcd;
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );

		/* Next we write via the cached address.
		 * Indeed the cached value does not penetrate
		 * to RAM.
		Peek at 0000000070000000 = 1111abcd
		Peek at 00000000b0000000 = 0001abcd
		Peek at 00000000f0000000 = 0001abcd
		 */
		printf ( "\n" );
		*test = 0x1111abcd;
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );

		printf ( "repeat --\n" );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		printf ( "repeat --\n" );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );

		/* Now simulate a write */
		printf ( "Write --\n" );
		*test_y = 0xeeeeffff;	/* trash */
		*test = 0xaaaabbbb;		/* good data */
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		printf ( "Flush\n" );
		flush_line ( (u64) test );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		/* Seems to work just fine */

		/* Now simulate a read */
		printf ( "Read 1 --\n" );
		*test_y = 0xccccdddd;	/* data into buffer */
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		printf ( "Invalidate\n" );
		invalidate_line ( (u64) test );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		/* seems to work */

		/* Now simulate a read again */
		printf ( "Read 2 --\n" );
		*test = 0x0000aaaa;
		*test_y = 0xccccdddd;	/* data read */
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		printf ( "Invalidate\n" );
		invalidate_line ( (u64) test );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );

		if ( *test_y != 0xccccdddd )
			printf ( "Fails !!\n" );
		if ( *test == 0xccccdddd )
			printf ( "OK !!\n" );
		/* FAILS - the invalidate pushed data from the cache */
		/* It looks like ISW acts like CISW */

		/* Now simulate a read yet a 3rd way */
		/* flushing prior to the "DMA" fixes the
		 * issue -- a workaround we should not need
		 */
		printf ( "Read 3 --\n" );
		*test = 0x0000aaaa;
		printf ( "Invalidate (preemptory)\n" );
		invalidate_line ( (u64) test );	/* <<< add this */
		*test_y = 0xccccdddd;	/* data read */
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		/* Correct here */
		printf ( "Invalidate\n" );
		invalidate_line ( (u64) test );
		printf ( " Peek at %016lx = %08x\n", test, *test );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
		/* And works here as well */

#ifdef notdef
		/* Now invalidate the cache and see if
		 * we see what we expect.
		 */
		printf ( "invalidate -- XXX\n" );

		val1 = *test;
		invalidate_line ( (u64) test );
		val2 = *test;
		// invalidate_line ( (u64) test );
		printf ( " Peek at %016lx = %08x\n", test, val1 );
		printf ( " Peek at %016lx = %08x\n", test, val2 );
		printf ( " Peek at %016lx = %08x\n", test_x, *test_x );
		printf ( " Peek at %016lx = %08x\n", test_y, *test_y );
#endif
}

/* THE END */
