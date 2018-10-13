/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ram.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  4/20/2017
 * Tom Trebisky  6/15/2018
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

/* XXX ultimately this should not be an ARM specific or board specific thing
 * and should get moved up one level.
 */

/* This is just a starting point for a low level
 * ram allocator.  The basic idea is that we find ourselves
 * pulling ram addresses out of a hat and scattering these
 * assignments around in the code.  This will formalize all of that.
 * 
 * At this point this is specific to the Orange Pi, but it will someday
 * get ported back to the BBB and perhaps even move to the ARM directory.
 */

/*
 * Orange Pi addresses:
 *
 *  1G of ram is at 40000000 to 7fffffff
 *
 *  U-boot lives at 4a000000 to 4a093xxx
 *  U-boot stack at 79f34axx  (when handed to us)
 *  Kyu loads to .. 42000000 to 4209dxxx (not any more)
 *  Kyu loads to .. 40000000 to 4009dxxx
 *  Kyu core stacks 6f004000 ...
 *  MMU base  ..... 7dff0059
 *
 * BBB addresses:
 *
 *  0.5G of ram is at 80000000 to 9fffffff
 *
 *  U-boot lives at 80800000 to 8088ebf4
 *  U-boot stack at 9ef2a808  (when handed to us)
 *  Kyu loads to .. 80300000 to 80382850 (not any more)
 *  Kyu loads to .. 80000000 to 80082850
 *  MMU base  ..... 9fff0000
 */

/* For no particular reason, we allocate memory in 16K quanta.
 * Note that this guarantees that memory blocks are
 * aligned as per the 64 byte ARM cache line size.
 */
#define RAM_QUANTA	(16*1024)		/* 0 - 0x3fff */
#define Q_SHIFT		14

#define MEG	(1024*1024)

void mmu_nocache ( unsigned long );
void mmu_invalid ( unsigned long );

// void mmu_initialize ( unsigned long, unsigned long );

addr_t ram_alloc ( int );

extern char _end;

static unsigned long ram_start;
static unsigned long ram_endp;	/* end + 1 */

static unsigned long next_ram;
static unsigned long last_ram;

static int cache_line_size;
static unsigned long cache_line_mask;

#define ram_round(x)	( (((x) >> Q_SHIFT)+1)<<Q_SHIFT )

static int ram_sizes[] = { 2048, 1024, 512, 256 };

/* This was introduced for the NanoPi Neo
 *  6-14-2017
 *
 * Now called very early in initialization in order to figure
 *  out the ram size in order to set up the MMU.
 *
 * Prior to this I worked only with the Orange Pi PC, with 1024M
 * But the NanoPi may have 512M (or 256M), which is also true of
 *  other Orange Pi variants.
 * Note that on my Orange Pi PC plus, I have 1024M of ram, but it
 * gets replicated into the upper part of the address space.
 * I try to watch for that.
 *
 * XXX - it would not be a bad idea to have the mmu unmap that.
 */
unsigned long
ram_probe ( unsigned long start )
{
	unsigned long save;
	unsigned long flip = 0xabcd1234;
	unsigned long *p;
	unsigned long *p2;
	int i;

	for ( i=0; i< sizeof(ram_sizes) / sizeof(unsigned long); i++ ) {
	    p = (unsigned long *) (start + (ram_sizes[i]-1)*MEG );
	    p2 = (unsigned long *) (start + (ram_sizes[i]/2-1)*MEG );
	    // printf ( "Poke at %d %08x\n", ram_sizes[i], p );
	    // dump_l ( p, 2 );
	    save = *p;
	    *p = flip;
	    // dump_l ( p, 2 );
	    // flush_dcache_range ( p, &p[cache_line_size] );
	    // invalidate_dcache_range ( p, &p[cache_line_size] );
	    if ( *p == flip && *p2 != flip ) {
		*p = save;
		return ram_sizes[i] * MEG;
	    }
	    *p = save;
	}

	/* This should not happen */
	printf ( " *** Ram size wacky\n" );
	// return 1024 * MEG;
	// return 512 * MEG;
	return 256 * MEG;
}

