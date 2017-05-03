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
 *
 */

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

/* For no particular reason, we allocate memory in 16K quanta */
#define RAM_QUANTA	16*1024		/* 0 - 0x3fff */
#define Q_SHIFT		14

#define MEG	(1024*1024)

void mmu_nocache ( unsigned long );

unsigned long ram_alloc ( long );

extern char _end;

static unsigned long next_ram;
static unsigned long last_ram;

#define ram_round(x)	(((x) >> Q_SHIFT)+1)<<Q_SHIFT;

void
ram_init ( unsigned long start, unsigned long size )
{
	unsigned long kernel_end;

	next_ram = start;
	last_ram = start + size;
	printf ( "RAM %dM total starting at %08x\n", size/MEG, start );

	kernel_end = (unsigned long) &_end;
	printf ( "Kyu size: %d bytes\n", kernel_end - start );

	if ( (kernel_end % RAM_QUANTA) != 0 )
	    kernel_end = ram_round ( kernel_end );

	if ( kernel_end < next_ram )
	    panic ( "Kernel lost before ram" );
	if ( kernel_end > last_ram )
	    panic ( "Kernel lost after ram" );
	next_ram = kernel_end;

	// printf ( "Ram start: %08x\n", next_ram );
	// printf ( "Ram end: %08x\n", last_ram );
	printf ( "Ram alloc start: %08x\n", next_ram );

	// this was an early test
	// kernel_end = ram_alloc ( 55*1024 );
	// printf ( "Ram next: %08x\n", next_ram );
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
	return rv;
}

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

static void
mmu_display ( unsigned long *mmu )
{
	int i;
	int type = 0xff;

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
mmu_scan ( void )
{
	unsigned long *mmubase;

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	mmu_display ( mmubase );

}

#define	MMU_SEC		0x02	/* descriptor maps 1M section */
#define	MMU_BUF		0x04	/* enable write buffering */
#define	MMU_CACHE	0x08	/* enable caching */
#define	MMU_XN		0x10	/* execute never, disallow execution from this address */

/* other access bits differentiate user/kernel access */
#define	MMU_RW		0x0c00	/* allow r/w access */
#define	MMU_NONE	0x0000	/* allow no access */

#define MMU_NOCACHE	(MMU_SEC | MMU_RW)
#define MMU_DOCACHE	(MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW)

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
	mmubase[index] = desc;

	printf ( "REMAP mmubase is %08x\n", mmubase );
	printf ( "REMAP index for %08x is %d (%x) to --> %08x\n", va, index, index, &mmubase[index] );

	// mmu_page_table_flush ( mmubase, &mmubase[MMU_SIZE] );
	flush_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	invalidate_tlb ();
}

/* special call for tranparent mapped section */
void
mmu_nocache ( unsigned long addr )
{
	char *caddr = (char *) addr;

	mmu_remap ( addr, addr, MMU_NOCACHE );
	invalidate_dcache_range ( addr, &caddr[MEG] );
}

// #define MMU_TICK	0x100000
#define MMU_TICK	MEG

void
mmu_setup ( unsigned long *mmu )
{
	unsigned long addr;
	int i;
	int size;
	int start;

	addr = 0;
	for ( i=0; i<MMU_SIZE; i++ ) {
	    mmu[i] = addr | MMU_SEC | MMU_XN | MMU_RW;
	    addr += MMU_TICK;
	}

	size = BOARD_RAM_SIZE >> MMU_SHIFT;
	addr = BOARD_RAM_START;
	start = addr >> MMU_SHIFT;
	for ( i=0; i<size; i++ ) {
	    mmu[start+i] = addr | MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW;
	    addr += MMU_TICK;
	}
}

/* Call this to set up our very own MMU */
void
mmu_initialize ( void )
{
	unsigned long *new_mmu;

	printf ( "Initializing and relocating MMU\n" );
	new_mmu = (unsigned long *) ram_alloc ( MMU_SIZE * sizeof(unsigned long) );
	mmu_setup ( new_mmu );
	flush_dcache_range ( new_mmu, &new_mmu[MMU_SIZE] );
	// mmu_display ( new_mmu );

	/* TTBR0 */
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r" (new_mmu) );
	invalidate_tlb ();
}

/* Run this in a thread since it tends to generate data aborts */
void
mmu_tester ( int xxx )
{
	unsigned long evil;
	unsigned long *evilp;
	unsigned long nice;
	unsigned long *nicep;
	unsigned long val;

	/* XXX - these addresses are BBB specific
	 */
	evil = (unsigned long) 0x90000000;
	evilp = (unsigned long *) evil;

	nice = (unsigned long) 0x20000000;
	nicep = (unsigned long *) evil;

	/* This should work */
	*evilp = 0xabcd;
	printf ( "read from %08x: %08x\n", evil, *evilp );

	flush_dcache_range ( evilp, &evilp[64] );

	mmu_remap ( nice, evil, MMU_NOCACHE );

	printf ( "read from %08x: %08x\n", nice, *nicep );

#ifdef notdef
	unsigned long *mmubase;

	mmu_remap ( evil, evil, 0 );
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	invalidate_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	mmu_scan ();
#endif

#ifdef notdef
	/* and this too gives a data abort */
	mmu_remap ( evil, evil, MMU_SEC );

	printf ( "Try to read at %08x\n", evil );
	val = * (unsigned long *) evil;
	printf ( "read from %08x: %08x\n", evil, val );
#endif

#ifdef notdef
	mmu_remap ( evil, evil, 0 );

	/* after the above remapping, this gives us
	 * a data abort also.
	 */
	printf ( "Try to read at %08x\n", evil );
	val = * (unsigned long *) evil;
	printf ( "read from %08x: %08x\n", evil, val );
#endif

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

	/* was in locore.S */
	mmubase = (unsigned long *) get_mmu ();
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
	    mmu_scan ();
	    printf ( "mmu checking done\n" );
	}

	(void) thr_new ( "mmu", mmu_tester, (void *) 0, 3, 0 );
}

/* THE END */
