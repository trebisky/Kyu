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
 *  U-boot stack at 79f34axx  (when handed to us)
 *  Kyu loads to .. 42000000 to 4209dxxx (not any more)
 *  Kyu loads to .. 40000000 to 4009dxxx
 *  Kyu core stacks 6f004000 ...
 *  MMU base  ..... 7dff0059
 *
 * BBB addresses:
 *
 *  0.5G of ram is at 80000000 to 9fffffff
 *
 *  U-boot lives at 80800000 to 8088ebf4
 *  U-boot stack at 9ef2a808  (when handed to us)
 *  Kyu loads to .. 80300000 to 80382850 (not any more)
 *  Kyu loads to .. 80000000 to 80082850
 *  MMU base  ..... 9fff0000
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

/* MMU stuff - this could someday go into a different file
 */

/* Introduced 7-17-2016 while playing with
 * ARM timers and the ARM mmu
 */
#define MMU_SIZE (4*1024)

static void
mmu_scan ( unsigned long *mmubase )
{
	int i;

	/* There is a big zone (over RAM with 0x0c0e */
	for ( i=0; i<MMU_SIZE; i++ ) {
	    if ( (mmubase[i] & 0xffff) != 0x0c12 )
		printf ( "%4d: %08x: %08x\n", i, &mmubase[i], mmubase[i] );
	}
}

/* We are curious as to just what the state of things
 * is as handed to us by U-boot.
 */
void
mmu_status ( void )
{
	int scr;
	int mmu_base;

	/* SCTRL */
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (scr) : : "cc");

	if ( scr & 0x01 )
	    printf ( "MMU enabled\n" );
	if ( scr & 0x02 )
	    printf ( "A alignment enabled\n" );
	if ( scr & 0x04 )
	    printf ( "D cache enabled\n" );
	if ( scr & 0x1000 )
	    printf ( "I cache enabled\n" );
}

/* On the BBB this yields:
 *      SP = 8049ffd0
 * --SCTRL = 00c5187d
 * --TTBCR = 00000000
 * --TTBR0 = 9fff0000
 * --TTBR1 = 00000000
 * MMU base = 9fff0000
 */
void
mmu_show ( void )
{
	unsigned long *mmubase;
	unsigned long pu_base;
	unsigned long esp;
	unsigned long val;

#ifdef notdef
	/* We know this works */
	esp = get_sp();
	printf ( " SP = %08x\n", esp );
	esp = 0xBADBAD;

	/* was in locore.S */
	mmubase = (unsigned long *) get_mmu ();
#endif

	/* This works just fine !! */
	asm volatile ("add %0, sp, #0\n" :"=r"(esp));
	printf ( " SP = %08x\n", esp );

	/* SCTRL */
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (val) : : "cc");
	printf ( "SCTRL = %08x\n", val );

	/* TTBCR */
	asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(val) );
	printf ( "--TTBCR = %08x\n", val );

	/* TTBR0 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(mmubase) );
	printf ( "--TTBR0 = %08x\n", mmubase );

	/* TTBR1 */
	asm volatile ("mrc p15, 0, %0, c2, c0, 1" : "=r"(val) );
	printf ( "--TTBR1 = %08x\n", val );

	if ( ! mmubase )
	    printf ( "MMU not initialized !\n" );
	else
	    printf ( "MMU base = %08x\n", mmubase );

	mmu_status ();

	/* protection unit base */
	asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(pu_base) );

	if ( pu_base )
	    printf ( "Protection unit base = %08x\n", pu_base );

	if ( mmubase ) {
	    // mmu_scan ( mmubase );
	    // printf ( "mmu checking done\n" );
	}
}

/* THE END */
