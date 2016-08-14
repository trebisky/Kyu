/* hardware.c
 * Tom Trebisky  5/5/2015
 *
 * Each architecture should someday have its own
 * version of this, for now we use ifdefs
 */

#include "kyu.h"
#include "kyulib.h"
#include "hardware.h"

#ifdef ARCH_ARM
/* Remember the BBB is an ARM Cortex-A8, the cycle count
 * registers on other ARM variants have different addresses.
 * All of this ARM cycle count stuff is supported by code
 * in locore.S (we could use inline assembly, but we don't.)
 * The counter runs at the full CPU clock rate (1 Ghz),
 * so it overflows every 4.3 seconds.
 * We can set the DIV bit to divide it by 64.
 * This means it counts at 15.625 Mhz and overflows
 * every 275 seconds, which may be important.
 * The overflow can be set up to cause an interrupt, but
 * that is a lot of bother, the best bet is to just reset
 * it before using it to time something.
 *
 * This was helpful:
 * http://stackoverflow.com/questions/3247373/
 *   how-to-measure-program-execution-time-in-arm-cortex-a8-processor
 */
#define PMCR_ENABLE	0x01	/* enable all counters */
#define PMCR_RESET	0x02	/* reset all counters */
#define PMCR_CC_RESET	0x04	/* reset CCNT */
#define PMCR_CC_DIV	0x08	/* divide CCNT by 64 */
#define PMCR_EXPORT	0x10	/* export events */
#define PMCR_CC_DISABLE	0x20	/* disable CCNT in non-invasive regions */

/* There are 4 counters besides the CCNT (we ignore them at present) */
#define CENA_CCNT	0x80000000
#define CENA_CTR3	0x00000008
#define CENA_CTR2	0x00000004
#define CENA_CTR1	0x00000002
#define CENA_CTR0	0x00000001

void
enable_ccnt ( int div64 )
{
	int val;

	val = get_pmcr ();
	val |= PMCR_ENABLE;
	if ( div64 )
	    val |= PMCR_CC_DIV;
	set_pmcr ( val );

	set_cena ( CENA_CCNT );
	// set_covr ( CENA_CCNT );
}

/* It does not seem necessary to disable the counter
 * to reset it.
 */
void
reset_ccnt ( void )
{
	/*
	set_cdis ( CENA_CCNT );
	set_pmcr ( get_pmcr() | PMCR_CC_RESET );
	set_cena ( CENA_CCNT );
	*/
	set_pmcr ( get_pmcr() | PMCR_CC_RESET );
}

/* delays with nanosecond resolution.
 * callers may also want to disable interrupts.
 * Also, beware of the compiler optimizing this away.
 * There is about 60 ns extra delay (call overhead).
 * A fussy caller could subtract 50 ns from the argument.
 * Good for up to 4.3 second delays.
 */
void
delay_ns ( int delay )
{
	reset_ccnt ();
	while ( get_ccnt() < delay )
	    ;
}

#endif

void
hardware_init ( void )
{
	mem_malloc_init ( MALLOC_BASE, MALLOC_SIZE );

#ifdef ARCH_ARM
	wdt_disable ();

	mux_init ();
	intcon_init ();
	cm_init ();

	enable_ccnt ( 0 );

	serial_init ( CONSOLE_BAUD );
	timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();

	gpio_init ();
	i2c_init ();
#endif

#ifdef ARCH_X86
	/* Must call trap_init() first */
	trap_init ();
	timer_init ();
	/* timer_init ( DEFAULT_TIMER_RATE ); */
	kb_init ();
	sio_init ();
#endif
}

#define BBB_RAM_START	0x80000000
#define BBB_RAM_END	0x9FFFFFFF
#define BBB_RAM_ENDP	0xA0000000

int
valid_ram_address ( unsigned long addr )
{
	if ( addr < BBB_RAM_START )
	    return 0;
	if ( addr > BBB_RAM_END )
	    return 0;
	return 1;
}

/* THE END */
