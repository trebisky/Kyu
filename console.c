/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* console.c
 * $Id: console.c,v 1.12 2005/03/25 20:51:18 tom Exp $
 *
 * Tom Trebisky  9/21/2001
 * made generic 12/21/2002, kb.c and vga.c were split out.
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "arch/cpu.h"

#include <stdarg.h>

// #include "interrupts.h"

extern struct thread *cur_thread;

static void panic_debug ( void );

void panic ( char * );

/*
void putchar ( int );
void puts ( char * );
*/

/* -------------------------------------------- */

/* Set up for minimal IO without interrupts */
void
console_initialize ( void )
{
	serial_init ( CONSOLE_BAUD );
}

/* Full initialization */
void
console_init ( void )
{
	serial_init ( CONSOLE_BAUD );
}

/* --------------- */
/* --------------- */

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

#define ERASE	0x08	/* Ctrl-H */
#define REBOOT	0x12	/* Ctrl-R */
#define KILL	0x15	/* Ctrl-U */
#define DELETE	0x7F	/* Backspace on PC keyboard */
#define SPACE	0x20

void
backspace ( void )
{
#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 ) {
	    ;	/* XXX */
	} else {
	    vga_back ();
	}
#else
	putchar ( ERASE );
	putchar ( SPACE );
	putchar ( ERASE );
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
	    if ( c == REBOOT ) {
	    	kb_reset ();
	    }
#endif

	    if ( c == KILL ) {
		while ( i ) {
		    backspace();
		    i--;
		}
		p = buf;
		continue;
	    }

	    if ( c == ERASE || c == DELETE ) {
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
	/* On the arm/BBB - this is all we got ! */
	serial_puts ( buf );
	serial_putc ( '\n' );
#ifdef notdef
	if ( cur_thread->con_mode == SERIAL ) {
	    serial_puts ( buf );
	    serial_putc ( '\n' );
	}
#endif

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

/* Only used by printf()/vprintf() (probably)
 * Unlike real puts() does not add newline
 *
 * Note that if we do printf from interrupt level
 * using the orange pi spinlock can hang the system.
 * This was kind of an experiment during orange pi
 * multicore testing.
 */
void
console_puts ( char *buf )
{
#ifdef BOARD_ORANGE_PI
 #ifdef WANT_PUTS_LOCK
	h3_spin_lock ( 3 );
 #endif
	serial_puts ( buf );
 #ifdef WANT_PUTS_LOCK
	h3_spin_unlock ( 3 );
 #endif
#else
	/* Everything else but Opi */
	serial_puts ( buf );
#endif

#ifdef ARCH_X86
	if ( cur_thread->con_mode == SIO_0 )
	    sio_puts ( 0, buf );
	else
	    vga_puts ( buf );
#endif
}

/* ------------------------------------------ */

/* XXX - for now, panic routines are here.
 */


/* This is a real panic, as distinguished
 * from calls to dpanic(), which are just
 * for debugging, and should be removed in
 * production version.
 */
static void
kyu_panic ( char *msg, int spin )
{
	// keep messages tidy.
	INT_lock;

	if ( msg )
	    printf ( "PANIC: %s\n", msg );
	else
	    printf ( "PANIC\n" );

#ifdef PANIC_DEBUG
	panic_debug ();
#endif

	if ( spin )
	    for ( ;; )
		;

	thr_block ( FAULT );
}

/* These two calls allow some probably useless attempt
 * to differentiate between a panic that might just allow
 * the current thread to be suspended, and a panic that
 * indicates a hopeless situation where the processor
 * should just halt (or spin).
 */
void
panic ( char *msg )
{
	kyu_panic ( msg, 0 );
}

void
panic_spin ( char *msg )
{
	kyu_panic ( msg, 1 );
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

/* ------------------------------------------ */
/* ------------------------------------------ */

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
		INT_unlock;
	    if ( c == '-' )	/* mask interrupts */
		INT_lock;
	    if ( c == 'r' )
	    	kb_reset ();
	    if ( c == 't' )
	    	thr_show ();
	    if ( c == 'u' )
	    	thr_show_current ();
	    if ( c == 's' ) {
		printf ( "eip: %8x\n", getip() );
		printf ( "esp: %8x\n", get_sp() );
		printf ( " ef: %8x\n", getfl() );
	    }
	    if ( c == 'd' ) {
	    	dump_l ( (void *) get_sp(), 8 );
	    }
	    if ( c == 'c' ) {	/* continue */
		printf ( "continue from panic\n" );
	    	return;
	    }
	}
}
#endif

/* This is specific to the ARM due to the RAM range test,
 * but it avoids a bunch of annoying data aborts.
 */
static void *
arm_address_fix ( unsigned long addr )
{
	if ( ! valid_ram_address ( addr ) )
	    return (void *) 0;

	/* No odd addresses */
	// addr &= ~1;
	/* This is even better */
	addr &= ~0xf;

	return (void *) addr;
}

