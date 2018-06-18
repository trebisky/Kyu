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

#define NUM_CONFIG	10
#define NUM_TARGET	40
#define NUM_PRIO	40
#define NUM_MASK	5

struct h3_gic_dist {
	volatile unsigned long ctrl;		/* 0x00 */
	volatile unsigned long type;		/* 0x04 */
	volatile unsigned long iidr;		/* 0x08 */
	int __pad0[61];
	volatile unsigned long eset[NUM_MASK];		/* BG - 0x100 */
	int __pad1[27];
	volatile unsigned long eclear[NUM_MASK];	/* BG - 0x180 */
	int __pad2[27];
	volatile unsigned long pset[NUM_MASK];		/* BG - 0x200 */
	int __pad3[27];
	volatile unsigned long pclear[NUM_MASK];	/* BG - 0x280 */
	int __pad4[27];
	volatile unsigned long aset[NUM_MASK];		/* BG - 0x300 */
	int __pad5[27];
	volatile unsigned long aclear[NUM_MASK];	/* BG - 0x300 */
	int __pad55[27];
	volatile unsigned long prio[NUM_PRIO];		/* BG - 0x400 */
	int __pad6[216];
	volatile unsigned long target[NUM_TARGET];	/* 0x800 */
	int __pad7[216];
	volatile unsigned long config[NUM_CONFIG];	/* 0xc00 */
	int __pad8[182];
	volatile unsigned long soft;			/* 0xf00 */
};

struct h3_gic_cpu {
	volatile unsigned long ctrl;		/* 0x00 */
	volatile unsigned long primask;		/* 0x04 */
	volatile unsigned long binpoint;	/* 0x08 */
	volatile unsigned long iack;		/* 0x0c */
	volatile unsigned long eoi;		/* 0x10 */
	volatile unsigned long run_pri;		/* 0x14 */
	volatile unsigned long high_pri;	/* 0x18 */
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
	unsigned long mask = 1 << (irq%32);

	gp->eset[x] = mask;
}

void
intcon_dis ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	unsigned long mask = 1 << (irq%32);

	gp->eclear[x] = mask;
}

static void
gic_unpend ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	unsigned long mask = 1 << (irq%32);

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

/* Initialize the "banked" registers that are unique to each core
 * This needs to be called by each core when it starts up.
 */
void
gic_cpu_init ( void )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	struct h3_gic_cpu *cp = GIC_CPU_BASE;
	int i;

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
