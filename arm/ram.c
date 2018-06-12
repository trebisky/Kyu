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
 */

/* XXX - TODO - split this into mmu.c and ram.c */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

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

static void mmu_initialize ( unsigned long, unsigned long );

unsigned long ram_alloc ( long );

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
 * Prior to this I worked only with the Orange Pi PC, with 1024M
 * But the NanoPi may have 512M (or 256M), which is also true of
 *  other Orange Pi variants.
 * Note that on my Orange Pi PC plus, I have 1024M of ram, but it
 * gets duplicated into the upper part of the address space.
 * I try to watch for that.
 * XXX - it would not be a bad idea to have the mmu unmap that.
 */
static unsigned long
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

void
ram_init ( unsigned long start, unsigned long size )
{
	unsigned long kernel_end;

	cache_line_size = get_cache_line_size();
	cache_line_mask = cache_line_size - 1;

	if ( size <= 0 ) {
	    printf ( "Probing for amount of ram\n" );
	    size = ram_probe ( start );
	} else {
	    printf ( "Configured with %d M of ram\n", size );
	}

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

	mmu_initialize ( start, size );

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
unsigned long
ram_alloc ( long arg )
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
unsigned long
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
unsigned long
ram_section_nocache ( int arg )
{
	unsigned long rv;

	rv = ram_section ( arg );
	mmu_nocache ( rv );
	return rv;
}

unsigned long
ram_next ( void )
{
	return next_ram;
}

unsigned long
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

/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */

/* MMU stuff - this could someday go into a different file
 *
 * The ARM MMU has a lot of capability that we are not using here.
 * It can have a two level page table and can deal with pages of various sizes.
 * We deal with 1M "sections" as the ARM people call them.
 * There are also two TTBR (translation table base) registers,
 * we use only the first of these.
 * Our table is build solely out of first level descriptors.
 * If the two low bits are 00 accessing that address yields a fault.
 *  all other bits in the descriptor are ignored.
 * If the two low bits are 10, the descriptor maps a section.
 * The values 01 and 11 map coarse and fine page tables and are not used here.
 */

/* Introduced 7-17-2016 while playing with
 * ARM timers and the ARM mmu
 */
#define MMU_SIZE (4*1024)

#define MMU_MASK	0xfffff
#define MMU_SHIFT	20

/* Mask for TTBR0 */
#define MMU_BASE_MASK 0x3fff

static void
mmu_display ( unsigned long ttbr )
{
	int i;
	int type = 0xff;
	unsigned long *mmu;

	mmu = (unsigned long *) (ttbr & ~MMU_BASE_MASK);

	printf ( "MMU at %08x\n", mmu );
	for ( i=0; i<16; i++ ) {
	    printf ( "%4d: %08x: %08x\n", i, &mmu[i], mmu[i] );
	}

	printf ( " -- -- -- --\n" );

	for ( i=0; i<MMU_SIZE; i++ ) {
	    if ( (mmu[i] & MMU_MASK) == type )
		continue;
	    type = mmu[i] & MMU_MASK;
	    printf ( "%4d: %08x: %08x\n", i, &mmu[i], mmu[i] );
	}

#ifdef notdef
	/* There is a big zone (over RAM with 0x0c0e */
	for ( i=0; i<MMU_SIZE; i++ ) {
	    if ( (mmu[i] & 0xffff) != 0x0c12 )
		printf ( "%4d: %08x: %08x\n", i, &mmu[i], mmu[i] );
	}
#endif
}

static void
mmu_base ( char *msg )
{
	unsigned long ttbr;

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr) );

	printf ( "%s TTBR0 = %08x\n", msg, ttbr );
}

void
mmu_scan ( char *msg )
{
	unsigned long mmubase;

	mmu_base ( msg );

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	mmu_display ( mmubase );

}

/* Oddly enough, U-Boot sets the XN bit for all of RAM and we had no problems ...
 * But we don't do that.
 */
#define	MMU_SEC		0x02	/* descriptor maps 1M section */
#define	MMU_BUF		0x04	/* enable write buffering */
#define	MMU_CACHE	0x08	/* enable caching */
#define	MMU_XN		0x10	/* execute never, disallow execution from this address */

/* other access bits differentiate user/kernel access */
#define	MMU_RW		0x0c00	/* allow r/w access */
#define	MMU_NONE	0x0000	/* allow no access */

