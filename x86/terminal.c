/* terminal.c
 * Tom Trebisky  11/17/2001
 */

#include "kyulib.h"
#include "thread.h"

#define PRI_BOSS	30

void terminal_init ( void );

static void term_in ( int );
static void term_out ( int );

static int term_port = 0;
static int term_baud = 9600;

void
terminal_init ( void )
{
	sio_baud ( term_port, term_baud );
	sio_crmod ( term_port, 0 );

	printf ("Terminal running on port %d, %d baud\n",
	    term_port, term_baud );

	(void) safe_thr_new ( "te_i",  term_in, (void *)0, PRI_BOSS, 0 );
	(void) safe_thr_new ( "te_o", term_out, (void *)0, PRI_BOSS, 0 );
}

static void
term_in ( int xx )
{
	int c;

	for ( ;; ) {
	    c = sio_getc ( term_port );
	    if ( c == '\r' )
	    	c = '\n';
	    vga_putc ( c );
	}
}

static void
term_out ( int xx )
{
	int c;

	for ( ;; ) {
	    c = kb_read ();
	    sio_putc ( term_port, c );
	    if ( c == '\r' )
		sio_putc ( term_port, '\n' );
	}
}

/* THE END */
