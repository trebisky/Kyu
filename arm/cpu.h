/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 *
 * cpu.h for the ARM
 *
 * Macros and definitions for the ARM processor
 *
 *  Kyu project  5-18-2015  Tom Trebisky
 *     Tom Trebisky  1/7/2017
 */

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


/* restore prior interrupt status */
static inline void splx ( int arg )
{
    asm volatile ( "msr     cpsr, %[new]" : : [new] "r" (arg) );
}

/* lock out all interrupts */
static inline int splhigh ( void )
{
    int rv;
    asm volatile ( "mrs     %[old], cpsr\n\t"
		    "mov     r4, %[old]\n\t"
		    "orr     r4, r4, #0xc0\n\t"
		    "msr     cpsr, r4"
		    : [old] "=r" (rv) : : "r4" );
    return rv;
}

/* lock out IRQ */
static inline int splirq ( void )
{
    int rv;
    asm volatile ( "mrs     %[old], cpsr\n\t"
		    "mov     r4, %[old]\n\t"
		    "orr     r4, r4, #0x80\n\t"
		    "msr     cpsr, r4"
		    : [old] "=r" (rv) : : "r4" );
    return rv;
}
/* lock out FIQ */
static inline int splfiq ( void )
{
    int rv;
    asm volatile ( "mrs     %[old], cpsr\n\t"
		    "mov     r4, %[old]\n\t"
		    "orr     r4, r4, #0x40\n\t"
		    "msr     cpsr, r4"
		    : [old] "=r" (rv) : : "r4" );
    return rv;
}

/* Almost surely we could do something ARM specific
 * that would be better than this. XXX XXX
 * Quick and dirty for now  5-30-2015 10:45 PM
 * Stolen from u-boot include/linux/byteorder/little-endian.h
 */
typedef unsigned short __u16;
typedef unsigned long __u32;

#define ___swab16(x) \
        ((__u16)( \
                (((__u16)(x) & (__u16)0x00ffU) << 8) | \
                (((__u16)(x) & (__u16)0xff00U) >> 8) ))
#define ___swab32(x) \
        ((__u32)( \
                (((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
                (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
                (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
                (((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

/* XXX - evil if we do htons(x++) */
#define htons(x)        ___swab16(x)
#define ntohs(x)        ___swab16(x)

#define htonl(x)        ___swab32(x)
#define ntohl(x)        ___swab32(x)

/*
#define __constant_htonl(x) ((__force __be32)___constant_swab32((x)))
#define __constant_ntohl(x) ___constant_swab32((__force __be32)(x))
#define __constant_htons(x) ((__force __be16)___constant_swab16((x)))
#define __constant_ntohs(x) ___constant_swab16((__force __be16)(x))
*/

/* THE END */
