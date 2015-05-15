/* omap_ints.h
 *  Kyu project  5-12-2015  Tom Trebisky
 */

/* List of fault codes */
/* Note that IRQ is in general, not a fault
 * and will not be handled as one.
 */
#define F_NONE	0
#define F_UNDEF	1
#define F_SWI	2
#define F_PABT	3
#define F_DABT	4
#define F_NU	5
#define F_FIQ	6
#define F_IRQ	7

/* Not a processor fault, but we try to
 * treat it like one.
 */
#define F_PANIC	8

/* List of interrupt numbers for the 3359 omap
 * There are 128 of these, we will fill out
 *  the list as needed.
 */

#define NUM_INTS	128

#define NINT_TIMER0	66
#define NINT_TIMER1	67
#define NINT_UART0	72

/* THE END */
