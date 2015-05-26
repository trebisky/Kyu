/* hardware.c
 * Tom Trebisky  5/5/2015
 *
 * Each architecture should someday have its own
 * version of this, for now we use ifdefs
 */

#include "kyu.h"
#include "hardware.h"

void
hardware_init ( void )
{

	mem_malloc_init ( MALLOC_BASE, MALLOC_SIZE );

#ifdef ARCH_ARM
	wdt_disable ();

	mux_init ();
	intcon_init ();
	cm_init ();

	serial_init ( CONSOLE_BAUD );
	timer_init ( DEFAULT_TIMER_RATE );

	/* CPU interrupts on */
	enable_irq ();

	gpio_init ();
#endif

#ifdef ARCH_X86
	/* Must call trap_init() first */
	trap_init ();
	timer_init ();
	kb_init ();
	sio_init ();
#endif
}

int
valid_ram_address ( unsigned long addr )
{
	if ( addr < 0x80000000 )
	    return 0;
	if ( addr >= 0x90000000 )
	    return 0;
	return 1;
}

/* THE END */
