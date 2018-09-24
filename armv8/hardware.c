/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * armv8/hardware.c
 *
 * Tom Trebisky  9/23/2018
 *
 * Each architecture should have its own version of this.
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"
#include "cpu.h"

#define PMCR_CCNT_DIV64	0x8
#define PMCR_CCNT_RESET	0x4
#define PMCR_CCNT_ENA	0x1

#define PMENSET_CCNT	0x80000000

/* Called at system startup */
/* XXX - does not work yet */
static void
enable_ccnt ( int div64 )
{
	int val;

	val = PMENSET_CCNT;
        asm volatile("msr pmcntenset_el0, %0" : : "r" (val) );

        val = PMCR_CCNT_ENA;
	if ( div64 )
	    val |= PMCR_CCNT_DIV64;
        asm volatile("msr pmcr_el0, %0" : : "r" (val) );
}

/* It does not seem necessary to disable the counter
 * to reset it.
 */
void
reset_ccnt ( void )
{
	int reg;
	int rreg;

	asm volatile ("mrs %0, PMCR_EL0": "=r" (reg) );
	rreg = reg | PMCR_CCNT_RESET;
	asm volatile ("msr PMCR_EL0, %0": "=r" (rreg) );
	asm volatile ("msr PMCR_EL0, %0": "=r" (reg) );
	/* XXX - do we really need to clear the bit ? */
}

void
hardware_init ( void )
{
	enable_ccnt ( 0 );
}

/* THE END */