/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */

void
ram_init ( unsigned long start, unsigned long size )
{
	unsigned long kernel_end;

	cache_line_size = get_cache_line_size();
	cache_line_mask = cache_line_size - 1;

#ifdef notdef
	if ( size <= 0 ) {
	    printf ( "Probing for amount of ram\n" );
	    size = ram_probe ( start );
	} else {
	    printf ( "Configured with %d M of ram\n", size );
	}
#endif

	ram_start = start;
	ram_endp = start + size;

	next_ram = start;
	last_ram = start + size;

	printf ( "RAM %dM total starting at %08x\n", size/MEG, start );

	kernel_end = (unsigned long) &_end;
	printf ( "Kyu image size: %d bytes\n", kernel_end - start );

	printf ( "kernel end: %08x\n", kernel_end );
	// printf ( "Quanta kernel end: %08x\n", RAM_QUANTA );
	// printf ( "Modulo kernel end: %08x\n", kernel_end % RAM_QUANTA );
	if ( (kernel_end % RAM_QUANTA) != 0 )
	    kernel_end = ram_round ( kernel_end );
	// printf ( "Nice kernel end: %08x\n", kernel_end );
	// printf ( "Round kernel end: %08x\n", ram_round(kernel_end) );

	if ( kernel_end < next_ram )
	    panic ( "Kernel lost before ram" );
	if ( kernel_end > last_ram )
	    panic ( "Kernel lost after ram" );

	next_ram = kernel_end;

	/* Now be sure this is cache aligned */

	// printf ( "Ram start: %08x\n", next_ram );
	// printf ( "Ram end: %08x\n", last_ram );
	printf ( "Ram alloc start: %08x\n", next_ram );
	if ( next_ram & cache_line_mask )
	    panic_spin ( "Invalid cache alignment in ram_init\n" );

	// not any more, now called from board_mmu_init()
	// mmu_initialize ( start, size );

	// this was an early test
	// kernel_end = ram_alloc ( 55*1024 );
	// printf ( "Ram next: %08x\n", next_ram );
}

int
valid_ram_address ( unsigned long addr )
{
        if ( addr < ram_start )
            return 0;

        if ( addr >= ram_endp )
            return 0;

        return 1;
}

/* We never ever expect to free anything we allocate
 *  using this facility
 */
addr_t
ram_alloc ( int arg )
{
	unsigned long rv;
	unsigned long size;

	size = arg;
	if ( (size % RAM_QUANTA) != 0 )
	    size = ram_round ( size );

	if ( next_ram + size >= last_ram )
	    panic ( "ran outa ram" );
	rv = next_ram;
	next_ram += size;

	printf ( "ram_alloc: %d (%d) bytes -- %08x\n", size, arg, rv );

	if ( rv & cache_line_mask )
	    panic_spin ( "Invalid cache alignment in ram_alloc\n" );

	return rv;
}

/* Allocate some number of 1M sections */
addr_t
ram_section ( int arg )
{
	unsigned long count;

	count = arg * MEG;

	if ( next_ram + count >= last_ram )
	    panic ( "ran outa ram sections" );

	last_ram -= count;
	printf ( "ram_section: %d (%d) bytes -- %08x\n", count, arg, last_ram );
	return last_ram;
}

/* Allocate some number of 1M sections
 *  with caching turned off.
 */
addr_t
ram_section_nocache ( int arg )
{
	unsigned long rv;

	rv = ram_section ( arg );
	mmu_nocache ( rv );
	return rv;
}

addr_t
ram_next ( void )
{
	return next_ram;
}

addr_t
ram_size ( void )
{
	return last_ram - next_ram;
}

#define MEG	(1024*1024)
void
ram_show ( void )
{
	int size;

	size = (last_ram - next_ram) / MEG;
	printf ( "RAM %dM+ available starting at %08x\n", size, next_ram );
}

/* THE END */