#define MMU_NOCACHE	(MMU_SEC | MMU_RW)
#define MMU_DOCACHE	(MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW)

/* Use this to black out a page so it gives data aborts */
#define	MMU_INVALID	0

/* We are curious as to just what the state of things
 * is as handed to us by U-boot.
 */
void
mmu_status ( void )
{
	int scr;
	int mmu_base;

	/* SCTRL */
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (scr) : : "cc");

	if ( scr & 0x01 )
	    printf ( "MMU enabled\n" );
	if ( scr & 0x02 )
	    printf ( "A alignment enabled\n" );
	if ( scr & 0x04 )
	    printf ( "D cache enabled\n" );
	if ( scr & 0x1000 )
	    printf ( "I cache enabled\n" );
}

/* ARM v7 now has actual barrier instructions, but these work for
 * older ARM (like v5) as well as v7.
 * At some point U-boot used these since it compiled with -march=armv5
 * but we compile Kyu with -marm -march=armv7-a
 */
#define CP15ISB asm volatile ("mcr     p15, 0, %0, c7, c5, 4" : : "r" (0))
#define CP15DSB asm volatile ("mcr     p15, 0, %0, c7, c10, 4" : : "r" (0))
#define CP15DMB asm volatile ("mcr     p15, 0, %0, c7, c10, 5" : : "r" (0))

void
invalidate_tlb ( void )
{
        /* Invalidate entire unified TLB */
        asm volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));
        /* Invalidate entire data TLB */
        asm volatile ("mcr p15, 0, %0, c8, c6, 0" : : "r" (0));
        /* Invalidate entire instruction TLB */
        asm volatile ("mcr p15, 0, %0, c8, c5, 0" : : "r" (0));

        /* Barriers so we don't move on until this is complete */
        CP15DSB;
        CP15ISB;
}

void
mmu_remap ( unsigned long va, unsigned long pa, int bits )
{
	unsigned long desc;
	int index;
	unsigned long *mmubase;
	unsigned long reg = 0;

	desc = pa & ~MMU_MASK;
	desc |= bits;
	index = va >> MMU_SHIFT;

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );

	printf ( "mmu_remap, mmubase = %08x\n", mmubase );
	printf ( "mmu_remap, index, bits = %d, %08x\n", index, bits );

	// mmu_page_table_flush ( mmubase, &mmubase[MMU_SIZE] );
	flush_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	invalidate_tlb ();

	mmubase[index] = desc;

	// printf ( "REMAP mmubase is %08x\n", mmubase );
	// printf ( "REMAP index for %08x is %d (%x) to --> %08x\n", va, index, index, &mmubase[index] );

	// mmu_page_table_flush ( mmubase, &mmubase[MMU_SIZE] );
	flush_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	invalidate_tlb ();
}

/* Make a section uncacheable.
 * This is useful for areas that hold DMA buffers and
 * control structures (as needed by certain network devices).
 */
void
mmu_nocache ( unsigned long addr )
{
	char *caddr = (char *) addr;

	mmu_remap ( addr, addr, MMU_NOCACHE );
	invalidate_dcache_range ( addr, &caddr[MEG] );
}

/* make a section invalid so that accesses to it
 * yield data aborts.
 */
void
mmu_invalid ( unsigned long addr )
{
	char *caddr = (char *) addr;

	mmu_remap ( addr, addr, MMU_INVALID );
	invalidate_dcache_range ( addr, &caddr[MEG] );
}

// #define MMU_TICK	0x100000
#define MMU_TICK	MEG

static void
mmu_setup ( unsigned long *mmu, unsigned long ram_start, unsigned long ram_size )
{
	unsigned long addr;
	int i;
	int size;
	int start;

	/* First make everything transparent and uncacheable */
	addr = 0;
	for ( i=0; i<MMU_SIZE; i++ ) {
	    mmu[i] = addr | MMU_SEC | MMU_XN | MMU_RW;
	    addr += MMU_TICK;
	}

	/* This is primarily for the Orange Pi that aliases existing
	 * ram all through a 2G address space, we make accesses to those
	 * addresses invalid here, then open up those that actually exist
	 * in the loop below.
	 */
	size = BOARD_RAM_MAX >> MMU_SHIFT;
	addr = ram_start;
	start = addr >> MMU_SHIFT;

	for ( i=0; i<size; i++ ) {
	    mmu[start+i] = addr | MMU_INVALID;
	    addr += MMU_TICK;
	}

	size = ram_size >> MMU_SHIFT;
	addr = ram_start;
	start = addr >> MMU_SHIFT;

	/* Redo the ram addresses so they are cacheable */
	for ( i=0; i<size; i++ ) {
	    mmu[start+i] = addr | MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW;
	    addr += MMU_TICK;
	}

	/* And for the Orange Pi, make accesses to address 0 cause faults
	 * (This kills the multicore code, so commented out 6-12-2018)
	 */
	// mmu[0] = MMU_INVALID;
}

