/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * cache.c for Kyu for the ARM v8 processor
 *
 * Tom Trebisky  5/30/2017
 *
 */

#include "types.h"

/* From U-Boot sources, just a tiny extract from:
 *  u-boot-2018.09/arch/arm/cpu/armv8/cache_v8.c
 */

/* Showing each line produces an outrageous amount of output
 */
static void
flusher ( addr_t start, addr_t stop )
{
		u64 val;
		int line;
		u64 add;

		printf ( "FLUSH ------------------- %08x %08x\n", start, stop );
		asm volatile ("mrs %0, CTR_EL0": "=r" (val) );
		printf ( "CTR = %016x\n", val );
		line = (val >> 16) & 0xf;
		line = 4 << line;
		printf ( "cache line length = %d\n", line );
		add &= ~(line - 1);
		while ( add < stop ) {
			printf ( "Line: %08x\n", add );
			add += line;
		}
}

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void
flush_dcache_range ( addr_t start, addr_t stop )
{
		// printf ( "FLUSH ------------------- %08x %08x\n", start, stop );
		// flusher ( start, stop );
        __asm_flush_dcache_range ( start, stop );
}

/*
 * Invalidates range in all levels of D-cache/unified cache
 */
void
invalidate_dcache_range ( addr_t start, addr_t stop )
{
		// printf ( "INVALIDATE ------------------- %08x %08x\n", start, stop );
        __asm_invalidate_dcache_range(start, stop);
}

/* THE END */
