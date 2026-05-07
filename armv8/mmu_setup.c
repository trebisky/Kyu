/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * mmu.c for the ARM v8
 *
 * Tom Trebisky  4/20/2017
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"
#include "cpu.h"

/* I was originally going to set up a level 2
 * table (with 2M blocks) and set aside the
 * last 2M block as an uncached area for the
 * network buffers and structure.
 *
 * But then I realized that I could set up a
 * separate uncached VA map for all of DRAM
 * using the next 1G entry.
 *
 * The scheme with 1G of ram is:
 *   0x4000_0000 to 0x7fff_ffff - normal cached RAM
 *   0x8000_0000 to 0xbfff_ffff - uncached RAM alias
 *   0xc000_0000 to 0xffff_ffff - invalid, illegal.
 *
 * The scheme with 512M of ram is:
 *   0x4000_0000 to 0x7fff_ffff - normal cached RAM
 *   0x8000_0000 to 0xbfff_ffff - uncached RAM alias
 *   0xc000_0000 to 0xffff_ffff - invalid, illegal.
 *
 * This sounded like a great idea, but involves more
 * effort in the emac driver than I expected.
 * The problem is that we have 3 sorts of addresses.
 * -- physical addresses used by emac DMA.
 * -- cached ram addresses, that match the above.
 * -- uncached ram addresses, which are distinct.
 * The rub is that physical and uncached addresses
 *  are now different.
 *
 * However having an uncached RAM alias does have
 *  advantages:
 *
 * It allows the "mem_probe()" code to work.
 * It allows the "dc ivac" test to work.
 *
 * We can make mem_probe() work by simply turning
 *  off the D cache while it runs, without any
 *  need for uncached addresses.
 *
 * This scheme should work just fine on the NanoPi NEO 2
 *  but that needs to be tested.
 */

/* So, we are back to my original idea
 * which is to set up a block of uncached ram
 * for use by network buffers and structures.
 *
 * This "hack" would be specific to the h5
 *
 * The first table (level 1) has 512 entries,
 *  only the first 4 need to be non-zero.
 *  Each maps 1G of address.
 * The second table (level 2) also has 512 entries,
 *  each maps 2M (so 2*512 = 1024).
 * The last of those 2M entries is set up uncached.
 *
 *  1M = 0x10_0000
 *  2M = 0x20_0000
 */

/* Old U-boot layout
PTE-7fff0000  0000000000000401
PTE-7fff0008  0000000040000711
 */

/* New U-boot layout
PTE-7fff0000  0000000000000401
PTE-7fff0008  000000007fff2003
PTE-7fff0010  000000007fff2401
PTE-7fff0018  000000007fff2401

 * TTBR0_EL2 = 000000007fff0000
 *   TCR_EL2 = 0000000080803520
 *  MAIR_EL2 = 000000ff440c0400
 *
 * We just retain the TCR and MAIR values
 *  from U-boot.
 */

/* When we break up our virtual address space
 * into 2M segments, the addresses for the last
 * few are like this:
ADDR 000000007fff0fd8  000000007f600000
ADDR 000000007fff0fe0  000000007f800000
ADDR 000000007fff0fe8  000000007fa00000
ADDR 000000007fff0ff0  000000007fc00000
ADDR 000000007fff0ff8  000000007fe00000
 *
 * My plan at this time is to use the last 2M page
 * for uncached network buffers.
 * The second to last 2M page will hold the
 * page tables in a 64K section at the end.
 */

/* A "chunk" is one of the 2M sections that the
 * level 2 table maps.
 */
#define CHUNK_SIZE 0x200000

/* We set aside 64K for page tables,
 * although we only need/user 8K
 */
#define PG_SIZE	64*1024

/* XXX - note that having "wired" values here
 * defeats the purpose of calling ram_probe()
 * before mmu_setup().  We can clean this up when
 * we tackle other 64 bit boards other than the H5
 * We will have to unless those other boards have
 *  the same ram layout as the H5.
 */
#ifdef BOARD_ORANGE_PI_PC2
/* with 1G ram */
#define RAM_BASE 0x40000000
#define MMU_BASE 0x7fcf0000
#define UNCACHED_BASE 0x7fe00000
#define NUM_CHUNKS  512
#else
/* Nano Pi Neo2 with 512M */
#define RAM_BASE 0x40000000
#define MMU_BASE 0x5fcf0000
#define UNCACHED_BASE 0x5fe00000
#define NUM_CHUNKS  256
#endif

