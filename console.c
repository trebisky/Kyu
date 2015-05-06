/* console.c
 * $Id: console.c,v 1.12 2005/03/25 20:51:18 tom Exp $
 *
 * Tom Trebisky  9/21/2001
 * made generic 12/21/2002, kb.c and vga.c were split out.
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include <stdarg.h>

extern struct thread *cur_thread;

void panic ( char * );
void putchar ( int );
void puts ( char * );

/* -------------------------------------------- */

void
console_serial ( void )
{
	cur_thread->con_mode = SERIAL;
}

#ifdef ARCH_X86
void
console_vga ( void )
{
	cur_thread->con_mode = VGA;
}

void
console_sio0 ( void )
{
	cur_thread->con_mode = SIO_0;
}

void
console_sio1 ( void )
{
	cur_thread->con_mode = SIO_1;
}
#endif

/* XXX */
int
get_console_type ( void )
{
	return (int) cur_thread->con_mode;
}

/* --------------------- */

/* get a character, no echo.
 */
int
getchare ( void )
{
	if ( cur_thread->con_mode == SERIAL )
	    return serial_getc ();
#ifdef ARCH_X86
	else if ( cur_thread->con_mode == SIO_0 )
	    return sio_getc ( 0 );
	else
	    return kb_read ();
#endif
	panic ( "No getchare configured" );
}

/* get a character, with echo
 */
int
getchar ( void )
{
	int c;

	if ( cur_thread->con_mode == SERIAL ) {
	    c = serial_getc ();
	    serial_putc ( c );
	}
#ifdef ARCH_X86
	else if ( cur_thread->con_mode == SIO_0 ) {
	    c = sio_getc ( 0 );
	    sio_putc ( 0, c );
	    return c;
	} else {
	    c = kb_read ();
	    vga_putc ( c );
	    return c;
	}
#endif
	panic ( "No getchar configured\n" );

}

void
backspace ( void )
{
#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 ) {
	    ;	/* XXX */
	} else {
	    vga_back ();
	}
#endif
}

void
getline ( char *buf, int n )
{
	int c;
	int i;
	char *p;

	n--;
	p = buf;
	for ( i=0; i<n; ) {
	    c = getchare ();
	    if ( c == '\r' )
	    	break;
#ifdef notdef
#define REBOOT	18
	    if ( c == REBOOT ) {
	    	kb_reset ();
	    }
#endif

#define KILL	21
	    if ( c == KILL ) {
		while ( i ) {
		    backspace();
		    i--;
		}
		p = buf;
		continue;
	    }

#define ERASE	8
	    if ( c == ERASE ) {
		if ( i ) {
		    backspace ();
		    --p;
		    --i;
		}
		continue;
	    }
	    putchar ( c );
	    *p++ = c;
	    i++;
	}
	putchar ( '\n' );
	*p = '\0';
}

/* --------------------- */

/* printf stuff follows.
 *
 * XXX - looks like printf is not thread safe!
 * va_start/va_end are probably the culprits, but I
 * am not sure, in any event if a print is in progress,
 * then an interrupt happens and does a printf, we get
 * messy and confusing output.
 *
 * 12/21/2002 - I think this is fixed now by putting.
 * msg_buf on the stack.
 */

/* Added 12/2/2002 -- with gcc 3.2 printf() calls
 * get optimized into puts() and putchar() calls
 * whenever possible, so you had better supply
 * puts() and putchar() functions.
 * Also note that puts is expected to add a newline.
 */
void
puts ( char *buf )
{
	if ( cur_thread->con_mode == SERIAL ) {
	    serial_puts ( buf );
	    serial_putc ( '\n' );
	}
#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 ) {
	    sio_puts ( 0, buf );
	    sio_putc ( 0, '\n' );
	} else {
	    vga_puts ( buf );
	    vga_putc ( '\n' );
	}
#endif
}

void
putchar ( int ch )
{
	serial_putc ( ch );
#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 )
	    sio_putc ( 0, ch );
	else
	    vga_putc ( ch );
#endif
}

#define PRINTF_BUF_SIZE 128

int vprintf ( const char *fmt, va_list args )
{
	char buf[PRINTF_BUF_SIZE];
	int rv;

	rv = vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );

	serial_puts ( buf );
#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 )
	    sio_puts ( 0, buf );
	else
	    vga_puts ( buf );
#endif

	return rv;
}

int
printf ( const char *fmt, ... )
{
	va_list args;
	int rv;

	va_start ( args, fmt );
	rv = vprintf ( fmt, args );
	va_end ( args );
	return rv;
}

/* ------------------------------------------ */

/* XXX - for now, panic routines are here.
 */

/* Really only ready/proper for x86
#define PANIC_DEBUG
*/

#ifdef PANIC_DEBUG

#include "intel.h"

extern long timer_count_t;
extern long timer_count_s;

extern long bogus_count;

static void
panic_debug ( void )
{
	int c;
	unsigned long hi, lo;

	for ( ;; ) {
	    c = getchare ();
	    if ( c == 'i' ) {
	    	printf ( "timer count: %d %d\n",
		    timer_count_s, timer_count_t );
	    	printf ( "bogus count: %d\n",
		    bogus_count );
	    }
	    if ( c == 'a' ) {
		unsigned long long t1, t2;
		int dt;
		/* read the Pentium hires timer.
		 * (macros in io.h)
		 * the low word rolls over every
		 * 8 seconds or so ...
		 */ 
		rdtscll ( t1 );
		rdtscll ( t2 );
		dt = t2 - t1;
		rdtsc ( lo, hi );
		printf ( "TSC: %u %u (%d)\n", hi, lo, dt );
	    }
	    if ( c == '+' )	/* allow interrupts */
		cpu_leave ();
	    if ( c == '-' )	/* mask interrupts */
		cpu_enter ();
	    if ( c == 'r' )
	    	kb_reset ();
	    if ( c == 't' )
	    	thr_show ();
	    if ( c == 'u' )
	    	thr_show_current ();
	    if ( c == 's' ) {
		printf ( "eip: %8x\n", getip() );
		printf ( "esp: %8x\n", getsp() );
		printf ( " ef: %8x\n", getfl() );
	    }
	    if ( c == 'd' ) {
	    	dump_l ( (void *) getsp(), 8 );
	    }
	    if ( c == 'c' ) {	/* continue */
		printf ( "continue from panic\n" );
	    	return;
	    }
	}
}
#endif

/* This is a real panic, as distinguished
 * from calls to dpanic(), which are just
 * for debugging, and should be removed in
 * production version.
 */
void
panic ( char *msg )
{
	if ( msg )
	    printf ( "PANIC: %s\n", msg );
	else
	    printf ( "PANIC\n" );

#ifdef PANIC_DEBUG
	panic_debug ();
	/* returning here is probably a BAD idea.
	 * (but, what the heck ...).
	 */
#else
	for ( ;; )
	    ;
#endif

}

/* temporary debugging panic
 */
void
dpanic ( char *msg )
{
	if ( msg )
	    printf ( "panic: %s\n", msg );
	else
	    printf ( "panic\n" );

#ifdef PANIC_DEBUG
	panic_debug ();
#endif
	/* OK to return and continue here.
	 */
}

/* THE END */
