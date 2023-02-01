/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * cache.c for Kyu for the ARM v7 processor
 *
 * Tom Trebisky  5/30/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

#include "cpu.h"

/* sequester all these ugly CP15 instructions here
 * and provide macros for them.
 */

// CSSELR - Cache select register - selects cache for CCSIDR
#define get_CSSELR(val)  asm volatile ( "mrc p15, 2, %0, c0, c0, 0" : "=r" ( val ) )
#define set_CSSELR(val)  asm volatile ( "mcr p15, 2, %0, c0, c0, 0" : : "r" ( val ) )

// CCSIDR - Cache size ID registers (CSSELR selects which cache)
#define get_CCSIDR(val) asm volatile ( "mrc p15, 1, %0, c0, c0, 0" : "=r" ( val ) )
// CTR - Cache type register -- read only
#define get_CTR(val) asm volatile ( "mrc p15, 0, %0, c0, c0, 1" : "=r" ( val ) )
// CLIDR - Cache level ID register -- read only
#define get_CLIDR(val) asm volatile ( "mrc p15, 1, %0, c0, c0, 1" : "=r" ( val ) )

/* See ARMv7 ref "Cache Maintenance operations" p. B3-1496
 * These really do need to involve an ARM register, even if it is always set 0.
 * The register contents are ignored.
 * These are all WO (write only) instructions
 */

// branch predictor invalidate
#define BPIALL()	asm volatile ("mcr p15, 0, %0, c7, c5, 6" : : "r" (0) );
#define BPIALLIS()	asm volatile ("mcr p15, 0, %0, c7, c1, 6" : : "r" (0) );
#define BPIMVA(val)	asm volatile ("mcr p15, 0, %0, c7, c5, 7" : : "r" (val) );

// clean and invalidate
#define DCCIMVAC(val)	asm volatile ("mcr p15, 0, %0, c14, c1, 7" : : "r" (val) );
#define DCCISW(val)	asm volatile ("mcr p15, 0, %0, c7, c14, 2" : : "r" (val) );

// clean by MVA
#define DCCMVAC(val)	asm volatile ("mcr p15, 0, %0, c7, c10, 1" : : "r" (val) );
#define DCCMVAU(val)	asm volatile ("mcr p15, 0, %0, c7, c11, 1" : : "r" (val) );
// Data cache clean by set/way 
#define DCCSW(val)	asm volatile ("mcr p15, 0, %0, c7, c10, 0" : : "r" (val) );

// invalidate by MVA
#define DCIMVAC(val)	asm volatile ("mcr p15, 0, %0, c7, c6, 1" : : "r" (val) );
// invalidate by set/way
#define DCISW(val)	asm volatile ("mcr p15, 0, %0, c7, c6, 2" : : "r" (val) );

// I cache invalidate all
#define ICIALL()	asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) );
#define ICIALLIS()	asm volatile ("mcr p15, 0, %0, c7, c1, 0" : : "r" (0) );
#define ICIMVA(val)	asm volatile ("mcr p15, 0, %0, c7, c5, 1" : : "r" (val) );

/* MMU related stuff
 */
#define get_TTBR0(val)  asm volatile ( "mrc p15, 0, %0, c2, c0, 0" : "=r" ( val ) )
#define set_TTBR0(val)  asm volatile ( "mcr p15, 0, %0, c2, c0, 0" : : "r" ( val ) )

#define get_TTBR1(val)  asm volatile ( "mrc p15, 0, %0, c2, c0, 1" : "=r" ( val ) )
#define set_TTBR1(val)  asm volatile ( "mcr p15, 0, %0, c2, c0, 1" : : "r" ( val ) )

#define get_TTBCR(val)  asm volatile ( "mrc p15, 0, %0, c2, c0, 2" : "=r" ( val ) )
#define set_TTBCR(val)  asm volatile ( "mcr p15, 0, %0, c2, c0, 2" : : "r" ( val ) )

/* CSSELR */
#define CSSEL_D		0	/* Data or unified */
#define CSSEL_I		1	/* Instruction */
#define CSSEL_level(x)	(x-1)<<1

/* CCSIDR */
#define CCSIDR_LINE_SIZE_OFFSET         0
#define CCSIDR_LINE_SIZE_MASK           0x7
#define CCSID_LINE_MASK           0x7

