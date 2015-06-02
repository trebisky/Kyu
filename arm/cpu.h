/* cpu.h
 *
 * Macros and definitions for the ARM processor
 *
 *  Kyu project  5-18-2015  Tom Trebisky
 */

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
