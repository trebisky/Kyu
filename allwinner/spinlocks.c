/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * h3_spinlocks.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/23/2017
 *
 */
#include <arch/types.h>

#define SPINLOCK_BASE     (struct h3_spinlocks *) 0x01c18000
#define NLOCKS	32

struct h3_spinlocks {
	vu32 sysstatus;
	u32  _pad0[3];
	vu32 status;
	u32  _pad1[59];
	vu32 lock[NLOCKS];
};

#define CCU_BASE		0x01c20000

#define BUS_GATING_REG1     	0x64
#define BUS_RESET_REG1     	0x02C4

#define SPINLOCK_BIT		(1<<22)

/* Initially all locks are unlocked */

void
h3_spinlocks_init ( void )
{
	vu32 *lp;

	lp = (vu32 *) (CCU_BASE + BUS_GATING_REG1);
	*lp |= SPINLOCK_BIT;
	lp = (vu32 *) (CCU_BASE + BUS_RESET_REG1);
	*lp |= SPINLOCK_BIT;

	// h3_spinlocks_test ();
}

int
h3_spin_check ( int lock )
{
	struct h3_spinlocks *sl = SPINLOCK_BASE;

	return sl->status & (1<<lock);
}

void
h3_spin_lock ( int lock )
{
	struct h3_spinlocks *sl = SPINLOCK_BASE;

	for ( ;; ) {
	    if ( sl->lock[lock] == 0 )
		break;
	}

}

void
h3_spin_unlock ( int lock )
{
	struct h3_spinlocks *sl = SPINLOCK_BASE;

	sl->lock[lock] = 0;	/* release it */
}

#ifdef notdef
static void
h3_spinlocks_test ( void )
{
	struct h3_spinlocks *sl = SPINLOCK_BASE;
	int lock = 0;
	int lmask = 1<<lock;
	u32 xyz;
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
#endif


/* THE END */
