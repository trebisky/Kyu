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

void
tlb_invalidate_all ( void )
{
		__asm_invalidate_tlb_all ();
}

/*
 *  MRS is "move to register from special register.
 *  MSR is "move to special register from ARM core register.
 *  ARM inline assembly:
 *     asm ( "A" : O : I : C );
 *     "r" indicates a register (being read from)
 *     "=r" indicates a register (being read to)
 * #define get_CCNT(val)   asm volatile ( "mrs %0, PMCCNTR_EL0" : "=r" ( val ) )
 * #define set_CCNT(val)   asm volatile ( "msr PMCCNTR_EL0, %0" : : "r" ( val ) )
 */

/* The following shows the EL1 registers all zero, and:
 *
 * TTBR0_EL1 = 0000000000000000
 * TTBR1_EL1 = 0000000000000000
 * TCR_EL1 = 0000000000000000
 * TTBR0_EL2 = 000000007fff0000
 * TCR_EL2 = 0000000080803520
 */
void pte_show ( u64 * );
void pte_dump ( u64 *, int );

void
mmu_show ( void )
{
	u64 val;
	void *ttbr;
	u64 *tp;
	int count;
	// u64 *bogus;

#ifdef notdef
	printf ( "fix HCR.SWIO bit\n" );
	// Clear the SWIO bit
	asm volatile ("mrs %0, HCR_EL2" : "=r" (val));
	printf ( "HCR_EL2= %016lx\n", val );
	val &= ~2;
	// val = 0;
	asm volatile ("msr HCR_EL2, %0" : : "r" (val));
	asm volatile ("mrs %0, HCR_EL2" : "=r" (val));
	printf ( "HCR_EL2= %016lx\n", val );
#endif

	// U-boot dump code
	// dump_mmu ();

	// val = 0x1234abcdaabbccdd;
	// printf ( "Sanity check on printf: %016x\n", val );
	// printf ( "Sanity check on printf: %016lx\n", val );

	asm volatile ("mrs %0, HCR_EL2" : "=r" (val));
	printf ( "HCR_EL2= %016lx\n", val );

	asm volatile("mrs %0, ttbr0_el1" : "=r" (val) );
	printf ( "TTBR0_EL1 = %016lx\n", val );
	asm volatile("mrs %0, ttbr0_el2" : "=r" (val) );
	printf ( "TTBR0_EL2 = %016lx\n", val );
	ttbr = (void *) val;

	asm volatile("mrs %0, ttbr1_el1" : "=r" (val) );
	printf ( "TTBR1_EL1 = %016lx\n", val );
	// This does not exist, at least not in a Cortex-A53
	// asm volatile("mrs %0, ttbr1_el2" : "=r" (val) );
	// printf ( "TTBR1_EL2 = %016lx\n", val );

	asm volatile("mrs %0, tcr_el1" : "=r" (val) );
	printf ( "TCR_EL1 = %016lx\n", val );
	asm volatile("mrs %0, tcr_el2" : "=r" (val) );
	printf ( "TCR_EL2 = %016lx\n", val );
	asm volatile("mrs %0, mair_el2" : "=r" (val) );
	printf ( "MAIR_EL2 = %016lx\n", val );

	printf ( " TCR:ips = %08x\n", (val>>32) & 0x7 );
	printf ( " TCR:t1sz = %08x\n", (val>>16) & 0x3f );
	printf ( " TCR:t0sz = %08x\n", val & 0x3f );

	printf ( " TCR:tg0 = %08x\n", (val>>14) & 0x3 );
	printf ( " TCR:tg1 = %08x\n", (val>>30) & 0x3 );

#ifdef notdef
	tp = (u64 *) ttbr;
	pte_dump ( tp, 512 );
	printf ( "     ----------------------------\n" );
	tp += 512;
	pte_dump ( tp, 512 );
	printf ( "     ----------------------------\n" );
	tp += 512;
	pte_dump ( tp, 512 );
	printf ( "     ----------------------------\n" );
	tp += 512;
	pte_dump ( tp, 512 );

	/* garbage */
	printf ( "     ----------------------------\n" );
	tp += 512;
	pte_dump ( tp, 512 );
#endif

	// mmu_setup ();

#ifdef notdef
	dump_b ( ttbr, 8 );
	// 64 lines on screen
	// dump_l ( ttbr, 64 );
	tp = (u64 *) ttbr;

	pte_show ( tp++ );
	pte_show ( tp++ );
	pte_show ( tp++ );
	pte_show ( tp++ );
	pte_show ( tp++ );

	count = 0;
	while ( count < 16 ) {
		// pte_show ( tp );
		if ( (u64) tp > (u64) 0x80000000 ) {
			printf ( "Done\n" );
			break;
		}
		if ( ! *tp ) {
			tp++;
			continue;
		}
		pte_show ( tp++ );
		count++;
	}
#endif

#ifdef notdef
	// should cause a fault
	// it does -- a synchronous abort
	bogus = (u64 *) 0x200000000;
	val = *bogus;
	printf ( "bogus = %016lx\n", val );
#endif
}

void
pte_show ( u64 *addr )
{
//	char *cp = (char *) addr;

	printf ( "PTE-%08x ", addr );

//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );

//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );
//	printf ( "%02x", *cp++ );

	printf ( " %016lx", *addr );

	printf ( "\n" );
}

void
pte_dump ( u64 *addr, int count )
{
	int i;

	for ( i=0; i<count; i++ ) {
		pte_show ( addr );
		addr++;
	}
}

/* THE END */
