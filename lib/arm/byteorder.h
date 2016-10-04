/*
 *  arch/arm/include/asm/byteorder.h
 *
 * ARM Endian-ness.  In little endian mode, the data bus is connected such
 * that byte accesses appear as:
 *  0 = d0...d7, 1 = d8...d15, 2 = d16...d23, 3 = d24...d31
 * and word accesses (data or instruction) appear as:
 *  d0...d31
 *
 * When in big endian mode, byte accesses appear as:
 *  0 = d24...d31, 1 = d16...d23, 2 = d8...d15, 3 = d0...d7
 * and word accesses (data or instruction) appear as:
 *  d0...d31
 */
#ifndef __ASM_ARM_BYTEORDER_H
#define __ASM_ARM_BYTEORDER_H

/* Kyu tjt 10-1-2016
 * From linux sources circa 4.0.5 or so.
 *    arch/arm/include/uapi/asm/byteorder.h
 *
 * The story on this file is interesting.
 * ARMEB is "big endian ARM", which is sort of an unusual variant,
 *  but an option you could choose for various reasons, but by
 *  no means mainstream.  Mostly for "Intel Xscale" chips,
 *  whatever they are.
 */

#ifdef KYU
#include "arm/little_endian.h"

#else
#ifdef __ARMEB__
#include <linux/byteorder/big_endian.h>
#else
#include <linux/byteorder/little_endian.h>
#endif
#endif

#endif
