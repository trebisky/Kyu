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

#ifdef notyet

/* XXX - look into armv8 ccnt */

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

/* Called at system startup */
void
enable_ccnt ( int div64 )
{
	int val;

	get_PMCR ( val );
	val |= PMCR_ENABLE;
	if ( div64 )
	    val |= PMCR_CC_DIV;
	set_PMCR ( val );

	set_CENA ( CENA_CCNT );
	// set_COVR ( CENA_CCNT );
}

/* It does not seem necessary to disable the counter
 * to reset it.
 */
void
reset_ccnt ( void )
{
	int reg;

	// set_CDIS ( CENA_CCNT );
	get_PMCR ( reg );
	reg |= PMCR_CC_RESET ;
	set_PMCR ( reg );
	// set_CENA ( CENA_CCNT );
}
#endif

void
hardware_init ( void )
{
	// enable_ccnt ( 0 );
}

/* THE END */
