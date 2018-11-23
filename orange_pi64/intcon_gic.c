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

struct h3_gic_cpu {
	vu32 ctrl;		/* 0x00 */
	vu32 primask;		/* 0x04 */
	vu32 binpoint;		/* 0x08 */
	vu32 iack;		/* 0x0c */
	vu32 eoi;		/* 0x10 */
	vu32 run_pri;		/* 0x14 */
	vu32 high_pri;		/* 0x18 */
};

#define GIC_DIST_BASE	((struct h3_gic_dist *) 0x01c81000)
#define GIC_CPU_BASE	((struct h3_gic_cpu *) 0x01c82000)

#define	G0_ENABLE	0x01
#define	G1_ENABLE	0x02

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

	irq = cp->iack;

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

	return cp->iack;
}

void
intcon_irqack ( int irq )
{
	struct h3_gic_cpu *cp = GIC_CPU_BASE;

	cp->eoi = irq;
	gic_unpend ( irq );
}

#ifdef notdef
XXX - for some diagnostic, never written
void
intcon_check ( void )
{
	struct h3_gic_cpu *cp = GIC_CPU_BASE;
}
#endif

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

	cp->primask = 0xf0;
	cp->ctrl = 1;
}

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

	gp->ctrl = G0_ENABLE;

	/* Initialize banked registers for core 0 */
	gic_cpu_init ();
}

/* THE END */
