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
