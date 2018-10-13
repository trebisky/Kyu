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

#ifndef __CPU_H_
#define __CPU_H_	1

#define NUM_IREGS	17

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

/* Almost surely we could do something ARM specific
 * that would be better than this. XXX XXX
 * Quick and dirty for now  5-30-2015 10:45 PM
 * Stolen from u-boot include/linux/byteorder/little-endian.h
 */
typedef unsigned short __u16;
typedef unsigned long __u32;

#ifndef __SWAP_H
#define __SWAP_H 1

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
#endif /* __SWAP_H */

#ifdef notdef
/* ARM v7 now has actual barrier instructions, but these work for
 * older ARM (like v5) as well as v7.
 * At some point U-boot used these since it compiled with -march=armv5
 * but we compile Kyu with -marm -march=armv7-a
 */
#define CP15ISB asm volatile ("mcr     p15, 0, %0, c7, c5, 4" : : "r" (0))
#define CP15DSB asm volatile ("mcr     p15, 0, %0, c7, c10, 4" : : "r" (0))
#define CP15DMB asm volatile ("mcr     p15, 0, %0, c7, c10, 5" : : "r" (0))
#endif

/* Added 6-14-2018
 * A collection of inline assembly for ARM control register access
 * Above all, this makes code more readable and less error prone.
 *  (as long as we get these macros right)
 */
#define get_SCTLR(val)	asm volatile ( "mrc p15, 0, %0, c1, c0, 0" : "=r" ( val ) )
#define set_SCTLR(val)	asm volatile ( "mcr p15, 0, %0, c1, c0, 0" : : "r" ( val ) )
#define get_ACTLR(val)	asm volatile ( "mrc p15, 0, %0, c1, c0, 1" : "=r" ( val ) )
#define set_ACTLR(val)	asm volatile ( "mcr p15, 0, %0, c1, c0, 1" : : "r" ( val ) )

#define set_TTBR0(val)	asm volatile ( "mcr p15, 0, %0, c2, c0, 0" : : "r" ( val ) )
#define get_TTBR0(val)	asm volatile ( "mrc p15, 0, %0, c2, c0, 0" : "=r" ( val ) )

#define set_TTBR1(val)	asm volatile ( "mcr p15, 0, %0, c2, c0, 1" : : "r" ( val ) )
#define get_TTBR1(val)	asm volatile ( "mrc p15, 0, %0, c2, c0, 1" : "=r" ( val ) )
#define set_TTBCR(val)	asm volatile ( "mcr p15, 0, %0, c2, c0, 2" : : "r" ( val ) )
#define get_TTBCR(val)	asm volatile ( "mrc p15, 0, %0, c2, c0, 2" : "=r" ( val ) )

#define set_DACR(val)	asm volatile ( "mcr p15, 0, %0, c3, c0, 0" : : "r" ( val ) )

#define get_VBAR(val)	asm volatile ( "mrc p15, 0, %0, c12, c0, 0" : "=r" ( val ) )
#define set_VBAR(val)	asm volatile ( "mcr p15, 0, %0, c12, c0, 0" : : "r" ( val ) )

/* Performance monitoring unit registers */
#define get_CCNT(val)	asm volatile ( "mrc p15, 0, %0, c9, c13, 0" : "=r" ( val ) )
#define set_CCNT(val)	asm volatile ( "mcr p15, 0, %0, c9, c13, 0" : : "r" ( val ) )

#define get_PMCR(val)	asm volatile ( "mrc p15, 0, %0, c9, c12, 0" : "=r" ( val ) )
#define set_PMCR(val)	asm volatile ( "mcr p15, 0, %0, c9, c12, 0" : : "r" ( val ) )
#define get_CENA(val)	asm volatile ( "mrc p15, 0, %0, c9, c12, 1" : "=r" ( val ) )
#define set_CENA(val)	asm volatile ( "mcr p15, 0, %0, c9, c12, 1" : : "r" ( val ) )
#define get_CDIS(val)	asm volatile ( "mrc p15, 0, %0, c9, c12, 2" : "=r" ( val ) )
#define set_CDIS(val)	asm volatile ( "mcr p15, 0, %0, c9, c12, 2" : : "r" ( val ) )
#define get_COVR(val)	asm volatile ( "mrc p15, 0, %0, c9, c12, 3" : "=r" ( val ) )

#define set_TLB_INV(val)	asm volatile ( "mcr p15, 0, %0, c8, c7, 0" : : "r" ( val ) )
#define set_TLB_INV_MVA(val)	asm volatile ( "mcr p15, 0, %0, c8, c7, 1" : : "r" ( val ) )
#define set_TLB_INV_ASID(val)	asm volatile ( "mcr p15, 0, %0, c8, c7, 2" : : "r" ( val ) )

/* Note that macros of the "get_XXX" type do not allow expressions like
 * var = get_XXX();
 * For that, we need inline functions, and we give them distinct names,
 *  such as r_XXX() to avoid collision with these macros.
 */
#define get_SP(x)	asm volatile ("add %0, sp, #0\n" :"=r" ( x ) )
#define get_FP(x)	asm volatile ("add %0, fp, #0\n" :"=r" ( x ) )
#define get_PC(x)	asm volatile ("add %0, pc, #0\n" :"=r" ( x ) )

#define get_CPSR(val)	asm volatile ( "mrs %0, cpsr" : "=r" ( val ) )
#define set_CPSR(val)	asm volatile ( "msr cpsr, %0" : : "r" ( val ) )

/* Multiprocessor affinity register */
#define get_MPID(val)	asm volatile ( "mrc p15, 0, %0, c0, c0, 5" : "=r" ( val ) )

/* Disable interrupts to lock section */
#define INT_lock	\
	asm volatile (	"mrs     r0, cpsr; \
			orr     r0, r0, #0xc0; \
			msr     cpsr, r0" ::: "r0" )

/* Enable interrupts to unlock section */
#define INT_unlock	\
	asm volatile (	"mrs     r0, cpsr; \
			bic     r0, r0, #0xc0; \
			msr     cpsr, r0" ::: "r0" )

/* These functions don't get compiled inline unless some level of
 * optimization is enabled.  By default they become just static functions
 * and get duplicated in more than one place in the code, which is harmless
 * but yields some unfortunate bloat.
 */

/* Provided for the sake of syntax, the macros above will yield better code.
 */
static inline unsigned int
r_CCNT ( void )
{
    unsigned int rv;

    // Read CCNT Register
    asm volatile ("mrc p15, 0, %0, c9, c13, 0": "=r" (rv) );
    return rv;
}

#ifdef notdef
/* Disable interrupts to lock section */
static inline void
INT_lock_f ( void )
{
	asm volatile ( "mrs     r0, cpsr" );
	asm volatile ( "orr     r0, r0, #0xc0" );
	asm volatile ( "msr     cpsr, r0" );
}

/* Enable interrupts to unlock section */
static inline void
INT_unlock_f ( void )
{
	asm volatile ( "mrs     r0, cpsr" );
	asm volatile ( "bic     r0, r0, #0xc0" );
	asm volatile ( "msr     cpsr, r0" );
}
#endif

/* Old BSD style interrupt locking */
#ifdef nodef
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
#endif

#endif /* __CPU_H_ */

/* THE END */
