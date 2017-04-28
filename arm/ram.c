/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ram.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  4/20/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"

/* This is just a starting point for a low level
 * ram allocator.  The basic idea is that we find ourselves
 * pulling ram addresses out of a hat and scattering these
 * assignments around in the code.  This will formalize all of that.
 * 
 * At this point this is specific to the Orange Pi, but it will someday
 * get ported back to the BBB and perhaps even move to the ARM directory.
 */

/*
 * Orange Pi addresses:
 *
 *  1G of ram is at 40000000 to 7fffffff
 *
 *  U-boot lives at 4a000000 to 4a093xxx
 *  U-boot stack at 79f34axx
 *  Kyu loads to .. 42000000 to 4209dxxx (not any more)
 *  Kyu loads to .. 40000000 to 4009dxxx
 *  Kyu core stacks 6f004000 ...
 *  MMU base  ..... 7dff0059
 */

/* For no particular reason, we allocate memory in 16K quanta */
#define RAM_QUANTA	16*1024		/* 0 - 0x3fff */
#define Q_SHIFT		14

unsigned long ram_alloc ( long );

extern char _end;

static unsigned long next_ram;
static unsigned long last_ram;

#define ram_round(x)	(((x) >> Q_SHIFT)+1)<<Q_SHIFT;

void
ram_init ( unsigned long start, unsigned long size )
{
	unsigned long kernel_end;

	next_ram = start;
	last_ram = start + size;

	kernel_end = (unsigned long) &_end;
	if ( kernel_end < next_ram )
	    panic ( "Kernel lost before ram" );
	if ( kernel_end > last_ram )
	    panic ( "Kernel lost after ram" );

	// printf ( "Ram start: %08x\n", next_ram );
	// printf ( "Ram end: %08x\n", last_ram );
	printf ( "Kyu end: %08x\n", kernel_end );
	next_ram = ram_round ( kernel_end );
	printf ( "Ram start: %08x\n", next_ram );

	kernel_end = ram_alloc ( 55*1024 );
	printf ( "Ram next: %08x\n", next_ram );

}

/* We never ever expect to free anything we allocate
 *  using this facility
 */
unsigned long
ram_alloc ( long size )
{
	unsigned long rv;

	size = ram_round ( size );
	if ( next_ram + size >= last_ram )
	    panic ( "ran outa ram" );
	rv = next_ram;
	next_ram += size;
	return rv;
}

/* THE END */
