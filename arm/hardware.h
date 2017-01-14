/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * hardware.h for the ARM
 *
 * T. Trebisky  1/7/2017
 */

/* We no longer need this now that we have discovered the A bit in
 * the system control register and all this should be deleted once
 * we have thoroughly tested the network stack without it.
 */
// #define ARM_ALIGNMENT_HACK

/* List of fault codes */
/* The first 8 are ARM hardware exceptions and interrupts */

#define F_NONE	0
#define F_UNDEF	1
#define F_SWI	2
#define F_PABT	3
#define F_DABT	4
#define F_NU	5
#define F_FIQ	6	/* not a fault */
#define F_IRQ	7	/* not a fault */

#define F_DIVZ	8	/* pseudo for linux library */
#define F_PANIC	9	/* pseudo for Kyu, user panic */

/* THE END */
