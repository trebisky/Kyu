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

struct h3_ccu {
        vu32 pll_cpux_ctrl;	/* 00 */
	u32 __pad0[3];
	u32 __pad1[16];
        vu32 cpux_ax1_cfg;	/* 50 */
};

#define CCU_BASE (struct h3_ccu *) 0x01c20000

/* -------------- */

#define F_24M	(24 * 1000 * 1000)

/* With the 32 bit ARM compiler, int and long are both 4 byte items.
 * With the 64 bit ARM compiler, int is 4 byte, long is 8 byte.
 */

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

struct h3_cpu_config {
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
	} rvb[4];		/* A0 */
};

#define CPU_CONFIG_BASE (struct h3_cpu_config *) 0x01700000

#define GIC_BYPASS	0x10

void
gic_bypass ( void )
{
	struct h3_cpu_config *ccp = CPU_CONFIG_BASE;
	struct h3_cpu_config *zcp = 0;

	printf ( "TJT gic bypass, gctrl = %08x\n", &zcp->gctrl );
	printf ( "TJT gic bypass, rvb = %08x\n", &zcp->rvb[0] );

	ccp->gctrl = GIC_BYPASS;
}

/* THE END */
