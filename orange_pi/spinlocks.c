/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * spinlocks.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/23/2017
 *
 */

#define SPINLOCK_BASE     (struct spinlocks *) 0x01c18000
#define NLOCKS	32

struct spinlocks {
	volatile unsigned long sysstatus;
	long  _pad0[3];
	volatile unsigned long status;
	long  _pad1[59];
	volatile unsigned long lock[NLOCKS];
};

static void
spinlocks_test ( void )
{
	struct spinlocks *sl = SPINLOCK_BASE;
	int lock = 0;
	int lmask = 1<<lock;
	unsigned long xyz;
	int count;

	printf ( "Spin sys: %08x\n", sl->sysstatus );
	printf ( "Spin stat: %08x\n", sl->status );
	printf ( "Spin sys: %08x\n", sl->sysstatus );

	printf ( "Take\n" );
	xyz = sl->lock[lock];	/* take it */
	printf ( "Spin stat: %08x\n", sl->status );
	printf ( "Spin lock: %08x\n", xyz );
	printf ( "Spin sys: %08x\n", sl->sysstatus );
	printf ( "Spin stat: %08x\n", sl->status );

	printf ( "Release\n" );
	sl->lock[lock] = 0;	/* release it */
	printf ( "Spin sys: %08x\n", sl->sysstatus );
	printf ( "Spin stat: %08x\n", sl->status );
	for ( count=1000; count > 0; count-- )
	    if ( (sl->status & lmask) == 0 )
		break;

	printf ( "Spin sys: %08x\n", sl->sysstatus );
	printf ( "Spin stat: %08x\n", sl->status );
}

#define CCU_BASE		0x01c20000

#define BUS_GATING_REG1     	0x64
#define BUS_RESET_REG1     	0x02C4

#define SPINLOCK_BIT		(1<<22)

void
spinlocks_init ( void )
{
	volatile unsigned long *lp;

	lp = (volatile unsigned long *) (CCU_BASE + BUS_GATING_REG1);
	*lp |= SPINLOCK_BIT;
	lp = (volatile unsigned long *) (CCU_BASE + BUS_RESET_REG1);
	*lp |= SPINLOCK_BIT;

	spinlocks_test ();
}

void
spin_lock ( int lock )
{
	struct spinlocks *sl = SPINLOCK_BASE;

	for ( ;; ) {
	    if ( sl->lock[lock] == 0 )
		break;
	}

}

void
spin_unlock ( int lock )
{
	struct spinlocks *sl = SPINLOCK_BASE;

	sl->lock[lock] = 0;	/* release it */
}

/* THE END */