#define SCTRL_MMU_ENABLE		0x1
#define SCTRL_ALIGN_CHECK_ENABLE	0x2
#define SCTRL_DCACHE_ENABLE		0x4

static void
mmu_off ( void )
{
	int val;

	/* SCTRL */
        asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (val) : : "cc");

	val &= ~SCTRL_MMU_ENABLE;

        asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (val) : "cc");
}

static void
mmu_on ( void )
{
	int val;

	/* SCTRL */
        asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (val) : : "cc");

	val |= SCTRL_MMU_ENABLE;

        asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (val) : "cc");
}

/* The low 7 bits in the TTBR0 register are described on page B4-1731
 * of the ARM v7-A and v7-R Architecture Reference Manual
 */

/* Inner Region bits */
#define MMU_INNER_NOCACHE	0
#define MMU_INNER_WT		1
#define MMU_INNER_WBWA		0x40
#define MMU_INNER_WBNOWA	0x41

/* Outer Region bits */
#define MMU_OUTER_NOCACHE	0
#define MMU_OUTER_WT		0x08
#define MMU_OUTER_WBWA		0x10
#define MMU_OUTER_WBNOWA	0x18

/* Although we note that the U-Boot setup for the Orange Pi did set
 * the cacheability bits for the page tables, we just say the heck
 * with it, and set them zero like the BBB, it certainly works just fine.
 */

#ifdef BOARD_ORANGE_PI
/* copied from U-Boot setup */
// #define TTBR_BITS	0x59
// #define TTBR_BITS	(MMU_INNER_WBNOWA | MMU_OUTER_WBNOWA) 
#define TTBR_BITS	0x00
#else
/* on the BBB, U-Boot sets zero for these bits.
 * if I set 0x59 like the Orange Pi, it reads back 0x19
 * and seems to work just fine.
 * So it seems the Arm-A8 in the BBB has no inner region and
 * the low bit (0x01) simply indicates the mmu table is cacheable.
 */
#define TTBR_BITS	0x00
#endif

/* U-boot sets the TTBCR = 0  (the reset value)
 * for both the BBB and the Orange Pi.
 */

/* Call this to set up our very own MMU tables */
static void
mmu_initialize ( unsigned long ram_start, unsigned long ram_size )
{
	unsigned long *new_mmu;
	unsigned long new_ttbr;

	// printf ( "Initializing and relocating MMU\n" );

	/* The mmu table needs to be on a 16K boundary, so we allocate twice
	 * the memory we need and select a properly aligned section.
	 * This wastes 16K, but we have memory to burn.
	 */
	new_ttbr = ram_alloc ( 2*MMU_SIZE * sizeof(unsigned long) );
	new_ttbr += MMU_SIZE * sizeof(unsigned long) & ~MMU_BASE_MASK;
	new_mmu = (unsigned long *) new_ttbr;

	mmu_setup ( new_mmu, ram_start, ram_size );

	cpu_enter();
	// mmu_off();
	flush_dcache_range ( new_mmu, &new_mmu[MMU_SIZE] );
	// printf ( "cache flushed\n" );
	// mmu_display ( (unsigned long) new_mmu );

	invalidate_tlb ();

	new_ttbr |= TTBR_BITS;
	/* TTBR0 */
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r" (new_ttbr) );

	// mmu_on();
	cpu_leave();

	printf ( "New MMU at: %08x\n", new_ttbr );
}

/* Run this in a thread since it tends to generate data aborts */
/* Do not include in a production system since it just grabs
 * addresses and remaps them in curious ways.
 */