static addr_t mmu_setup ( addr_t, u64 );

static inline void
mmu_on ( void )
{
		u64 val;

		get_SCTLR (val);
        val |= 0x1;
        set_SCTLR (val);
}

static inline void
mmu_off ( void )
{
		u64 val;

		get_SCTLR (val);
        val &= ~0x1;
        set_SCTLR (val);
}

/* This gets called very very early as Kyu boots,
 * even before the Kyu main code runs.
 * The assembly startup code calls board_mmu_init()
 * This will call ram_probe() before calling this
 * so that it can pass proper start and size values
 */
addr_t
mmu_initialize ( addr_t ram_start, u64 ram_size )
{
	// puts ( "XXX mmu_initialize still pending for ARM v8" );
	// printf ( " -- mmu_initialize --\n" );
	// printf ( "Ram at %016lx, %ld bytes\n", ram_start, ram_size );
	// printf ( " %d M of ram\n", ram_size/(1024*1024) );

	return mmu_setup ( ram_start, ram_size );
}

/* Just a stub for now - this was an experiment for the armv7,
 * but I decided just to flush/invalidate instead.
 * Unlikely ever to get implemented on arm64.
 * the same scheme now uses ram_section_nocache () below.
 */
void
mmu_nocache ( unsigned long addr )
{
	panic ( "mmu_nocache not implemented for ARM v8" );
}

/* This is called by the emac driver to get
 * the base address of an uncached memory block.
 */
void *
ram_section_nocache ( int xx )
{
		return (void *) UNCACHED_BASE;
}

/* Currently we ignore the arguments as far as the MMU
 * setup and have things wired in for our two H5 boards.
 * We do return a corrected size value.
 */
addr_t
mmu_setup ( addr_t ram_start, u64 ram_size )
{
		int i;
		u64 *addr;
		u64 add;
		u64 val;
		u64 *level1;
		u64 *level2;

		// dump_l ( (void *) MMU_BASE, 16 );

		addr = (u64 *) MMU_BASE;
		level1 = addr;

		memset ( addr, 0, PG_SIZE );

		/* We leave the other 2G of possible ram
		 * addresses 0 (invalid)
		 * This is very much H5 specific and has
		 * the value for RAM_BASE wired in.
		 */
		level1[0] = 0x0000000000000401;		/* IO */
		level1[1] = 0x0000000040000711;		/* 1G of ram */
		level1[2] = 0x0000000040000401;		/* again - uncached */
		level1[3] = 0;						/* invalid, just for the record */
		// dump_l ( level1, 4 );

		addr += 512;
		level2 = addr;

		level1[1] = (u64)addr | 3;

		// printf ( "Level 2 pages at %016lx\n", level2 );
		// printf ( "Addr[1] = %08lx\n", (u64)addr | 3 );

		add = RAM_BASE;

		for ( i=0; i<NUM_CHUNKS; i++ ) {
			*addr++ = add | 0x711;
			add += CHUNK_SIZE;
		}

		/* Now make the last entry uncached */
		val = level2[NUM_CHUNKS-1];
		// printf ( "Level2-last = %016lx\n", val );
		val &= ~0xfff;
		val |= 0x401;
		// printf ( "Level2-last = %016lx\n", val );
		level2[NUM_CHUNKS-1] = val;

		// dump_l ( level2, 8 );

		/* Don't turn off the MMU without first
		 * disabling the D cache.
		 * Is all this needed?  I don't know.
		 * But it does work, and it is fast.
		 */
		dcache_disable ();
		mmu_off ();
		asm volatile("msr ttbr0_el2, %0" : : "r" (level1) );
		tlb_invalidate_all ();
	    // asm volatile ( "tlbi alle2" );
		mmu_on ();
        asm volatile ( "dsb sy" );
        asm volatile ( "isb" );
		dcache_enable ();

		/* This value gets returned to board.c and then
		 * gets passed to ram_init()
		 */
		// ram_size -= CHUNK_SIZE * 2;
		ram_size -= CHUNK_SIZE;
		ram_size -= PG_SIZE;
		// printf ( "MMU setup returns %ld size\n", ram_size );

		return ram_size;
}

/* THE END */
