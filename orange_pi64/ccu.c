/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ccu.c for the Orange Pi PC and PC Plus
 *
 * CCU is the "Clock Control Unit"
 *  (also known as the CCM = clock control module)
 *
 * Tom Trebisky  6/8/2018
 *
 */
#include <arch/types.h>

/* With the 32 bit ARM compiler, int and long are both 4 byte items.
 * With the 64 bit ARM compiler, int is 4 byte, long is 8 byte.
 */

struct h3_ccu {
        vu32 pll_cpux_ctrl;   /* 00 */
        u32 __pad0[3];
        u32 __pad1[16];
        vu32 cpux_ax1_cfg;    /* 50 */
        u32 __pad2[3];
        vu32 gate0;           /* 60 */
        vu32 gate1;           /* 64 */
        vu32 gate2;           /* 68 */
        vu32 gate3;           /* 6c */
};

#define CCU_BASE (struct h3_ccu *) 0x01c20000

/* -------------- */

#define F_24M	(24 * 1000 * 1000)

/* Return CPU clock rate in Hz */
u32
get_cpu_clock ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	int source;
	unsigned int val;
	int n, k, m;
	u32 freq;

	// printf ( "CCU %08x\n", &cp->cpux_ax1_cfg );

	source = (cp->cpux_ax1_cfg >> 16) & 0x3;
	if ( source == 0 )
	    return 32768;		/* LOSC */
	if ( source == 1 )
	    return F_24M;		/* OSC24M */

	/* the usual case is "source = 2" where the PLL yields the clock */
	/* I see 0x90001b21 for the PLL register, so
	 *  n = 0x1b + 1 = 28, k = 2 + 1 = 3, m = 1 + 1 = 2;
	 *  freq = 1008000000 = 24M * 42
	 */
	val = cp->pll_cpux_ctrl;
	n = ((val>>8) & 0x1f) + 1;
	k = ((val>>4) & 0x03) + 1;
	m = (val & 0x3) + 1;
	freq = F_24M * n * k / m;

	// printf ( "sizeof int: %d\n", sizeof(val) );
	// printf ( "sizeof long: %d\n", sizeof(freq) );
	// printf ( "clock source: %d\n", source );
	printf ( "PLL %08x\n", val );
	printf ( "CPU clock: %d\n", freq );
	return freq;
}

/* -------------- */

/* See section 4.4 in H5 user manual */

#define NUM_H5_CPU	4

struct h5_cpu_config {
        vu32 cluster0;		/* 00 */
        vu32 cluster1;		/* 04 */
        vu32 cache0;		/* 08 */
        vu32 cache1;		/* 0c */
	int __pad0[6];

        vu32 gctrl;		/* 28 */
	int __pad1;

        vu32 status;		/* 30 */
	int __pad2[2];

        vu32 l2_status;		/* 3c */
	int __pad3[17];

        vu32 reset;		/* 80 */
	int __pad4[7];

	struct {
	    vu32  lo;
	    vu32  hi;
	} vbase[NUM_H5_CPU];		/* A0 */
};

#define CPU_CONFIG_BASE (struct h5_cpu_config *) 0x01700000

#define GIC_BYPASS	0x10

void
show_gic_bypass ( void )
{
	struct h5_cpu_config *ccp = CPU_CONFIG_BASE;

	show_reg ("GIC bypass currently is: ", &ccp->gctrl );
}

/* page 156 of H5 Manual, "set this bit to disable the GIC
 * and route external signals directly to the processor.
 */
void
gic_bypass ( void )
{
	struct h5_cpu_config *ccp = CPU_CONFIG_BASE;
	struct h5_cpu_config *zcp = 0;

	printf ( "TJT gic bypass, addr of vbase[0] = %08x\n", &zcp->vbase[0] );
	printf ( "TJT gic bypass, addr of gctrl = %08x\n", &zcp->gctrl );
	printf ( "TJT gic bypass, gctrl = %08x\n", ccp->gctrl );

	ccp->gctrl = GIC_BYPASS;

	printf ( "TJT gic bypass, gctrl = %08x\n", ccp->gctrl );
}

void
gic_unbypass ( void )
{
	struct h5_cpu_config *ccp = CPU_CONFIG_BASE;

	printf ( "TJT gic bypass, gctrl = %08x\n", ccp->gctrl );

	ccp->gctrl &= ~GIC_BYPASS;

	printf ( "TJT gic bypass, gctrl = %08x\n", ccp->gctrl );
}

void
ccu_gic_setup ( u32 vecbase )
{
	struct h5_cpu_config *ccp = CPU_CONFIG_BASE;
	int cpu;

	for ( cpu=0; cpu<NUM_H5_CPU; cpu++ ) {
	    ccp->vbase[cpu].lo = vecbase;
	    ccp->vbase[cpu].hi = 0;

	}

	/* unbypass the GIC */
	ccp->gctrl &= ~GIC_BYPASS;
}

/* XXX - need to gather other CCU related stuff like this
 * that is scattered around in other modules.
 * Start with spinlocks.c
 */

#define TWI0_BIT        0x01
#define TWI1_BIT        0x02
#define TWI2_BIT        0x04

// #define ALL_TWI              (TWI0_BIT | TWI1_BIT)
#define ALL_TWI         (TWI0_BIT | TWI1_BIT | TWI2_BIT)

/* This is part of the above, but ...
 * XXX I am too lazy to calculate the pad values to
 * stick it into the above structure.
 * (Not only that, but this is less error prone).
 */
#define CCU_BASE_VAL 0x01c20000
#define BUS_RESET_REG4  0x2D8

/* We only fire up 0 and 1 since only they are routed to the
 * outside world.
 */
void
twi_clocks_on ( void )
{
        struct h3_ccu *cp = CCU_BASE;
        vu32 *lp;

        /* Turn on clock
         */
        // show_reg ( "TWI gate reg:", &cp->gate3 );
        cp->gate3 |= ALL_TWI;
        // show_reg ( "TWI gate reg:", &cp->gate3 );

        /* release reset
         */
        lp = (vu32 *) (CCU_BASE_VAL + BUS_RESET_REG4);
        // show_reg ( "TWI reset reg:", lp );
        *lp |= ALL_TWI;
        // show_reg ( "TWI reset reg:", lp );
}

/* Simple copied from H3 code */
#define UART0_BIT       0x10000
#define UART1_BIT       0x20000
#define UART2_BIT       0x40000
#define UART3_BIT       0x80000

#define ALL_UART        (UART0_BIT | UART1_BIT | UART2_BIT | UART3_BIT)

/* See datasheet section 4.3
 *  gating in 4.3.5.17
 *  reset in 4.3.5.74
 */
void
serial_clocks_on ( void )
{
        struct h3_ccu *cp = CCU_BASE;
        vu32 *lp;

        /* Turn on clock */
        cp->gate3 |= ALL_UART;

        /* release reset */
        lp = (vu32 *) (CCU_BASE_VAL + BUS_RESET_REG4);
        *lp |= ALL_UART;
}

/* THE END */
