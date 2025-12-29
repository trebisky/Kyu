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

#define BIT(nr)		(1<<(nr))

/* With the 32 bit ARM compiler, int and long are both 4 byte items.
 * With the 64 bit ARM compiler, int is 4 byte, long is 8 byte.
 */

struct h3_ccu {
        vu32 	pll_cpux_ctrl;   /* 00 */
        u32 	__pad00;
        vu32 	pll2;		 /* 08 */
        u32 	__pad01;
        vu32 	pll3;		 /* 10 */
        u32 	__pad02;
        vu32 	pll4;		 /* 18 */
        u32 	__pad03;
        vu32 	pll5;		 /* 20 */
        u32 	__pad04;
        vu32 	pll6;		 /* 28 - periph0 */
        u32 	__pad05;
        vu32 	pll7;		 /* 30 */
        u32 	__pad06;
        vu32 	pll8;		 /* 38 */
        u32 	__pad07;
        vu32 	mipi_pll;	 /* 40 */
        vu32	pll9;	 	 /* 44 - periph1 */
        vu32 	pll10;		 /* 48 */
        vu32 	pll11;		 /* 4c */

        vu32 	cpux_ax1_cfg;    /* 50 */
        vu32 	ahb1_apb1_div;   /* 54 */
        vu32 	apb2_div;        /* 58 */
        vu32 	ahb2_cfg;        /* 5c */

        vu32 	gate0;           /* 60 */
        vu32 	gate1;           /* 64 */
        vu32 	gate2;           /* 68 */
        vu32 	gate3;           /* 6c */
        vu32 	gate4;           /* 70 */
        u32 	__pad3[147];
        vu32 	reset0;          /* 2C0 */
        vu32 	reset1;          /* 2C4 */
        vu32 	reset2;          /* 2C8 */
	u32	__pad4;
        vu32 	reset3;          /* 2D0 */
	u32	__pad5;
        vu32 	reset4;          /* 2D8 */
};

#define CCU_BASE (struct h3_ccu *) 0x01c20000

/* Uart gate and reset bits.
 * See datasheet section 4.3
 *  gating in 4.3.5.17
 *  reset in 4.3.5.74
 */
#define UART0_BIT	0x00010000
#define UART1_BIT	0x00020000
#define UART2_BIT	0x00040000
#define UART3_BIT	0x00080000

#define ALL_UART        (UART0_BIT | UART1_BIT | UART2_BIT | UART3_BIT)

#define PIO_BIT		0x20

/* -------------- */

static void xshow_reg ( char *m, void *addr )
{
	printf ( "show (%s) : %016lx\n", m, addr );
}

static void ccu_show ( void )
{
	struct h3_ccu *ccp = CCU_BASE;

	printf ( "\n" );
	show_reg ( "CCU pll6 (periph0)", &ccp->pll6 );
	show_reg ( "CCU pll9 (periph1)", &ccp->pll9 );
	printf ( "\n" );

	show_reg ( "CCU ax1", &ccp->cpux_ax1_cfg );
	show_reg ( "CCU ahb1_apb1_div", &ccp->ahb1_apb1_div );
	show_reg ( "CCU apb2_div", &ccp->apb2_div );
	show_reg ( "CCU ahb2_cfg", &ccp->ahb2_cfg );
	printf ( "\n" );
}

#define PLL_ENABLE	0x80000000
#define PLL_LOCK	0x10000000

/* Called from board.c */
void
ccu_init ( void )
{
	struct h3_ccu *ccp = CCU_BASE;

	ccu_show ();

	ccp->pll9 |= PLL_ENABLE;
	while ( ! (ccp->pll9 & PLL_LOCK) )
	    ;

	ccu_show ();

	ccp->gate3 |= ALL_UART;
	show_reg ("CCU gate3: ", &ccp->gate3 );

	ccp->reset4 |= ALL_UART;;
	show_reg ("CCU reset4: ", &ccp->reset4 );

	ccp->gate2 |= PIO_BIT;
	show_reg ("CCU gate2 (PIO) : ", &ccp->gate2 );
	// PIO has no reset ??
	// ccp->reset3 |= PIO_BIT;;

	reset_release_all ();
}

