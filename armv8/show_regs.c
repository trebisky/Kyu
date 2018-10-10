/*
 * Copyright (C) 2018  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * Taken from U-boot /arch/arm/lib/interrupts.c
 * and trimmed for use in Kyu
 * 4-30-2015  U-boot 2015.01
 *
 * TOTALLY bogus for armv8
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include "types.h"
#include "cpu.h"

struct thread *cur_thread;

#define DUMP_BYTES	512
// #define LONG_LIMIT	1024
// #define LONG_LIMIT	256
#define LONG_LIMIT	320

void
show_stack ( unsigned long sp, int max )
{
	unsigned long start;
	unsigned long end;
	int num;

	// printf ( "SHOW stack, %08x, %d\n", sp, max );
	start = sp & ~0xf;

	// I don't know what I was thinking here
	// end = (sp & ~0xfff) + DUMP_BYTES;
	end = sp + DUMP_BYTES;

	num = (end-start)/sizeof(long);

	if ( num < 0 ) num = 4;

	if ( num > LONG_LIMIT )
	    num = LONG_LIMIT;

	if ( max > 0 && num > max )
	    num = max;

	// printf ( "SHOW stack, %08x, %d\n", start, num );

	printf ( "\n" );
	dump_ln ( (void *) start, num );
	printf ( "\n" );
}


void
show_regs ( reg_t *regs )
{
	int i, r;

	/*
	reg_t elr, sp;
	asm volatile ( "mrs %0, elr_el2" : "=r" ( elr ) );
	printf ( "ELR:     %lx\n", elr );
	asm volatile ( "add %0, sp, #0" : "=r" ( sp ) );
	printf ( "SP:      %lx\n", sp );
	*/

	printf ( "SP:      %lx\n", regs[ARMV8_SP] );
        printf ( "LR:      %lx\n", regs[ARMV8_LR] );
	printf ( "ELR:     %lx\n", regs[ARMV8_ELR] );
	printf ( "SPSR:    %lx\n", regs[ARMV8_SPSR] );

	printf("\n");

        for (i = 0; i < 29; i += 2) {
	    r = ARMV8_X0 + i;
	    printf("x%-2d: %016lx x%-2d: %016lx\n",
		   i, regs[r], i+1, regs[r+1]);
	}

	// x30 is the LR, but we show it here also
	i = 30; 
	r = ARMV8_X0 + i;
	printf("x%-2d: %016lx\n", i, regs[r] );

	printf("\n");
}


void
show_thread_regs ( struct thread *tp, int stack_len )
{
	reg_t *regs;

	regs = (reg_t *) tp->iregs.regs;

	printf ( "\n" );
	show_regs ( regs );

	if ( stack_len )
	    show_stack ( regs[ARMV8_SP], stack_len );
}

void
show_my_regs ( void )
{
	reg_t	regs[NUM_IREGS];

	get_regs ( regs );

	printf ( "\n" );

	show_regs ( regs );
	show_stack ( regs[ARMV8_SP], 80 );
}

/* THE END */
