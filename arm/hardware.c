/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * arm/hardware.c
 *
 * Tom Trebisky  5/5/2015
 * Tom Trebisky  1/6/2017
 *
 * Each architecture should have its own version of this.
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"

/* Remember the BBB is an ARM Cortex-A8, the cycle count
 * registers on other ARM variants have different addresses.
 * (They are the same on the Cortex-A7 in the Allwinner H3).
 *
 * All of this ARM cycle count stuff is supported by code
 * in locore.S (we could use inline assembly, but we don't.)
 * The counter runs at the full CPU clock rate (1 Ghz),
 * so it overflows every 4.3 seconds.
 * We can set the DIV bit to divide it by 64.
 * This means it counts at 15.625 Mhz and overflows
 * every 275 seconds, which may be important.
 * The overflow can be set up to cause an interrupt, but
 * that is a lot of bother, the best bet is to just reset
 * it before using it to time something.
 *
 * This was helpful:
 * http://stackoverflow.com/questions/3247373/
 *   how-to-measure-program-execution-time-in-arm-cortex-a8-processor
 */
#define PMCR_ENABLE	0x01	/* enable all counters */
#define PMCR_RESET	0x02	/* reset all counters */
#define PMCR_CC_RESET	0x04	/* reset CCNT */
#define PMCR_CC_DIV	0x08	/* divide CCNT by 64 */
#define PMCR_EXPORT	0x10	/* export events */
#define PMCR_CC_DISABLE	0x20	/* disable CCNT in non-invasive regions */

/* There are 4 counters besides the CCNT (we ignore them at present) */
#define CENA_CCNT	0x80000000
#define CENA_CTR3	0x00000008
#define CENA_CTR2	0x00000004
#define CENA_CTR1	0x00000002
#define CENA_CTR0	0x00000001

void
enable_ccnt ( int div64 )
{
	int val;

	val = get_pmcr ();
	val |= PMCR_ENABLE;
	if ( div64 )
	    val |= PMCR_CC_DIV;
	set_pmcr ( val );

	set_cena ( CENA_CCNT );
	// set_covr ( CENA_CCNT );
}

/* It does not seem necessary to disable the counter
 * to reset it.
 */
void
reset_ccnt ( void )
{
	/*
	set_cdis ( CENA_CCNT );
	set_pmcr ( get_pmcr() | PMCR_CC_RESET );
	set_cena ( CENA_CCNT );
	*/
	set_pmcr ( get_pmcr() | PMCR_CC_RESET );
}

/* delays with nanosecond resolution.
 * callers may also want to disable interrupts.
 * Also, beware of the compiler optimizing this away.
 * There is about 60 ns extra delay (call overhead).
 * A fussy caller could subtract 50 ns from the argument.
 * Good for up to 4.3 second delays.
 */
void
delay_ns ( int delay )
{
	reset_ccnt ();
	while ( get_ccnt() < delay )
	    ;
}

/* We are curious as to just what the state of things
 * is as handed to us by U-boot.
 */
void
mmu_status ( void )
{
	int cr_val;
	int mmu_base;

	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (cr_val) : : "cc");

	if ( cr_val & 0x01 )
	    printf ( "MMU enabled\n" );
	if ( cr_val & 0x04 )
	    printf ( "D cache enabled\n" );
	if ( cr_val & 0x1000 )
	    printf ( "I cache enabled\n" );

	asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r" (mmu_base) : : "cc");
	printf ( "MMU base: %08x\n", mmu_base );
}

void
hardware_init ( void )
{
	mem_malloc_init ( MALLOC_BASE, MALLOC_SIZE );

	enable_ccnt ( 0 );

	mmu_status ();
}

int
valid_ram_address ( unsigned long addr )
{
	if ( addr < BOARD_RAM_START )
	    return 0;
	if ( addr > BOARD_RAM_END )
	    return 0;
	return 1;
}

/* THE END */
