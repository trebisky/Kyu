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
        volatile unsigned int pll_cpux_ctrl;	/* 00 */
	int __pad0[3];
	int __pad1[16];
        volatile unsigned int cpux_ax1_cfg;	/* 50 */
	int __pad2[3];
        volatile unsigned int gate0;		/* 60 */
        volatile unsigned int gate1;		/* 64 */
        volatile unsigned int gate2;		/* 68 */
        volatile unsigned int gate3;		/* 6c */
};

#define CCU_BASE (struct h3_ccu *) 0x01c20000

#define F_24M	(24 * 1000 * 1000)

/* bits in the pll_cpux_ctrl register */
#define CPUX_PLL_ENABLE		0x80000000
#define CPUX_PLL_LOCK		0x10000000
#define CPUX_SDM_ENA		1<<24	/* no idea what this is, leave it 0 */

/* ==================================================== */
/* ==================================================== */

void
set_cpu_clock_1008 ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	int clock = 1008;
	int new_n, new_k, new_pll;
	int tmo;

	new_n = clock / 24;
	new_k = 1;
	if ( new_n > 32 ) {
	    new_n /= 2;
	    new_k = 2;
	}

	new_pll = CPUX_PLL_ENABLE
	    | (new_n-1) << 8
	    | (new_k-1) << 4;

	cp->pll_cpux_ctrl = new_pll;
	printf ( "new CPU pll_cpux_ctrl = %08x\n", cp->pll_cpux_ctrl );

	for ( tmo = 1000; tmo; tmo-- )
	    if ( cp->pll_cpux_ctrl & CPUX_PLL_LOCK )
		break;

	// This reports 0, which means the delay caused by the printf
	// above to show the PLL reg value is plenty.
	// printf ( "new CPU pll timeout counter = %d\n", 1000 - tmo );

	if ( tmo == 0 )
	    printf ( "new CPU setting failed (timed out)\n" );
}

/* Return CPU clock rate in Hz */
unsigned int
get_cpu_clock ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	int source;
	unsigned int val;
	int n, k, m;
	unsigned int freq;

	printf ( "CCU cpux_ax1_cfg = %08x\n", cp->cpux_ax1_cfg );
	printf ( "CCU pll_cpux_ctrl = %08x\n", cp->pll_cpux_ctrl );

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

	printf ( "CCU n, k, m = %d %d %d\n", n, k, m );

	printf ( "clock source: %d\n", source );

	// printf ( "PLL %08x\n", val );
	printf ( "CPU clock: %d\n", freq );
	return freq;
}

/* XXX - need to gather other CCU related stuff like this
 * that is scattered around in other modules.
 * Start with spinlocks.c
 */

#define TWI0_BIT	0x01
#define TWI1_BIT	0x02
#define TWI2_BIT	0x04

// #define ALL_TWI		(TWI0_BIT | TWI1_BIT)
#define ALL_TWI		(TWI0_BIT | TWI1_BIT | TWI2_BIT)

/* This is part of the above, but ...
 * XXX I am too lazy to calculate the pad values to
 * stick it into the above structure.
 * (Not only that, but this is less error prone).
 */
#define CCU_BASE_VAL 0x01c20000
#define BUS_RESET_REG4	0x2D8

/* We only fire up 0 and 1 since only they are routed to the
 * outside world.
 */
void
twi_clocks_on ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	volatile unsigned int *lp;

	/* Turn on clock
	 */
	// show_reg ( "TWI gate reg:", &cp->gate3 );
	cp->gate3 |= ALL_TWI;
	// show_reg ( "TWI gate reg:", &cp->gate3 );

	/* release reset
	 */
        lp = (volatile unsigned int *) (CCU_BASE_VAL + BUS_RESET_REG4);
	// show_reg ( "TWI reset reg:", lp );
        *lp |= ALL_TWI;
	// show_reg ( "TWI reset reg:", lp );
}

#define UART0_BIT	0x10000
#define UART1_BIT	0x20000
#define UART2_BIT	0x40000
#define UART3_BIT	0x80000

#define ALL_UART	(UART0_BIT | UART1_BIT | UART2_BIT | UART3_BIT)

/* See datasheet section 4.3
 *  gating in 4.3.5.17
 *  reset in 4.3.5.74
 */
void
serial_clocks_on ( void )
{
	struct h3_ccu *cp = CCU_BASE;
	volatile unsigned int *lp;

	/* Turn on clock */
	cp->gate3 |= ALL_UART;

	/* release reset */
        lp = (volatile unsigned int *) (CCU_BASE_VAL + BUS_RESET_REG4);
        *lp |= ALL_UART;
}

/* These are registers in the CCM (clock control module)
 */
#define CCM_GATE	((unsigned int *) 0x01c2006c)
#define CCM_RESET4	((unsigned int *) 0x01c202d8)

#define GATE_UART0	0x00010000
#define GATE_UART1	0x00020000
#define GATE_UART2	0x00040000
#define GATE_UART3	0x00080000

#define RESET4_UART0	0x00010000
#define RESET4_UART1	0x00020000
#define RESET4_UART2	0x00040000
#define RESET4_UART3	0x00080000

/* This is probably set up for us by U-boot,
 * true bare metal would need this.
 * XXX do we want this routine, or the above ??
 */
void
uart_clock_init ( int uart )
{
	if ( uart == 0 ) {
	    *CCM_GATE |= GATE_UART0;
	    *CCM_RESET4 |= RESET4_UART0;
	} else if ( uart == 1 ) {
	    *CCM_GATE |= GATE_UART1;
	    *CCM_RESET4 |= RESET4_UART1;
	} else if ( uart == 2 ) {
	    *CCM_GATE |= GATE_UART2;
	    *CCM_RESET4 |= RESET4_UART2;
	} else if ( uart == 3 ) {
	    *CCM_GATE |= GATE_UART3;
	    *CCM_RESET4 |= RESET4_UART3;
	}
}

void
gpio_clocks_on ( void )
{
}

/* THE END */
