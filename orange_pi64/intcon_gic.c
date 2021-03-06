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
 *
 * A document is entitled "Corelink GIC-400 Generic
 *			Interrupt controller, r0p1, 2012" 59 pages.
 * This seems to be what you need to get started, but ...
 * Another document, "ARM Generic Interrupt Controller,
 *			Architecture specification, 2013", 214 pages
 * This document provides details the first does not.
 */
#include <arch/types.h>

#define NUM_ISR		16	/* 1 bit fields, 0x40 */
#define NUM_CONFIG	32	/* 2 bit fields, 0x80 */
#define NUM_PRIO	128	/* 8 bit fields, 0x200 */

struct h3_gic_dist {
	vu32 ctrl;			/* 0x00 */
	vu32 type;			/* 0x04 */
	vu32 iidr;			/* 0x08 */
	u32 		__pad0[61];

	/* enable */
	vu32 eset[NUM_ISR];		/* BG - 0x100 */
	u32 		__pad1[16];
	vu32 eclear[NUM_ISR];		/* BG - 0x180 */
	u32 		__pad2[16];

	/* pending */
	vu32 pset[NUM_ISR];		/* BG - 0x200 */
	u32 		__pad3[16];
	vu32 pclear[NUM_ISR];		/* BG - 0x280 */
	u32 		__pad4[16];

	/* active */
	vu32 aset[NUM_ISR];		/* BG - 0x300 */
	u32 		__pad5[16];
	vu32 aclear[NUM_ISR];		/* BG - 0x300 */
	u32 		__pad55[16];

	/* 8 bit fields */
	vu32 prio[NUM_PRIO];		/* BG - 0x400 */
	u32 		__pad6[128];

	/* 8 bit fields */
	vu32 target[NUM_PRIO];		/* 0x800 */
	u32 		__pad7[128];

	/* 2 bit fields */
	vu32 config[NUM_CONFIG];	/* 0xc00 */
	u32 		__pad77[32];

	/* single bits */
	vu32 isr[NUM_ISR];		/* 0xd00 */
	u32 		__pad8[112];

	vu32 soft;			/* 0xf00 */
};

/* In theory, a GIC can support up to 480 spi's,
 * but actual GIC are configured for less in most cases.
 * 480/32 = 15, adding a first word for ppi's
 * explains why NUM_ISRis 16.
 * The config registers give 2 bits per interrupt,
 * so we need 32 words of 32 bits to handle the 512
 * (512 once we add 480 spi + 32 ppi).
 *
 * The H3 uses interrupt numbers up to 157,
 *  skipping the first 32 (sgi and ppi)
 */

struct h3_gic_cpu {
	vu32 ctrl;		/* 0x00 */
	vu32 primask;		/* 0x04 */
	vu32 binpoint;		/* 0x08 */
	vu32 iack;		/* 0x0c */
	vu32 eoi;		/* 0x10 */
	vu32 run_pri;		/* 0x14 */
	vu32 high_pri;		/* 0x18 */
	/* -- */
        vu32 abpr;              /* 0x1c */
        vu32 aiar;              /* 0x20 */
        vu32 aeoir;             /* 0x24 */
        vu32 ahpr;              /* 0x28 */
        u32 __pad1[5];          /* 0x2C ... reserved*/
        u32 __pad2[36];         /* 0x40 ... imp. depend. */
        vu32 apr[4];            /* 0xD0 ... */
        vu32 nsapr[4];          /* 0xE0 ... */
        u32 __pad3[3];          /* reserved*/
        vu32 iidr;              /* 0xFC */
        u32 __pad4[960];        /* reserved*/
        vu32 dir;               /* 0x1000 */
};

#ifdef notdef
struct h3_gic_cpu {
        vu32 ctrl;              /* 0x00 */
        vu32 pmr;               /* 0x04 */
        vu32 bpr;               /* 0x08 */
        vu32 iar;               /* 0x0c */
        vu32 eoi;               /* 0x10 */
        vu32 rpr;               /* 0x14 */
        vu32 hpr;               /* 0x18 */
        vu32 abpr;              /* 0x1c */
        vu32 aiar;              /* 0x20 */
        vu32 aeoir;             /* 0x24 */
        vu32 ahpr;              /* 0x28 */
        u32 __pad1[5];          /* 0x2C ... reserved*/
        u32 __pad2[36];         /* 0x40 ... imp. depend. */
        vu32 apr[4];            /* 0xD0 ... */
        vu32 nsapr[4];          /* 0xE0 ... */
        u32 __pad3[3];          /* reserved*/
        vu32 iidr;              /* 0xFC */
        u32 __pad4[960];        /* reserved*/
        vu32 dir;               /* 0x1000 */
};
#endif