#define CCSID_ASSO_SHIFT     3
#define CCSID_ASSO_MASK       (0x3FF << 3)

#define CCSID_SETS_SHIFT          13
#define CCSID_SETS_MASK            (0x7FFF << 13)

#define CCSID_WT	0x80000000
#define CCSID_WB	0x40000000
#define CCSID_RA	0x20000000
#define CCSID_WA	0x10000000

#define CTR_DMIN_SHIFT	16
#define CTR_ERG_SHIFT	20
#define CTR_CWG_SHIFT	24

static int cache_line_size = 64;

static void
show_ccsidr ( char *msg, unsigned int reg )
{
	int sets;
	int asso;
	int line;
	int llen, lwords;

	printf ( "CCSIDR, %s = %08x\n", msg, reg );
	if ( reg & CCSID_WT ) printf ( " supports Write through\n" );
	if ( reg & CCSID_WB ) printf ( " supports Write back\n" );
	if ( reg & CCSID_RA ) printf ( " supports Read allocate\n" );
	if ( reg & CCSID_WA ) printf ( " supports Write allocate\n" );

	sets = 1 + ((reg & CCSID_SETS_MASK) >> CCSID_SETS_SHIFT);
	asso = 1 + ((reg & CCSID_ASSO_MASK) >> CCSID_ASSO_SHIFT);
	printf ( " %d sets as %d way\n", sets, asso );

        llen = (reg & CCSID_LINE_MASK) + 2;
	lwords = 1 << llen;
	printf ( " line size = %d words (%d bytes)\n", lwords, lwords*4 );
}

static void
show_ctr ( unsigned int reg )
{
	int size;

	printf ( "CTR = %08x\n", reg );

	size = 1 << (reg & 0xf);
	printf ( " CTR - minimum line in I cache = %d\n", size*4 );
	size = 1 << ((reg>>CTR_DMIN_SHIFT) & 0xf);
	printf ( " CTR - minimum line in D cache = %d\n", size*4 );

	size = 1 << ((reg>>CTR_CWG_SHIFT) & 0xf);
	printf ( " CTR - CWG = %d\n", size*4 );
	size = 1 << ((reg>>CTR_ERG_SHIFT) & 0xf);
	printf ( " CTR - ERG = %d\n", size*4 );
}

#define CLIDR_LOUU_SHIFT	27
#define CLIDR_LOC_SHIFT		24
#define CLIDR_LOUIS_SHIFT	21

static void
show_clidr ( unsigned int reg )
{
	int val;
	int type;
	int i;

	printf ( "CLIDR = %08x\n", reg );

	val = (reg>>CLIDR_LOUU_SHIFT) & 0x7;
	printf ( " CLIDR - LoUU = %d\n", val );
	val = (reg>>CLIDR_LOC_SHIFT) & 0x7;
	printf ( " CLIDR - Loc = %d\n", val );
	val = (reg>>CLIDR_LOUIS_SHIFT) & 0x7;
	printf ( " CLIDR - LoUIS = %d\n", val );

	val = reg;
	for ( i=0; i<7; i++ ) {
	    type = val & 0x7;
	    if ( type == 0 )
		break;
	    printf ( " CLIDR - L%d type = %d", i+1, type );
	    if ( type == 1 ) printf ( " I" );
	    if ( type == 2 ) printf ( " D" );
	    if ( type == 3 ) printf ( " I/D" );
	    if ( type == 4 ) printf ( " unified" );
	    printf ( "\n" );
	    val >>= 3;
	}
}