void
mmu_tester ( int xxx )
{
	unsigned long evil;
	unsigned long *evilp;
	unsigned long nice;
	unsigned long *nicep;
	unsigned long val;

	printf ( "Start MMU tester\n" );

/* The idea is to fiddle with the MMU to map a 1M chunk
 * of RAM into some unused place in the address map and
 * then verify that the mapping worked.
 */

#ifdef BOARD_ORANGE_PI
	evil = (unsigned long) 0x50000000;
	nice = (unsigned long) 0x20000000;
#else	/* BBB */
	evil = (unsigned long) 0x90000000;
	nice = (unsigned long) 0x20000000;
#endif

	evilp = (unsigned long *) evil;
	nicep = (unsigned long *) evil;

	/* This should work regardless of mmu mapping */
	*evilp = 0xabcd;
	printf ( "read from %08x: %08x\n", evil, *evilp );

	flush_dcache_range ( evilp, &evilp[64] );

	mmu_remap ( nice, evil, MMU_NOCACHE );

	/* This is the one that matters */
	printf ( "read from %08x: %08x\n", nice, *nicep );
	if ( *nicep == *evilp )
	    printf ( "-- that looks good\n" );
	else 
	    printf ( " ??? I don't like that !!\n" );

#ifdef notdef
	unsigned long *mmubase;

	mmu_remap ( evil, evil, 0 );
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	invalidate_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	mmu_scan ( "Before remapping\n" );
#endif

	/* We wish this would give a data abort, but it doesn't */
	mmu_remap ( evil, evil, MMU_SEC );

	printf ( "Try to read at %08x\n", evil );
	val = * (unsigned long *) evil;
	printf ( "read from %08x: %08x\n", evil, val );

	/* This indeed gives us data aborts */
	mmu_remap ( evil, evil, 0 );

	printf ( "Try to read at %08x\n", evil );
	val = * (unsigned long *) evil;
	printf ( "read from %08x: %08x\n", evil, val );

	printf ( "Finished with MMU tester\n" );

}

/* On the BBB this yields:
 *      SP = 8049ffd0
 * --SCTRL = 00c5187d
 * --TTBCR = 00000000
 * --TTBR0 = 9fff0000
 * --TTBR1 = 00000000
 * MMU base = 9fff0000
 */
void
mmu_show ( void )
{
	unsigned long *mmubase;
	unsigned long pu_base;
	unsigned long esp;
	unsigned long val;

#ifdef notdef
	/* We know this works */
	esp = get_sp();
	printf ( " SP = %08x\n", esp );
	esp = 0xBADBAD;
#endif

	/* This works just fine !! */
	asm volatile ("add %0, sp, #0\n" :"=r"(esp));
	printf ( " SP = %08x\n", esp );

	/* SCTRL */
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (val) : : "cc");
	printf ( "SCTRL = %08x\n", val );

	/* TTBCR */
	asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(val) );
	printf ( "--TTBCR = %08x\n", val );

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	printf ( "--TTBR0 = %08x\n", mmubase );

	/* TTBR1 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 1" : "=r"(val) );
	printf ( "--TTBR1 = %08x\n", val );

	if ( ! mmubase )
	    printf ( "MMU not initialized !\n" );
	else
	    printf ( "MMU base = %08x\n", mmubase );

	mmu_status ();

	/* protection unit base */
	asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(pu_base) );

	if ( pu_base )
	    printf ( "Protection unit base = %08x\n", pu_base );

	if ( mmubase ) {
	    mmu_scan ( "Setup by Kyu: " );
	    printf ( "mmu checking done\n" );
	}

	/* This just grabs random page addesses and fiddles
	 * with them, so should not be part of a production system.
	 */
	// (void) thr_new ( "mmu", mmu_tester, (void *) 0, 3, 0 );

	printf ( "mmu_show done\n" );
}

#ifdef notdef
/* Investigate odd ram issues on OPi */
static void
mmu_poke ( int meg )
{
	unsigned int addr;
	unsigned long val;

	addr = meg * MEG + BOARD_RAM_START;
	printf ( "MMU poke %d %08x\n", meg, addr );
	val = *((unsigned long *) addr );
	printf ( "MMU poke got: %08x\n", val );
}

/* trying to figure out issues before I understood
 * mmu table alignment requirements.
 */
void
mmu_debug ( void )
{
	mmu_poke ( 0 );
	mmu_poke ( 4 );
	mmu_poke ( 0 );
	mmu_poke ( 4 );

	/* odd */
	mmu_poke ( 2 );

	/* Fail */
	mmu_poke ( 1 );
	mmu_poke ( 3 );
	mmu_poke ( 5 );
	mmu_poke ( 6 );
	mmu_poke ( 8 );
	mmu_poke ( 7 );
}
#endif

/* THE END */
