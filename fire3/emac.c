/*
 * Copyright (C) 2018  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * emac.c for the Samsung s5p6818 on the NanoPi Fire3
 *
 * Tom Trebisky  10/13/2018
 *
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
// #include <h3_ints.h>
#include <arch/cpu.h>

#include "netbuf.h"
// #include "net.h"

/* line size for this flavor of ARM */
/* XXX should be in some header file */
// #define	ARM_DMA_ALIGN	64

/* from linux/compiler_gcc.h in U-Boot */
#define __aligned(x)            __attribute__((aligned(x)))

/* ..... */

struct emac {
	volatile unsigned int xyz;		/* 00 */
};

struct emac_dma {
	volatile unsigned int xyz;		/* 00 */
};

#define EMAC_BASE	((struct emac *) 0xc0060000)
#define EMAC_DMA_BASE	((struct emac_dma *) 0xc0061000)

/* THE END */
