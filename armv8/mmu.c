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

#define MEG	(1024*1024)
#define MMU_SIZE (4*1024)

/* I used to dynamically allocate this, but now we just let
 * the linker stick it in the data segment.
 */
/* page table must be on 16K boundary */
static unsigned int page_table[MMU_SIZE] __attribute__ ((aligned (16384)));

/* MMU stuff
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

// void mmu_scan ( char * );
// void mmu_status ( void );

/* This is a stub -- to get things going, we just continue to
 * use the setup we inherit from U-boot.
 * XXX
 */
void
mmu_initialize ( unsigned long ram_start, unsigned long ram_size )
{
	puts ( "XXX mmu_initialize still pending for ARM v8" );
}

/* Just a stub for now - this was an experiment for the armv7,
 * but I decided just to flush/invalidate instead.
 * With 512M descriptors on the armv8, this is basically useless.
 */
void
mmu_nocache ( unsigned long addr )
{
	panic ( "mmu_nocache not implemented yet for ARM v8" );
}

#ifdef OLD_v7_code

#define MMU_MASK	0xfffff
#define MMU_SHIFT	20

/* Mask for TTBR0 */
#define MMU_BASE_MASK 0x3fff

/* The low 7 bits in the TTBR0 register are described on page B4-1731
 * of the ARM v7-A and v7-R Architecture Reference Manual
 */
#define TTBR_SHAREABLE		2	/* shareable, cacheable */

/* Inner Region bits */
#define TTBR_INNER_NOCACHE	0
#define TTBR_INNER_WT		1
#define TTBR_INNER_WBWA		0x40
#define TTBR_INNER_WBNOWA	0x41

/* Outer Region bits */
#define TTBR_OUTER_NOCACHE	0
#define TTBR_OUTER_WT		8
#define TTBR_OUTER_WBWA		0x10
#define TTBR_OUTER_WBNOWA	0x18

/* Although we note that the U-Boot setup for the Orange Pi did set
 * the cacheability bits for the page tables, we just say the heck
 * with it, and set them zero like the BBB, it certainly works just fine.
 */

#ifdef BOARD_ORANGE_PI
/* copied from U-Boot setup */
// #define TTBR_BITS	0x59
// #define TTBR_BITS	(TTBR_INNER_WBNOWA | TTBR_OUTER_WBNOWA) 
#define TTBR_BITS	0x00
#else
/* on the BBB, U-Boot sets zero for these bits.
 * if I set 0x59 like the Orange Pi, it reads back 0x19
 * and seems to work just fine.
 * So it seems the Arm-A8 in the BBB has no inner region and
 * the low bit (0x01) simply indicates the mmu table is cacheable.
 */
// #define TTBR_BITS	0x00
#define TTBR_BITS	(TTBR_SHAREABLE|TTBR_INNER_WT|TTBR_OUTER_WT)
#endif

/* These first routines are called at startup */

/* Called by each core when it starts up.
 */
