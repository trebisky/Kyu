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

struct h3_ccu {
        volatile unsigned long pll_cpux_ctrl;	/* 00 */
	int __pad0[3];
	int __pad1[16];
        volatile unsigned long cpux_ax1_cfg;	/* 50 */
};

#define CCU_BASE (struct h3_ccu *) 0x01c20000

#define F_24M	(24 * 1000 * 1000)

/* With this ARM compiler, int and long are both 4 byte items
 */

/* Return CPU clock rate in Hz */
unsigned long
get_cpu_clock ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	int source;
	unsigned int val;
	int n, k, m;
	unsigned long freq;

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

/* THE END */