#define GIC_DIST_BASE	((struct h3_gic_dist *) 0x01c81000)
#define GIC_CPU_BASE	((struct h3_gic_cpu *) 0x01c82000)

#define	G0_ENABLE	0x01
#define	G1_ENABLE	0x02

static int gic_initialized = 0;

static void
gic_setter ( vu32 *addr, u32 mask )
{
	int i;
	u32 val;

	for ( i=0; i<16; i++ ) {
	    *addr = mask;
	    val = *addr;
	    if ( val & mask )
		break;
	}

	if ( *addr & mask )
	    printf ( "gic set good after %d\n", i );
	else
	    printf ( "gic set FAIL after %d (%08x)\n", i, *addr );
}

void
intcon_ena ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	u32 mask = 1 << (irq%32);

	/* Avoid bugs when we initialize hardware prior
	 * to initializing the GIC.
	 */
	if ( ! gic_initialized )
	    panic ( "GIC not yet initialized" );

	printf ( "GIC dist base: %08x\n", gp );
	printf ( "GIC enable IRQ %d e[%d] = %08x\n", irq, x, mask );
	show_gic_bypass ();

	// gp->eset[x] = mask;
	gic_setter ( &gp->eset[x], mask );
}

void
intcon_dis ( int irq )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int x = irq / 32;
	u32 mask = 1 << (irq%32);

	printf ( "GIC disable IRQ %d e[%d] = %08x\n", irq, x, mask );

	gp->eclear[x] = mask;
	gic_setter ( &gp->eclear[x], mask );
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

	printf ( "GIC dist base: %08x\n", gp );
	printf ( "GIC cpu base: %08x\n", cp );

	/* disable and clear all PPI and SGI */
	gp->eclear[0] = 0xffffffff;
	gp->pclear[0] = 0xffffffff;

	/* enable all SGI */
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

	gic_unbypass ();

	gic_initialized = 1;

	/* Initialize the distributor */
	gp->ctrl = 0;

	/* make all SPI level triggered */
	for ( i=2; i<NUM_CONFIG; i++ )
	    gp->config[i] = 0;

	/* 8 bit fields */
	for ( i=8; i<NUM_PRIO; i++ )
	    gp->target[i] = 0x01010101;

	/* 8 bit fields */
	for ( i=8; i<NUM_PRIO; i++ )
	    gp->prio[i] = 0xa0a0a0a0;

	/* clear all enables */
	for ( i=1; i<NUM_ISR; i++ )
	    gp->eclear[i] = 0xffffffff;

	/* clear all pending bits */
	for ( i=0; i<NUM_ISR; i++ )
	    gp->pclear[i] = 0xffffffff;

	gp->ctrl = G0_ENABLE;

	/* Initialize banked registers for core 0 */
	gic_cpu_init ();
}

/* While working on the TWI drive, the ISR display
 * had these values.  1 is IRQ 32 (the uart).
 * 0x40 is IRQ 38 -- the TWI !!
 *
 * GIC isr[0] = 00000000
 * GIC isr[1] = 00000041
 */

void
gic_show ( void )
{
	struct h3_gic_dist *gp = GIC_DIST_BASE;
	int i;

	printf ( "GIC isr at %08x\n", gp->isr );
	printf ( "GIC soft at %08x\n", &gp->soft );
	for ( i=0; i<NUM_ISR; i++ )
	    printf ( "GIC isr[%d] = %08x\n", i, gp->isr[i] );
	for ( i=0; i<NUM_ISR; i++ )
	    printf ( "GIC eset[%d] = %08x\n", i, gp->eset[i] );
	for ( i=0; i<NUM_ISR; i++ )
	    printf ( "GIC eclear[%d] = %08x\n", i, gp->eclear[i] );
}

/* Called from IO test menu */
void
intcon_test ( void )
{
        struct h3_gic_dist *gp = GIC_DIST_BASE;
        struct h3_gic_cpu *cp = GIC_CPU_BASE;
        int i;

        // gic_unbypass ();
	show_gic_bypass ();

        printf ( "GIC Type = %08x\n", gp->type );
        printf ( "Dist IIDR = %08x\n", gp->iidr );
        //intcon_check_iidr ( gp->iidr );
        //printf ( "CPU  IIDR = %08x\n", cp->iidr );
        //intcon_check_iidr ( cp->iidr );

#ifdef notdef
        /* XXX enable everything !! */
        printf ( "Enabling all IRQ !!\n" );
        for ( i=0; i < NUM_MASK; i++ ) {
            gp->eset[i] = 0xffffffff;
        }
#endif
	gic_show ();
}

/* THE END */