void
mmu_set_ttbr ( void )
{
	unsigned int new_ttbr;

	set_TTBCR ( 0 );

	new_ttbr = (unsigned int) page_table;
	new_ttbr |= TTBR_BITS;

	set_TTBR0 ( new_ttbr );

	/* "just in case" */
	set_TTBR1 ( new_ttbr );

	// set DACR to manager level for all domains
	set_DACR ( 0xffffffff );

	asm volatile ( "isb" );
	asm volatile ( "dsb" );
	asm volatile ( "dmb" );
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

/* From Marco */

#define	PDE_PTSEC	0x00000002 	/* Page Table Entry is 1 MB Section */
#define	PDE_B		0x00000004      /* Bufferable */
#define	PDE_C		0x00000008	/* Cacheable */
#define	PDE_XN		0x00000010      /* Execute Never */
#define	PDE_DOM		0x000001E0	/* Domain Bits */
#define	PDE_S		0x00010000	/* Shareable */
#define	PDE_NG		0x00020000	/* Not Global */
#define	PDE_NS		0x00080000	/* Non-Secure */
#define	PDE_BASEADDR	0xFFF00000

/* Access Permissions */
#define	PDE_AP		0x00008C00	/* Access Permission Bits */
#define	PDE_AP0		0x00000000
#define	PDE_AP1		0x00000400
#define	PDE_AP2		0x00000800
#define	PDE_AP3		0x00000C00	/* Full Access */
#define	PDE_AP4		0x00008000
#define	PDE_AP5		0x00008400
#define	PDE_AP6		0x00008800
#define	PDE_AP7		0x00008C00

/* Memory Region Attributes */
#define	PDE_TEX		0x00007000
#define	PDE_TEX0	0x00000000
#define	PDE_TEX1	0x00001000
#define	PDE_TEX2	0x00002000
#define	PDE_TEX3	0x00003000
#define	PDE_TEX4	0x00004000
#define	PDE_TEX5	0x00005000
#define	PDE_TEX6	0x00006000
#define	PDE_TEX7	0x00007000

#define PDE_MARCO_IO	(PDE_PTSEC | PDE_B | PDE_AP3 | PDE_TEX0	| PDE_S | PDE_XN )
#define PDE_MARCO_RAM	(PDE_PTSEC | PDE_B | PDE_C | PDE_AP3 | PDE_TEX5 )

// including PDE_S  will make ldrex/strex data abort
#define PDE_MARCO_RAM_S	(PDE_PTSEC | PDE_B | PDE_C | PDE_AP3 | PDE_TEX5	| PDE_S )

#define PDE_TOM_IO	( MMU_SEC | MMU_XN | MMU_RW )
#define PDE_TOM_RAM	( MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW )

/* Use this to black out a page so it gives data aborts */
#define	MMU_INVALID	0

// #define MMU_TICK	0x100000
#define MMU_TICK	MEG

static void
mmu_setup ( unsigned int *mmu, unsigned int ram_start, unsigned int ram_size )
{
	unsigned int addr;
	int size;
	int start;
	int i;

	printf ( "mmu_setup, page table at: %08x\n", mmu );
	printf ( "Ram at %08x, %d bytes\n", ram_start, ram_size );

	/* First make everything transparent and uncacheable */
	addr = 0;
	for ( i=0; i<MMU_SIZE; i++ ) {
	    // mmu[i] = addr | MMU_SEC | MMU_XN | MMU_RW;
	    // mmu[i] = PDE_TOM_IO | addr;
	    mmu[i] = PDE_MARCO_IO | addr;
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
	    // mmu[start+i] = addr | MMU_INVALID;
	    mmu[start+i] = MMU_INVALID | addr;
	    addr += MMU_TICK;
	}

	size = ram_size >> MMU_SHIFT;
	addr = ram_start;
	start = addr >> MMU_SHIFT;

	/* Redo the ram addresses so they are cacheable */
	for ( i=0; i<size; i++ ) {
	    // mmu[start+i] = addr | MMU_SEC | MMU_BUF | MMU_CACHE | MMU_RW;
	    // mmu[start+i] = PDE_TOM_RAM | addr;
	    // mmu[start+i] = PDE_MARCO_RAM | addr;
	    mmu[start+i] = PDE_MARCO_RAM_S | addr;
	    addr += MMU_TICK;
	}

	/* And for the Orange Pi, make accesses to address 0 cause faults
	 * (This kills the multicore code, so commented out 6-12-2018)
	 */
	// mmu[0] = MMU_INVALID;

	asm volatile ( "isb" );
	asm volatile ( "dsb" );
	asm volatile ( "dmb" );
}

/* Currently this is called very early in startup.
 * locore.S calls board_hardware_init()
 *  which calls ram_init(), which calls this.
 * The main goal is to set up our MMU tables.
 * It will be called with interrupts off.
 */

/* Call this to set up our very own MMU tables */
void
mmu_initialize ( unsigned long ram_start, unsigned long ram_size )
{
	unsigned int *new_mmu;

	new_mmu = page_table;

	// This just loads values into the page table
	// in ram, but does not try to enable anything.
	mmu_setup ( new_mmu, ram_start, ram_size );

	// We now call this with the D cache disabled
	// and it was flushed before disabling it.
	// flush_dcache_range ( new_mmu, &new_mmu[MMU_SIZE] );

	// mmu_display ( (unsigned int) new_mmu );

	// We do all this in locore.S now
	// invalidate_tlb ();
	// mmu_set_ttbr ();
}

/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */

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

	get_TTBR0 ( mmubase );

	printf ( "mmu_remap, mmubase = %08x\n", mmubase );
	printf ( "mmu_remap, index, bits = %d, %08x\n", index, bits );

	// mmu_page_table_flush ( mmubase, &mmubase[MMU_SIZE] );
	flush_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	// invalidate_tlb ();
	set_TLB_INV ( 0 );

	mmubase[index] = desc;

	// printf ( "REMAP mmubase is %08x\n", mmubase );
	// printf ( "REMAP index for %08x is %d (%x) to --> %08x\n", va, index, index, &mmubase[index] );

	// mmu_page_table_flush ( mmubase, &mmubase[MMU_SIZE] );
	flush_dcache_range ( mmubase, &mmubase[MMU_SIZE] );
	// invalidate_tlb ();
	set_TLB_INV ( 0 );
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


#define SCTRL_MMU_ENABLE		0x1
#define SCTRL_ALIGN_CHECK_ENABLE	0x2
#define SCTRL_DCACHE_ENABLE		0x4

static void
mmu_off ( void )
{
	int val;

	get_SCTLR ( val );
	val &= ~SCTRL_MMU_ENABLE;
	set_SCTLR ( val );
}

static void
mmu_on ( void )
{
	int val;

	get_SCTLR ( val );
	val |= SCTRL_MMU_ENABLE;
	set_SCTLR ( val );
}
#endif /* OLD_v7_code */

/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */

#ifdef OLD_ARMv7
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
	get_TTBR0 ( mmubase );
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
	unsigned long esp;
	unsigned long val;

	get_SP ( esp );
	printf ( " SP = %08x\n", esp );

	get_SCTLR ( val );
	printf ( "SCTRL = %08x\n", val );

	get_TTBCR ( val );
	printf ( "--TTBCR = %08x\n", val );

	get_TTBR0 ( mmubase );
	printf ( "--TTBR0 = %08x\n", mmubase );

	get_TTBR1 ( val );
	printf ( "--TTBR1 = %08x\n", val );

	if ( ! mmubase )
	    printf ( "MMU not initialized !\n" );
	else
	    printf ( "MMU base = %08x\n", mmubase );

	mmu_status ();

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

static void
mmu_display ( unsigned int ttbr )
{
	int i;
	int type = 0xff;
	unsigned int *mmu;

	mmu = (unsigned int *) (ttbr & ~MMU_BASE_MASK);

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

	get_TTBR0 ( ttbr );
	printf ( "%s TTBR0 = %08x\n", msg, ttbr );
}

void
mmu_scan ( char *msg )
{
	unsigned long mmubase;

	mmu_base ( msg );

	get_TTBR0 ( mmubase );
	mmu_display ( mmubase );

}

/* We are curious as to just what the state of things
 * is as handed to us by U-boot.
 */
void
mmu_status ( void )
{
	int scr;
	int mmu_base;

	get_SCTLR ( scr );

	if ( scr & 0x01 )
	    printf ( "MMU enabled\n" );
	if ( scr & 0x02 )
	    printf ( "A alignment enabled\n" );
	if ( scr & 0x04 )
	    printf ( "D cache enabled\n" );
	if ( scr & 0x1000 )
	    printf ( "I cache enabled\n" );
}

#endif

/* THE END */
