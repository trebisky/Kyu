/* kyu.h
 * Parameters and definitions specific to Kyu
 *
 *	Tom Trebisky  11/25/2001
 *
 */

#define MAX_THREADS	32
#define MAX_SEM		64
#define MAX_CV		32

#define DEBUG_THREADS	1

#define STACK_SIZE	4096	/* bytes */

/*
#define	ARCH_X86
*/
#define	ARCH_ARM

/* XXX XXX - doesn't belong here */
#ifdef	ARCH_ARM
#define IRQ_STACK_BASE 0x9D000000
#endif

/*
#define WANT_DELAY
#define WANT_PCI
#define WANT_NET
#define WANT_SLAB
#define WANT_BENCH
#define WANT_USER
*/

/* define this even if not using serial console.
#define CONSOLE_BAUD		38400
 */
#define CONSOLE_BAUD		115200

/*
#define INITIAL_CONSOLE		VGA
#define INITIAL_CONSOLE		SIO_0
*/
#define INITIAL_CONSOLE		SERIAL

#define DEFAULT_TIMER_RATE	100

/* THE END */