#ifdef notdef
/* These are registers in the CCM (clock control module)
 */
#define CCM_GATE	((u32 *) 0x01c2006c)
#define CCM_RESET4	((u32 *) 0x01c202d8)

#define GATE_UART0	0x00010000
#define GATE_UART1	0x00020000
#define GATE_UART2	0x00040000
#define GATE_UART3	0x00080000

#define RESET4_UART0	0x00010000
#define RESET4_UART1	0x00020000
#define RESET4_UART2	0x00040000
#define RESET4_UART3	0x00080000

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
#endif

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

#ifdef notdef
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
#endif

void
gpio_clocks_on ( void )
{
}

/* There is another CCU for all of the "R" peripherals.
 * It is labelled R_PRCM in the documents, but essentially
 * nothing about it is documented.
 *
 * Only by looking at linux source, in particular
 *  sunxi-ng/ccu-sun8i-r.c do we learn about it.
 */

#define R_CCU_BASE	0x01f01400

struct r_ccu {
	vu32		cfg_reg;		/* 00 */
	u32		  _pad1[2];
	vu32		apb0_clk_div_reg;	/* 0c */
	vu32		cpu1_en;		/* 10 */
	vu32		cpu2_en;		/* 14 */
	vu32		cpu3_en;		/* 18 */
	vu32		cpu4_en;		/* 1c */
	u32		  _pad2[2];		/* 20 */
	vu32		apb0_gate;		/* 28 */
	u32		  _pad3[5];		/* 2c */
	vu32		pll_ctrl0;		/* 40 */
	vu32		pll_ctrl1;		/* 44 */
	u32		  _pad4[2];		/* 48 */
	vu32		ow_clk;			/* 50 */
	vu32		cir_clk;		/* 54 */
	u32		  _pad5[3];		/* 58 */
	vu32		cpu1_pwr_clamp_status;	/* 64 */
	u32		  _pad6[15];		/* 68 */
	vu32		cpu2_pwr_clamp_status;	/* a4 */
	u32		  _pad7[2];		/* a8 */
	vu32		apb0_reset;		/* b0 */
};

#define R_GATE_REG	0x28

/* #define R_PIO_GATE	0x0001		 bit(0) */
#define R_PIO_BIT	BIT(0)
#define R_IR_BIT	BIT(1)

static void
r_ccu_show ( void )
{
	struct r_ccu *rcp = (struct r_ccu *) R_CCU_BASE;

	show_reg ( "R_CCU cfg: ", &rcp->cfg_reg );
	show_reg ( "R_CCU clk_div: ", &rcp->apb0_clk_div_reg );
	show_reg ( "R_CCU gate: ", &rcp->apb0_gate );
	show_reg ( "R_CCU reset: ", &rcp->apb0_reset );
}

/* Called from board.c */
void
r_ccu_init ( void )
{
	struct r_ccu *rcp = (struct r_ccu *) R_CCU_BASE;

	r_ccu_show ();

	rcp->apb0_gate |= R_PIO_BIT;
	rcp->apb0_reset |= R_PIO_BIT;

	r_ccu_show ();

	// dump SRAM A2
	// count is lines on screen
	// dump_l ( 0x44000, 64 * 1024 / 16 );

	// All indications are that this is getting loaded with
	// something repeatable on each reboot.
	// 00044000  94000003 d2a94000 d61f0000 d2800600 
	// 00044010  b2760000 b27e0000 b2400000 d51e1100 

	dump_l ( 0x44000, 4 );

	// When we do this, we continue running, but
	// get into trouble on a watchdog reset.
	// memset ( 0x44000, 0, 64*1024 );

#ifdef notdef
        vu32 *lp;
        /* release reset
         */
        lp = (vu32 *) (R_CCU_BASE + R_GATE_REG);
        *lp |= R_PIO_GATE;
        *lp |= R_IR_GATE;
#endif
	
}

/* THE END */