static int
mem_prod ( unsigned long *addr, int val )
{
	*addr = val;
	if ( *addr == val )
	    return 1;
	printf ( "Fail at %08x, %08x (found: %08x)\n", addr, val, *addr );
	return 0;
}

#define MTAG(x)	(0xabcd0000 | (unsigned long) (x))

/* Scan a range of memory up to where it fails to
 * act like ram.
 */
void
mem_test ( unsigned long *start, unsigned long *end )
{
	unsigned long *p;

	for ( p = start; p < end; p++ ) {
	    if ( ! mem_prod ( p, 0 ) ) return;
	    if ( ! mem_prod ( p, 0xffffffff ) ) return;
	    if ( ! mem_prod ( p, 0xabcd1234 ) ) return;
	    if ( ! mem_prod ( p, ~0xabcd1234 ) ) return;
	    *p = MTAG(p);
	}
	printf ( "Range all OK\n" );
}

void
mem_verify ( unsigned long *start, unsigned long *end )
{
	unsigned long *p;
	int val;
	int limit = 0;

	for ( p = start; p < end; p++ ) {
	    val = MTAG(p);
	    if ( *p != val )
		printf ( "Fail at %08x, %08x (found: %08x)\n", p, val, *p );
	}
}


/* Shared by code in tests.c
 * Data aborts just waste time and cause frustration
 * when debugging, so we ensure addresses are in ram,
 * Unless type is 'i'.
 */
void
mem_dumper ( int type, char *a_start, char *a_lines )
{
	unsigned long addr;
	void *start;
	int lines;

	if ( strncmp ( a_start, "0x", 2 ) == 0 )
	    addr = atoi ( a_start );
	else
	    addr = hextoi ( a_start );

	lines = atoi ( a_lines );

	if ( type == 'i' ) {
	    /* bypass ram range checks */
	    start = (void *) (addr & ~0xf);
	    type = 'l';
	} else {
	    start = arm_address_fix ( addr );

	    // printf ( "DUMP at %08x\n", start );
	    if ( ! start ) {
		printf ( "Start address not in RAM\n" );
		return;
	    }

	    /* check end address too */
	    addr = (unsigned long) start + 16 * lines - 1;
	    if ( ! valid_ram_address ( addr ) ) {
		printf ( "End address not in RAM\n" );
		return;
	    }
	}

	if ( type == 'b' )
	    dump_b ( start, lines );
	else if ( type == 'w' )
	    dump_w ( start, lines );
	else
	    dump_l ( start, lines );
}

/* The idea here is
 * that we can call this from any place we are interested
 * in examining, interact with this, then type "c"
 * to continue.
 * 
 * Consider whether we can use printf without switching into
 *   polling versus interrupt mode.
 * Also consider ensuring that we have a valid stack.
 */
#define MAXB	64
#define MAXW	4

void
kyu_debugger ( void )
{
	char buf[MAXB];
	char *wp[MAXW];
	int nw;
	void *start;

	for ( ;; ) {
	    printf ( "Kyu, debug> " );
	    getline ( buf, MAXB );
	    if ( ! buf[0] )
	    	continue;

	    nw = split ( buf, wp, MAXW );

	    if ( **wp == 'c' ) {
		printf ( "Resume execution ...\n");
		break;
	    }

	    /* Reboot */
	    if ( **wp == 'R' ) {
		printf ( "Rebooting\n" );
	    	reset_cpu ();
	    }

	    if ( **wp == 'r' ) {
		show_my_regs ();
	    }

	    /* turn on or off thread debugging
	     * think 'v' for verbose or
	     *  'o' to match the illogical test command.
	     */
	    if ( (**wp == 'v' || **wp == 'o') && nw == 2 ) {
	    	int flags = 0;
		if ( nw > 1 )
		    flags = atoi ( wp[1] );
		thr_debug ( flags );
	    }

	    /* Show threads */
	    if ( **wp == 't' && nw == 1 ) {
	    	thr_show ();
	    }

	    if ( **wp == 'u' )
	    	thr_show_current ();

	    if ( **wp == '+' )	/* allow interrupts */
		INT_unlock;

	    if ( **wp == '-' )	/* mask interrupts */
		INT_lock;

	    /* Show thread by name */
	    if ( **wp == 't' && nw == 2 ) {
	    	thr_show_name ( wp[1] );
	    }

	    /* dump memory as bytes */
	    if ( wp[0][0] == 'd' && wp[0][1] == 'b' && nw == 3 ) {
		mem_dumper ( 'b', wp[1], wp[2] );
		continue;
	    }

	    /* dump memory as words */
	    if ( wp[0][0] == 'd' && wp[0][1] == 'w' && nw == 3 ) {
		mem_dumper ( 'w', wp[1], wp[2] );
		continue;
	    }

	    /* dump memory as longs */
	    if ( **wp == 'd' && nw == 3 ) {
		mem_dumper ( 'l', wp[1], wp[2] );
		continue;
	    }
	}
}

/* THE END */
