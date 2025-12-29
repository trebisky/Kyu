/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * intcon_gic.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/19/2017
 *
 * Driver for the ARM GIC in the Allwinner H3
 *
 * The GIC is the ARM "Generic Interrupt Controller"
 * It has two sections, "cpu" and "dist"
 */

#include <arch/types.h>

#include "board.h"
#include "ints.h"

#define NUM_CONFIG	10
#define NUM_TARGET	40
#define NUM_PRIO	40
#define NUM_MASK	5

struct h3_gic_dist {
	vu32 ctrl;			/* 0x00 */
	vu32 type;			/* 0x04 */
	vu32 iidr;			/* 0x08 */
	u32 __pad0[61];
	vu32 eset[NUM_MASK];		/* BG - 0x100 */
	u32 __pad1[27];
	vu32 eclear[NUM_MASK];		/* BG - 0x180 */
	u32 __pad2[27];
	vu32 pset[NUM_MASK];		/* BG - 0x200 */
	u32 __pad3[27];
	vu32 pclear[NUM_MASK];		/* BG - 0x280 */
	u32 __pad4[27];
	vu32 aset[NUM_MASK];		/* BG - 0x300 */
	u32 __pad5[27];
	vu32 aclear[NUM_MASK];		/* BG - 0x300 */
	u32 __pad55[27];
	vu32 prio[NUM_PRIO];		/* BG - 0x400 */
	u32 __pad6[216];
	vu32 target[NUM_TARGET];	/* 0x800 */
	u32 __pad7[216];
	vu32 config[NUM_CONFIG];	/* 0xc00 */
	u32 __pad8[182];
	vu32 soft;			/* 0xf00 */
};

/* Bits in the Distributor CTRL register.
 * These enable forwarding from the distributor to the CPU section.
 */
#define	G0_ENABLE	0x01
#define	G1_ENABLE	0x02

struct h3_gic_cpu {
	vu32 ctrl;		/* 0x00 */
	vu32 pmr;		/* 0x04 */
	vu32 bpr;		/* 0x08 */
	vu32 iar;		/* 0x0c */
	vu32 eoi;		/* 0x10 */
	vu32 rpr;		/* 0x14 */
	vu32 hpr;		/* 0x18 */
	vu32 abpr;		/* 0x1c */
	vu32 aiar;		/* 0x20 */
	vu32 aeoir;		/* 0x24 */
	vu32 ahpr;		/* 0x28 */
	u32 __pad1[5];		/* 0x2C ... reserved*/
	u32 __pad2[36];		/* 0x40 ... imp. depend. */
	vu32 apr[4];		/* 0xD0 ... */
	vu32 nsapr[4];		/* 0xE0 ... */
	u32 __pad3[3];		/* reserved*/
	vu32 iidr;		/* 0xFC */
	u32 __pad4[960];	/* reserved*/
	vu32 dir;		/* 0x1000 */
};

/* Bits in the ctrl register */
/* G0 is enabled regardless and has no bit */
#define CPU_G1_ENABLE   0x01

#define GIC_DIST_BASE	((struct h3_gic_dist *) 0x01c81000)
#define GIC_CPU_BASE	((struct h3_gic_cpu *) 0x01c82000)

void
intcon_ena ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	u32 mask = 1 << (irq%32);

	gp->eset[x] = mask;
}

void
intcon_dis ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	u32 mask = 1 << (irq%32);

	gp->eclear[x] = mask;
}

static void
gic_unpend ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	u32 mask = 1 << (irq%32);

	gp->pclear[x] = mask;
}

#define SGI_LIST	0
#define SGI_ALL		(1<<24)
#define SGI_SELF	(2<<24)

/* Trigger a software generated interrupt (SGI)
 * "cpu" is the target core.
 */
void
gic_soft ( int sgi_num, int cpu )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;

	gp->soft = SGI_LIST | (1<<(16+cpu)) | sgi_num;
}

/* Trigger a software generated interrupt (SGI)
 *  to ourself
 */
void
gic_soft_self ( int sgi_num )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;

	gp->soft = SGI_SELF | sgi_num;
}

#ifdef notdef
void
gic_handler ( void )
{
	struct h3_gic_cpu *cp = GIC_CPU_BASE;
	int irq;

	irq = cp->iar;

	/* Do we need to EOI a spurious interrupt ? */
	if ( irq == 1023 ) {
	    return;
	}

	if ( irq == IRQ_TIMER0 )
	    timer_handler ( 0 );

	cp->eoi = irq;
	gic_unpend ( IRQ_TIMER0 );
}
#endif

int
intcon_irqwho ( void )
{
	struct h3_gic_cpu *cp = GIC_CPU_BASE;

	return cp->iar;
}

void
intcon_irqack ( int irq )
{
	struct h3_gic_cpu *cp = GIC_CPU_BASE;

	cp->eoi = irq;
	gic_unpend ( irq );
}

/* ----------------------------------- */

/* Initialize the "banked" registers that are unique to each core
 * This needs to be called by each core when it starts up.
 */
void
gic_cpu_init ( void )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	struct h3_gic_cpu *cp = GIC_CPU_BASE;
	int i;

	printf ( "GIC cpu_init called\n" );
	/* enable all SGI, disable all PPI */
	gp->eclear[0] = 0xffff0000;
	gp->eset[0]   = 0x0000ffff;

	/* priority for PPI and SGI */
	for ( i=0; i<8; i++ )
	    gp->prio[i] = 0xa0a0a0a0;

	cp->pmr = 0xf0;
	cp->ctrl = CPU_G1_ENABLE;
}

