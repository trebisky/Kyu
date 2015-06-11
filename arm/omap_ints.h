/* omap_ints.h
 *  Kyu project  5-12-2015  Tom Trebisky
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

/* List of interrupt numbers for the 3359 omap
 * There are 128 of these, we will fill out
 *  the list as needed.
 */

#define NUM_INTS	128

#define NINT_TIMER0	66
#define NINT_TIMER1	67
#define NINT_UART0	72

/* THE END */
