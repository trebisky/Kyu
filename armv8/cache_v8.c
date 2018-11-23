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

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void
flush_dcache_range ( addr_t start, addr_t stop )
{
        __asm_flush_dcache_range(start, stop);
}

/*
 * Invalidates range in all levels of D-cache/unified cache
 */
void
invalidate_dcache_range ( addr_t start, addr_t stop )
{
        __asm_invalidate_dcache_range(start, stop);
}

/* THE END */
