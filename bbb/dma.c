/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * dma.c
 * Driver for the EDMA (aka TPCC)
 */
#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include <omap_ints.h>
// #include <cpu.h>

#define DMA_BASE      0x49000000
#define DMA_0_BASE    0x49800000
#define DMA_1_BASE    0x49900000
#define DMA_2_BASE    0x49a00000

static void
probeit ( char *msg, unsigned long addr )
{
	int s;

	s = data_abort_probe ( addr );
	if ( s )
	    printf ( "Probe %s at %08x, Fails\n", msg, addr );
	else
	    printf ( "Probe %s at %08x, ok\n", msg, addr );
}

void
dma_probe ( void )
{
	probeit ( "dma", DMA_BASE );
	probeit ( "dma_0", DMA_0_BASE );
	probeit ( "dma_1", DMA_1_BASE );
	probeit ( "dma_2", DMA_2_BASE );
}

void
dma_init ( void )
{
	// dma_probe ();
}

/* THE END */
