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

/* CCSIDR */
#define CCSIDR_LINE_SIZE_OFFSET         0
#define CCSIDR_LINE_SIZE_MASK           0x7
#define CCSIDR_ASSOCIATIVITY_OFFSET     3
#define CCSIDR_ASSOCIATIVITY_MASK       (0x3FF << 3)
#define CCSIDR_NUM_SETS_OFFSET          13
#define CCSIDR_NUM_SETS_MASK            (0x7FFF << 13)

static int cache_line_size;

void
cache_init ( void )
{
	unsigned long ccsidr;
	int line_len;

        /* Read current CP15 Cache Size ID Register */
        asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));

        line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;

	/* Convert to bytes */
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