void
cache_init ( void )
{
	unsigned int ccsidr;
	unsigned int clidr;
	unsigned int ctr;
	int line_len;
	int val;

	printf ( "-- Cache init --\n" );

	get_CLIDR ( clidr );
	show_clidr ( clidr );

	get_CTR ( ctr );
	show_ctr ( ctr );

        /* Read current CP15 Cache Size ID Register */
	val = CSSEL_level(1) | CSSEL_D;
	set_CSSELR ( val );
	get_CCSIDR ( ccsidr );
	// printf ( "CCSIDR, L1-D = %08x\n", ccsidr );
	show_ccsidr ( "L1-D", ccsidr );

	val = CSSEL_level(1) | CSSEL_I;
	set_CSSELR ( val );
	get_CCSIDR ( ccsidr );
	show_ccsidr ( "L1-I", ccsidr );

	val = CSSEL_level(2) | CSSEL_D;
	set_CSSELR ( val );
	get_CCSIDR ( ccsidr );
	show_ccsidr ( "L2-D", ccsidr );

	val = CSSEL_level(2) | CSSEL_I;
	set_CSSELR ( val );
	get_CCSIDR ( ccsidr );
	show_ccsidr ( "L2-I", ccsidr );

	/* On Orange Pi --
ARM cache line size: 64
	 * CLIDR = 0a200023
	 * CLIDR says:
	 *   LoUU is  001 (clear/invalidate to level 1)
	 *   LoC is   010 (clear/invalidate to level 2)
	 *   LoUIS is 001 (clear/invalidate to level 1)
	 *   L2 is unified, L1 is I and D

	 * CCSIDR, L1-D = 700fe01a
	 * CCSIDR, L1-I = 203fe009
	 * CCSIDR, L2-D = 707fe03a
	 * CCSIDR, L2-I = 707fe03a

	 * CCSIDR L1-D says:
	 *  does not support Write through
	 *  supports Write back
	 *  supports Read allocate
	 *  supports Write allocate
	 *  number of sets = 0xFE>>2+1 = 128
	 *  associativity = 4 way
	 */


        line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;

	/* Convert to bytes
	 * get 64 on Orange Pi H3 (cortex-A7)
	 * get 64 on BBB (cortex-A8)
	 */
        cache_line_size = 1 << (line_len + 2 );

	printf ( "ARM cache line size: %d\n", cache_line_size );
}

int
get_cache_line_size ( void )
{
	return cache_line_size;
}

#define SCTLR_I_CACHE	0x1000
#define SCTLR_D_CACHE	4
#define SCTLR_ALIGN	2
#define SCTLR_MMU	1

void
dcache_off ( void )
{
}

void
dcache_on ( void )
{
}

void
icache_invalidate_all ( void )
{
        /*
         * Invalidate all instruction caches to PoU.
         * Also flushes branch target cache.
         */
	ICIALL ();

        /* Invalidate entire branch predictor array */
	BPIALL ();

        /* Full system DSB - make sure that the invalidation is complete */
        dsb ();

        /* ISB - make sure the instruction stream sees it */
        isb ();
}

void
icache_off ( void )
{
	reg_t sctlr;

	icache_invalidate_all ();

	get_SCTLR ( sctlr );
	sctlr &= ~SCTLR_I_CACHE;
	set_SCTLR ( sctlr );
}

void
icache_on ( void )
{
	reg_t sctlr;

	get_SCTLR ( sctlr );
	sctlr |= SCTLR_I_CACHE;
	set_SCTLR ( sctlr );
}

/* ========================================== */
/* ========================================== */

static void
sctlr_show ( void )
{
        int scr;

        get_SCTLR ( scr );

        if ( scr & 0x01 )
            printf ( "MMU enabled\n" );
        if ( ! (scr & 0x02) )
            printf ( "A alignment check disabled\n" );
        if ( scr & 0x04 )
            printf ( "D cache enabled\n" );
        if ( scr & 0x1000 )
            printf ( "I cache enabled\n" );
}


/* Called from IO test menu */
void
arch_cache_test ( void )
{
	reg_t cpsr;
	reg_t sctlr;
	reg_t new_sctlr;

	printf ( "Cache test\n" );

	get_CPSR ( cpsr );
	printf ( " CPSR  = %08x\n", cpsr );

	get_SCTLR ( sctlr );
	printf ( " SCTLR = %08x\n", sctlr );
	sctlr_show ();

	/* takes 5 sec */
	printf ( "Start 1 second delay\n" );
	delay_us ( 1000 * 1000 );
	printf ( " Delay finished\n" );

	new_sctlr = sctlr;
	new_sctlr &= ~SCTLR_I_CACHE;
	set_SCTLR ( new_sctlr );
	printf ( "I cache disabled\n" );

	/* takes 10 sec */
	printf ( "Start 1 second delay\n" );
	delay_us ( 1000 * 1000 );
	printf ( " Delay finished\n" );

	set_SCTLR ( sctlr );
}

/* THE END */
