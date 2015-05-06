/* hardware.c
 * Tom Trebisky  5/5/2015
 *
 * Each architecture should someday have its own
 * version of this, for now we use ifdefs
 */

#include "kyu.h"

void
hardware_init ( void )
{
#ifdef ARCH_ARM
	trap_init ();
	serial_init ( CONSOLE_BAUD );
#endif

#ifdef ARCH_X86
	/* Must call trap_init() first */
	trap_init ();
	timer_init ();
	kb_init ();
	sio_init ();
#endif
}

/* THE END */