extern u32 vectors[2];

void
gic_init ( void )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int i;

	printf ( "GIC init called\n" );
	/* Initialize the distributor */
	gp->ctrl = 0;

	/* make all SPI level triggered */
	for ( i=2; i<NUM_CONFIG; i++ )
	    gp->config[i] = 0;

	for ( i=8; i<NUM_TARGET; i++ )
	    gp->target[i] = 0x01010101;

	for ( i=8; i<NUM_PRIO; i++ )
	    gp->prio[i] = 0xa0a0a0a0;

	for ( i=1; i<NUM_MASK; i++ )
	    gp->eclear[i] = 0xffffffff;

	for ( i=0; i<NUM_MASK; i++ )
	    gp->pclear[i] = 0xffffffff;

 	/* Group 0 are secure mode interrupts.
 	 * Group 1 are non-secure
 	 */
	// gp->ctrl = G0_ENABLE;
 	gp->ctrl = G0_ENABLE | G1_ENABLE;

	/* Initialize banked registers for core 0 */
	gic_cpu_init ();

#ifdef ARCH_ARMV8

	printf ( "GIC init, vectors at: %016lx\n", vectors );
	/* Added 12-12-2018, see ccu.c */
	ccu_gic_setup ( vectors );
#endif
}

/* ----------------------------------- */
/* Tests and diagnostics */

#ifdef notdef
/* Try to cause an exception
 * This scheme never worked.
 * The idea was go generated an exception
 * on an unaligned memory access.
 */
static void
intcon_test_fault1 ( void )
{
	reg_t sc;
	char *p;
	long *lp;
	int i;

	/* This does NOT yield a fault */
	printf ( "Try to provoke a fault 1\n" );
#ifdef notused
	/* This just locks things up.
	 * We really need to flush the cache first.
	 */
	// turn off D cache
	get_SCTLR(sc);
	sc &= ~4;
	set_SCTLR(sc);
	printf ( "D cache is now off\n" );
#endif
	p = (char *) 0x40000000;
	for ( i=0; i<8; i++ ) {
	    lp = (long *) p;
	    printf ( "Fetch long: %016x = %016x\n", lp, *lp );
	    p += 1;
	}

#ifdef notused
	// turn D cache back on
	get_SCTLR(sc);
	sc |= 4;
	set_SCTLR(sc);
#endif
}
#endif

/* Try to cause an exception
 * This definitely worked.
 */
static void
intcon_test_fault2 ( void )
{
	reg_t zero;

	/* This definitely yields a fault */
	printf ( "Try to provoke a fault 2\n" );
	/* Writing to a register at a higher EL should yield a fault */
	zero = 0;
	asm volatile ( "msr SCTLR_EL3, %0" : : "r" ( zero ) );
}

#define CORE_0  0
#define CORE_1  1
#define CORE_2  2
#define CORE_3  3

static void
sgi_handler_1 ( int xxx )
{
        printf ( "BANG!\n" );
}

static void
sgi_handler_2 ( int xxx )
{
        printf ( "BOOM!\n" );
}

static void
intcon_test_sgi ( void )
{
	irq_hookup ( IRQ_SGI_1, sgi_handler_1, 0 );
        irq_hookup ( IRQ_SGI_2, sgi_handler_2, 0 );

	printf ( "Try to interrupt ourself\n" );
        gic_soft_self ( SGI_1 );
        gic_soft ( SGI_2, CORE_0 );
	printf ( "Done\n" );
}

static void
intcon_check_iidr ( u32 val )
{
	if ( (val & 0xffff) == 0x143B )
	    printf ( "  Looks OK (0x143B indicates ARM implementer)\n" );
	else
	    printf ( "  Looks FISHY\n" );
}

/* Called from IO test menu */
void
intcon_test ( void )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	struct h3_gic_cpu *cp = GIC_CPU_BASE;
	int i;

	// gic_unbypass ();

	printf ( "GIC Type = %08x\n", gp->type );
	printf ( "Dist IIDR = %08x\n", gp->iidr );
	intcon_check_iidr ( gp->iidr );
	printf ( "CPU  IIDR = %08x\n", cp->iidr );
	intcon_check_iidr ( cp->iidr );

	/* XXX enable everything !! */
	printf ( "Enabling all IRQ !!\n" );
	for ( i=0; i < NUM_MASK; i++ ) {
	    gp->eset[i] = 0xffffffff;
	}

#ifdef notdef
#endif
	{
	struct h3_gic_dist *zgp = 0;
	struct h3_gic_cpu *zcp = 0;

	printf ( "addr of CPU IIDR = %08x\n", &zcp->iidr );
	printf ( "addr of CPU DIR = %08x\n", &zcp->dir );
	printf ( "addr of DIST SOFT = %08x\n", &zgp->soft );
	}

	/* same as my irqwho */
	/* returns 0x3ff = 1023 = spurious */
	printf ( "CPU IAR register: %08x\n", cp->iar );

	for ( i=0; i < NUM_MASK; i++ ) {
	    printf ( "Pending %d = %08x\n", i, gp->pset[i] );
	}

	printf ( "DIST soft register: %08x\n", gp->soft );

	intcon_test_sgi ();

	printf ( "DIST soft register: %08x\n", gp->soft );

	for ( i=0; i < NUM_MASK; i++ ) {
	    printf ( "Pending %d = %08x\n", i, gp->pset[i] );
	}
	printf ( "CPU IAR register: %08x\n", cp->iar );

	/* in ccu.c  - should not be needed */
	// gic_bypass ();

}
/* THE END */
