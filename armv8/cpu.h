/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 *
 * cpu.h for the ARMv8
 *
 * Macros and definitions for the ARM processor
 *
 *  Kyu project  9-23-2018  Tom Trebisky
 */

#ifndef __CPU_H_
#define __CPU_H_	1

#define NUM_IREGS	34

/* Offsets into iregs and jregs in the thread structure */
#define ARMV8_X0                0
#define ARMV8_X30               30
#define ARMV8_LR                30

#define ARMV8_SP                31
#define ARMV8_ELR               32
#define ARMV8_SPSR              33

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/* Byte swapping macros.
 * Almost surely we could do something ARM specific
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

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/* Provided for the sake of syntax, the macros below will yield better code.
 * This is a 64 bit counter in ARM v8
 */
static inline unsigned long
r_CCNT ( void )
{
    unsigned long rv;

    // Read CCNT Register
    asm volatile ("mrs %0, PMCCNTR_EL0": "=r" (rv) );
    return rv;
}

/* Performance monitoring unit registers */
#define get_CCNT(val)	asm volatile ( "mrs %0, PMCCNTR_EL0" : "=r" ( val ) )
#define set_CCNT(val)	asm volatile ( "msr PMCCNTR_EL0, %0" : : "r" ( val ) )

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

#define get_SP(x)	asm volatile ("add %0, sp, #0\n" :"=r" ( x ) )
#define get_FP(x)	asm volatile ("add %0, fp, #0\n" :"=r" ( x ) )

#define INT_unlock 	asm volatile("msr DAIFClr, #3" : : : "cc")
#define INT_lock 	asm volatile("msr DAIFSet, #3" : : : "cc")

#ifdef notdef
/* These bit me with a horrible and hard to track down bug.
 * If I just type INT_lock; the compiler accepts the line, but
 * generates no code!!  I have no idea why, but am switching to
 * the above syntax.  If I type INT_lock(); with the above macros,
 * I get a compile error, which is better than a silent bug.
 * tjt 10-10-2018
 */
/* These only go inline with gcc -O of some kind */
static inline void
INT_unlock ( void )
{
        asm volatile("msr DAIFClr, #3" : : : "cc");
}

static inline void
INT_lock ( void )
{
        asm volatile("msr DAIFSet, #3" : : : "cc");
}
#endif

/* The above Clr/Set work as if DAIF is right justified, as you might expect.
 *  Note that I am enabling/disabling both IRQ and FIQ.
 * The following raw read/write deal with DAIF shifted left 6 bits
 *  just as it is located in the PSR, which is different than the above.
 */

/* DAIF */
#define get_DAIF(val)	asm volatile ( "mrs %0, DAIF" : "=r" ( val ) )
#define set_DAIF(val)	asm volatile ( "msr DAIF, %0" : : "r" ( val ) )

#ifdef notdef
static inline unsigned int
raw_read_DAIF ( void )
{
	unsigned int val;

	__asm__ __volatile__("mrs %0, DAIF\n\t" : "=r" (val) :  : "memory");

	return val;
}

static inline void
raw_write_DAIF( unsigned int val )
{
	__asm__ __volatile__("msr DAIF, %0\n\t" : : "r" (val) : "memory");
}
#endif

#endif

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

#ifdef notdef

/* ARM v7 stuff parked below here */

/* These functions don't get compiled inline unless some level of
 * optimization is enabled.  By default they become just static functions
 * and get duplicated in more than one place in the code, which is harmless
 * but yields some unfortunate bloat.
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

#define get_CPSR(val)	asm volatile ( "mrs %0, cpsr" : "=r" ( val ) )
#define set_CPSR(val)	asm volatile ( "msr cpsr, %0" : : "r" ( val ) )

#endif

/* THE END */
