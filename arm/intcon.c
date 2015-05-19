/* intcon.c
 *
 * Driver for the am3359 interrupt controller
 *   see TRM pages 212 and therabouts
 * 
 */
#include "omap_ints.h"

#define INTCON_BASE      0x48200000

#define getb(a)          (*(volatile unsigned char *)(a))
#define getw(a)          (*(volatile unsigned short *)(a))
#define getl(a)          (*(volatile unsigned int *)(a))

#define putb(v,a)        (*(volatile unsigned char *)(a) = (v))
#define putw(v,a)        (*(volatile unsigned short *)(a) = (v))
#define putl(v,a)        (*(volatile unsigned int *)(a) = (v))

struct intmask {
	volatile unsigned long itr;	/* 00 */
	volatile unsigned long mir;	/* 04 */
	volatile unsigned long clear;	/* 08 */
	volatile unsigned long set;	/* 0C */
	volatile unsigned long sw_set;	/* 10 */
	volatile unsigned long sw_clear;
	volatile unsigned long irq_pending;
	volatile unsigned long fiq_pending;
};

struct intcon {
	volatile unsigned long id;	/* 00 */
	unsigned long _pad0[3];
	volatile unsigned long config;	/* 10 */
	volatile unsigned long status;	/* 14 */
	unsigned long _pad1[10];
	volatile unsigned long sir_irq;	/* 40 */
	volatile unsigned long sir_fiq;	/* 44 */
	volatile unsigned long control;	/* 48 */
	volatile unsigned long protect;	/* 4c */
	volatile unsigned long idle;	/* 50 */
	unsigned long _pad2[3];
	volatile unsigned long irq_prio; /* 60 */
	volatile unsigned long fiq_prio; /* 64 */
	volatile unsigned long threshold; /* 68 */
	unsigned long _pad3[5];
	struct intmask mask[4];
	unsigned long ilr[128];
};

void
intcon_init ( void )
{
	struct intcon *base = (struct intcon *) INTCON_BASE;

	base->config = 0;
}

void
intcon_test ( void )
{
	struct intcon *base = (struct intcon *) INTCON_BASE;
	unsigned long *p;

	p = &base->ilr[0];
	printf ( "ILR starts at %08x\n", p );
}

/* We seem to have 3 ways to get at the interrupt mask
 * We can see the whole 32 bits in "mir".
 * The reset state is all ones, hinting that we clear
 * a bit to enable the interrupt.
 * Then we have clear and set registers, which I guess
 * avoid reading, changing and setting the MIR register.
 */
void
intcon_ena ( int item )
{
	struct intcon *base = (struct intcon *) INTCON_BASE;
	int who;

	item &= 0x7f;
	who = item/32;
	base->mask[who].clear = 1 << (item%32);
}

void
intcon_dis ( int item )
{
	struct intcon *base = (struct intcon *) INTCON_BASE;
	int who;

	item &= 0x7f;
	who = item/32;
	base->mask[who].set = 1 << (item%32);
}

/* Called from interrupt handler */
int
intcon_irqwho ( void )
{
	struct intcon *base = (struct intcon *) INTCON_BASE;
	int n = base->sir_irq;

	/* ack the irq in the intcon */
	base->control = 1;

	/*
	printf ("Interrupt: %08x %d\n", n, n&0x7f );
	*/

	return ( base->sir_irq & 0x7f );
}

/* THE END */