/* omap_ints.h
 *  Kyu project  5-12-2015  Tom Trebisky
 *
 * This filename is our big chance to use "omap"
 *  and look super cool to everyone.
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

/* List of interrupt numbers for the am3359 on the BBB
 *
 * There are 128 of these, we will fill out the list as needed.
 * Interrupts are chapter 6 in the TRM, table on pages 212-215
 */

#define NUM_INTS	128

#define IRQ_CPSW_RX_THR		40
#define IRQ_CPSW_RX		42
#define IRQ_CPSW_TX		43
#define IRQ_CPSW_MISC		44

#define IRQ_I2C0		70
#define IRQ_I2C1		71
#define IRQ_I2C2		30

#define IRQ_TIMER0	66
#define IRQ_TIMER1	67
#define IRQ_TIMER2	68
#define IRQ_TIMER3	69
#define IRQ_TIMER4	92
#define IRQ_TIMER5	93
#define IRQ_TIMER6	94
#define IRQ_TIMER7	95

#define IRQ_UART0	72

/* THE END */
