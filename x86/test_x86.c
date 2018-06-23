/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * test_x86.c
 *
 * Tom Trebisky
 *
 * began as user.c -- 11/17/2001
 * made into tests.c  9/15/2002
 * added to kyu 5/11/2015
 * split out of tests.c  --  6/23/2018
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"
#include "arch/cpu.h"

#include "netbuf.h"

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* X86 specific tests */
/* Split out of Kyu tests.c on 6/23/2018
 * This certainly will not compile as is,
 * this is just a trash pile of code I did not want to
 * simply delete.  Most of it might belong in an x86 specific
 * IO test menu someday.  If we ever return to the x86.
 */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

#ifdef ARCH_X86
extern struct desc48 vector_desc;
extern struct gate vector_table[];

extern char vector_jump_0;
extern char vector_jump_1;
extern char vector_jump_end;

extern struct cqueue *kb_queue;

static void test_cv ( int );
static void test_pci ( int );
static void test_siow ( int );
static void test_sior ( int );
static void test_siol ( int );
extern void test_das ( int );
static void test_keyboard ( int );

void sioserv ( int );

void show_ports ( int, int );
void read_ports ( int, int );
#endif

/* From IO test menu */
#ifdef ARCH_X86
	test_cv,	"cv lockup test",	0,
	test_pci,	"PCI scan",		0,
	test_keyboard,	"Keyboard test",	0,
	test_siow,	"sio write test",	0,
	test_sior,	"sio read test",	0,
	test_siol,	"sio loopback test",	0,
	test_das,	"das16 test",		0,
#endif

/* Used to be included in main loop before each prompt */
#ifdef ARCH_X86
	if ( (getfl() & 0x200) == 0 ) {
	    printf ("Interrupts mysteriously disabled\n");
	    printf ( "flags: %08x\n", getfl() );
	    panic ( "interrupts lost" );
	}

	/* XXX - not the best place for these checks,
	 * but it will do for now.
	 */
	if ( sizeof(struct desc64) != 8 ) {
	    printf ( "desc64 size %d\n", sizeof ( struct desc64 ) );
	    panic ( "desc64");
	}
	if ( sizeof(struct desc48) != 6 ) {
	    printf ( "desc48 size %d\n", sizeof ( struct desc48 ) );
	    panic ( "desc48");
	}
#endif

#ifdef ARCH_X86
void
stack_db ( char *msg )
{
	printf ( "stack_db: %s,\n", msg);
	printf ( "sp = %08x  bp = %08x\n", getsp(), getbp() );
}

void
kb_debug ( int val )
{
	struct sem *kbd_sem;

	/*
	stack_db ( "kb_debug thread");
	*/

	kbd_sem = safe_sem_signal_new ();
	kb_hookup ( 0x3c, kbd_sem );

	for ( ;; ) {
	    sem_block ( kbd_sem );
	    printf ( "KB debug keypress seen\n" );
	}
}
#endif

#ifdef ARCH_X86
#define MAX_VECTOR	256
#define MAX_VJUMP	48

static void
show_vtable_3 ( int iv, void *jp )
{
	long *lp;

	printf ( "V%3d: ", iv );
	dump_v ( jp, 10 );
	lp = (long *) (jp+1);
	printf ( " --> %08x\n", *lp );
}

void
show_vtable_1 ( int iv, int num )
{
	int i;
	int lv;
	unsigned long off;

	lv = iv + num;
	if ( lv > MAX_VECTOR )
	    lv = MAX_VECTOR;

	printf ( " vector_desc: %04x  %04x%04x\n",
	    vector_desc.limit,
	    vector_desc.off_hi,
	    vector_desc.off_lo);

	for ( i=iv; i<lv; i++ ) {
	    printf ( "V%3d: ", i );
	    dump_v ( &vector_table[i],
		sizeof(struct gate) );
	    printf ( " --> %04x%04x\n",
		vector_table[i].off_hi,
		vector_table[i].off_lo );
	    off = vector_table[i].off_hi << 16 |
		    vector_table[i].off_lo;
	    /*
	    show_vtable_3 ( i, (void *) off );
	    */
	}
}

void
show_vtable_2 ( int iv, int num )
{
	void *jp0 = (void *) &vector_jump_0;
	void *jp1 = (void *) &vector_jump_1;
	void *jpe = (void *) &vector_jump_end;
	void *jp;
	long *lp;
	int i, size;
	int lv;

	lv = iv + num;
	if ( lv > MAX_VJUMP )
	    lv = MAX_VJUMP;

	printf ( " vector_jump table:\n");
	size = jp1 - jp0;
	printf ( "%d entries of size = %d\n",
	    (jpe - jp0) / size, size );

	jp = jp0 + iv * size;
	for ( i=iv; i<lv; i++ ) {
	    printf ( "V%3d: ", i );
	    dump_v ( jp, size );
	    lp = (long *) (jp+1);
	    printf ( " --> %08x\n", *lp );
	    jp += size;
	}
}

void
dump_gdt ( struct desc64 *gp )
{
	unsigned long off;

	printf ( "GDT @ %08x\n", (long)gp );

	off = gp->limit;
	off |= (gp->attr_hi & 0x0f) <<16;
	printf ( "  limit = %08x\n", off );

	off = gp->base_hi << 24 |
		gp->base_mid <<16 |
		    gp->base_lo;
	printf ( "  base = %08x\n", off );

	/* attr_hi does include upper 4 bits of limit.
	 */
	printf ( "  attr = %02x %02x\n", gp->attr_hi, gp->attr_lo );
}

void
rom_search ( void )
{
    	char *p;
	unsigned long w;

	/* search ROM extension space at 16k (0x4000) increments */
	for ( p = (char *) 0xc0000; p < (char *) 0xf0000; p += 0x4000 ) {
		printf ("%08x", p);
		w = *((unsigned long *) p);
	    	if ( w == 0xffffffff )
		    printf (" Nope\n");
		else
		    printf (" %08x\n", w);
	}
}

#ifdef ARCH_X86
void
read_ports ( int start, int count )
{
	int p;
	volatile int x;

    	for ( ;; ) {
	    p = start;
	    while ( count -- )
		x = inb ( p++ );
	}
}

void
show_ports ( int start, int count )
{
	int n = (count + 15) / 16;
	int i;

	while ( n-- ) {
	    printf ( "%03x: ", start );
	    for ( i = 0; i < 16; i++ ) {
		printf ( "%02x", inb ( start++ ) );
		printf ( "%c", i == 15 ? '\n' : ' ' );
	    }
	}
}

void
show_port ( int start )
{
	printf ( "%03x: %02x\n", start, inb ( start ) );
}

static void
test_pci ( int xx )
{
#ifdef WANT_PCI
	pci_scan ( 0 );
#endif
}

#endif	/* ARCH_X86 */

#endif

/* Used to be part of main menu */
#ifdef ARCH_X86
	    if ( **wp == 'R' ) {
	    	/* Reboot the machine */
		printf ( "Rebooting\n" );
	    	kb_reset ();
	    }

	    if ( **wp == 'r' ) {
		rom_search ();
		continue;
	    }

	    if ( **wp == 's' ) {
	    	sio_show ( 0 );
	    	sio_show ( 1 );
	    }

	    if ( **wp == 'l' && nw == 2) {
		read_ports ( atoi(wp[1]), 256 );
	    }

	    if ( **wp == 'p' && nw == 2) {
		show_ports ( atoi(wp[1]), 16 );
	    }
	    if ( **wp == 'p' && nw == 3) {
		show_ports ( atoi(wp[1]), atoi(wp[2]) );
	    }
	    if ( **wp == 'g' ) {
		struct desc48 gdtbuf;
		struct desc48 idtbuf;
		struct desc64 *gdtp;
		unsigned long offset;

	    	getidt ( &idtbuf );
		printf ("idt: limit: %04x, base: %04x%04x\n",
		    idtbuf.limit, idtbuf.off_hi, idtbuf.off_lo);

	    	getgdt ( &gdtbuf );
		printf ("gdt: limit: %04x, base: %04x%04x\n",
		    gdtbuf.limit, gdtbuf.off_hi, gdtbuf.off_lo);

		offset = gdtbuf.off_hi << 16 | gdtbuf.off_lo;
		dump_w ( (void *)offset, 3 );
		gdtp = (struct desc64 *) offset;
		dump_gdt ( &gdtp[2] );
		dump_gdt ( &gdtp[3] );

	    }
	    if ( **wp == 'k' ) {
		n = 2;
		if ( nw > 1 )
		    n = atoi ( wp[1] );
		test_keyboard ( n );
	    }

	    if ( **wp == 'x' && nw == 1 ) {
		if ( get_console_type () == VGA ) {
		    printf ( "Switch console to serial 0\n" );
		    sio_baud ( 0, 38400 );
		    console_sio0 ();
		} else {
		    printf ( "Switch console to vga\n" );
		    console_vga ();
		}
	    }
	    if ( **wp == 'c' && nw == 1 ) {
	    	n = vga_get_cursor ();
		printf ( "Cursor at: %d\n", n );
	    }
	    if ( **wp == 'c' && nw == 2 ) {
	    	n = vga_get_cursor ();
		printf ( "Cursor was at: %d\n", n );
		n = atoi ( wp[1] );
		vga_set_cursor ( n );
	    	n = vga_get_cursor ();
		printf ( "Cursor now at: %d\n", n );
	    }

	    if ( **wp == 'm' && nw == 4 ) {
	    	int start, end, mode;
		start = atoi ( wp[1] );
		end = atoi ( wp[2] );
		mode = atoi ( wp[3] );
		vga_config_cursor ( start, end, mode );
	    }

	    if ( **wp == 'v' ) {
		int iv;

		iv = 0;
		if ( nw > 1 )
		    iv = atoi ( wp[1] );
		if ( iv >= MAX_VECTOR )
		    iv = MAX_VECTOR-1;

		/* "v2" command
		 */
		if ( (*wp)[1] == '2' ) {
		    show_vtable_2 ( iv, 10 );
		}

		/* "v" command
		 */
		else {
		    show_vtable_1 ( iv, 10 );
		}
	    }
#ifdef notdef
	    /* sort of like a keyboard test.
	     * useful with a serial port to figure out
	     * what gets sent when various keys get pressed.
	     */
	    if ( **wp == 'k' ) {
		int cc = getchare ();
		printf ( "Received: %02x\n", cc );
	    }
	    if ( **wp == 'w' && nw == 2 ) {
		int timer_rate = timer_rate_get ();
		nw = atoi(wp[1]);
		printf ("Delay for %d seconds\n", nw );

		thr_delay ( nw * timer_rate );
	    }

	    if ( **wp == 's' ) {
		static int screen;

		/*
		screen = 1 - screen;
		*/
		screen = (screen+1) % 8;
	    	vga_screen ( screen );
		if ( screen )
		    printf ( "Screen %d\n", screen );
	    }
#endif

#endif

#ifdef ARCH_X86
/* -------------------------------------------- */
/* Serial i/o
 */

#define SIO_DELAY	200

/*
#define SIO_BAUD	56000
#define SIO_BAUD	38400
#define SIO_BAUD	1200
*/
#define SIO_BAUD	9600

#define WRITE_PORT	0
#define WRITE_PORT_ALT	1
#define READ_PORT	0
#define LOOP_PORT	0

static void
test_siow ( int count )
{
	int n;
	int x;

	printf ( "Serial write test: " );

	sio_baud ( WRITE_PORT, SIO_BAUD );
	sio_puts ( WRITE_PORT, "Skidoo sio 0\n" );

	/*
	sio_baud ( WRITE_PORT_ALT, SIO_BAUD );
	sio_puts ( WRITE_PORT_ALT, "Skidoo sio 1\n" );
	*/

	/* clean out the local keyboard queue.
	 */
	while ( kb_check () )
	    (void) kb_raw ();

	for ( n=0;; n++ ) {
	    printf ( "#%d", n );
	    thr_delay ( SIO_DELAY );
	    if ( n % 2 ) {
		sio_puts ( WRITE_PORT, "port 0 sio test A\n");
		/*
		sio_puts ( WRITE_PORT_ALT, "port 1 sio test A\n");
		*/
	    } else {
		sio_puts ( WRITE_PORT, "port 0 sio test  B\n");
		/*
		sio_puts ( WRITE_PORT_ALT, "port 1 sio test  B\n");
		*/
	    }

	    printf ( " K" );

	    /* any keystroke will stop this,
	     * but watch out for spurious up
	     * keystrokes.
	     */
	    if ( kb_check () ) {
		x = kb_raw();
		/*
		printf ( " %02x ", x );
		*/
		if ( ! (x & 0x80) )
		    break;
	    }
	    printf ( "B\n" );
	}
	/* NOTREACHED */
}

/* ------ */

static int sior_running;

static void
sior_func ( int count )
{
	int c;

	while ( sior_running ) {
	    c = sio_getc ( READ_PORT );
	    printf ( "Got: %02x %c\n", c, c );
	    if ( c == 0x1b ) /* ESC */
		break;
	}
	printf ( "Sio read thread done\n" );
}

static void
test_sior ( int count )
{
	struct thread *tp;

	printf ( "Serial read test: " );

	sior_running = 1;
	sio_baud ( READ_PORT, SIO_BAUD );

	tp = safe_thr_new ( "sior", sior_func, (void *) 1, PRI_TEST, 0 );

	/* since the read is going on in another
	 * thread, we can block here waiting for
	 * a keystroke.  And since the thread is
	 * mostly blocked waiting for serial i/o
	 * we can be pretty sure of actually running.
	 */

	/* any keystroke will stop this,
	 */
	while ( sior_running ) {
	    (void) kb_read ();
	    /*
	    thr_kill ( tp );
	    */
	    sior_running = 0;
	}

	/* XXX - without the following join,
	 * after a thr_kill, we hang!
	 */
	thr_join ( tp );
}

/* ------ */


static int siol_done;

#define MAX_LOOP	100

static void
siol_func ( int count )
{
	char *test = "Leaping lizzards!\n"; 
	int i;

	if ( count ) {
	    printf ( "Sending %d lines for loopback\n", count );
	    for ( i=0; i<count; i++ ) {
		if ( siol_done )
		    break;
		sio_puts ( LOOP_PORT, test );
	    }
	} else {
	    printf ( "Sending forever for loopback\n" );
	    for ( ;; ) {
		if ( siol_done )
		    break;
		sio_puts ( LOOP_PORT, test );
	    }
	}
	sio_puts ( LOOP_PORT, ".\n" );
}

static void
test_siol ( int count )
{
	char buf[64];
	int i;

	sio_baud ( LOOP_PORT, SIO_BAUD );

	printf ( "Serial loopback test: (%d)\n", SIO_BAUD );
	sio_crmod ( LOOP_PORT, 0 );

	siol_done = 0;

	(void) safe_thr_new ( "siol", siol_func, (void *) MAX_LOOP,
	    PRI_TEST, 0 );

	thr_show ();

#ifdef notdef
	for ( i=0;;i++ ) {
	    sio_gets ( LOOP_PORT, buf );
	    printf ( "Got (%4d): %s", i, buf );
	    if ( buf[0] = '.' )
	    	break;
	}
#endif
}

#ifdef notdef
static void
test_siol ( int count )
{
	char *test = "Leaping lizzards!\n"; 
	char buf[64];
	int i;

	sio_baud ( LOOP_PORT, SIO_BAUD );
	printf ( "Serial loopback test: (%d)\n", SIO_BAUD );
	sio_crmod ( LOOP_PORT, 0 );

	tp = safe_thr_new ( "siol", siol_func, (void *) 1, PRI_TEST, 0 );

	for ( i=0; i<MAX_LOOP; i++ ) {
	    sio_puts ( LOOP_PORT, test );
	    sio_gets ( LOOP_PORT, buf );
	    printf ( "Got: %s", buf );
	    /*
	    char *p;
	    p = buf;
	    while ( *p ) {
	    	printf ( "Got: %02x, %c\n", *p, *p );
		p++;
	    }
	    */
	}

}
#endif
#endif /* ARCH_X86 */

#ifdef ARCH_X86
/* The following test is keyboard intensive
 * (using keyboard driver CV stuff).
 * Fails with serial console.
 */
static int cv_done_flag;

static void
func_cv ( int xx )
{
	int count;

	thr_show ();
	printf ( "Waiting!\n");

	for ( count = 6; count; count-- ) {
	    thr_delay ( 5 * timer_rate );
	    printf ( "Ticked!\n");
	}
	cv_done_flag = 1;
	printf ( "Done\n" );
}

static void
test_cv ( int xx )
{
	int c;

	cv_done_flag = 0;
	kb_debug_on ();
	(void) safe_thr_new ( 0, func_cv, (void *) 0, 11, 0 );

	for ( ;; ) {
	    c = kb_read ();
	    printf ("kb Got: %02x %c\n", c, c );
	    if ( c == 0x1b )
		break;
	    if ( cv_done_flag )
		break;
	}
	kb_debug_off ();
}
#endif

#ifdef ARCH_X86
static void kb_test_1 ( void );
static void kb_test_2 ( void );
static void kb_test_3 ( void );
static void kb_test_4 ( void );
static void kb_test_5 ( void );

static void
test_keyboard ( int test )
{
	printf ( "Running keyboard test %d\n", test );

	switch ( test ) {
	    case 1:
		kb_test_1 ();
	    	break;
	    case 2:
		kb_test_2 ();
	    	break;
	    case 3:
		kb_test_3 ();
	    	break;
	    case 4:
		kb_test_4 ();
	    	break;
	    case 5:
		kb_test_5 ();
	    	break;
	    default:
		printf ( "No such test\n" );
	    	break;
	}
}

#define ESC	0x1b

/* show translated characters
 * control characters produce hieroglyphics.
 * ESC escapes.
 */
static void
kb_test_1 ( void )
{
	int i, t;

	for ( ;; ) {
	    i = kb_raw ();
	    if ( i & 0x80 )
	    	continue;
	    t = kb_tran ( i );
	    if ( t == ESC )
	    	break;
	    if ( t == '\r' ) t = '\n';
	    if ( t == '\b' ) {
		vga_back ();
		continue;
	    }
	    if ( t )
		vga_putc ( t );
	}
}

/* show translated characters
 * uses the getchar and putc calls that
 * we expect to give to the world.
 * shows control characters as ^x
 * ESC gets us out of here.
 */
static void
kb_test_2 ( void )
{
	int c;

	for ( ;; ) {
	    c = getchare ();
	    if ( c == ESC )
	    	break;
	    if ( c == '\r' ) {
	    	vga_putc ( '\n' );
	    }
	    if ( c < 0x20 ) {
		vga_putc ( '^' );
		vga_putc ( c | 0x40 );
	    } else {
		vga_putc ( c );
	    }
	}
}

/* Here is the down and dirty keyboard scooper.
 * We get to see all the raw keystroke scan codes.
 * ESC does get us out of here.
 */
static void
kb_test_3 ( void )
{
	int i, n, t;

	for ( ;; ) {
	    i = kb_raw ();
	    kb_lookup ( i, &n, &t );
	    if ( n == ESC )
	    	break;
	    vga_puts ( "Keyboard: " );
	    vga_hex4 ( i );
	    if ( i & 0x80 ) {	/* upstroke */
		vga_puts ( "\n" );
		continue;
	    }
	    vga_puts ( " " );
	    vga_hex2 ( n );	/* ascii (maybe) */
	    vga_puts ( " " );
	    vga_hex2 ( t );	/* type */
	    vga_puts ( "\n" );
	}
}

/* look at interrupt queue.
 */
static void
kb_test_4 ( void )
{
	int n, c;

	if ( n = cq_count ( kb_queue ) )
	    printf ( "Keyboard queue: %d characters\n", n );
	else
	    printf ( "Keyboard queue empty\n" );

	while ( n = cq_count ( kb_queue ) ) {
	    printf ( "count = %d", n );
	    c = cq_remove ( kb_queue );
	    printf ( " %02x\n", c );
	}
	printf ( "Waiting\n" );

	for ( ;; ) {
	    while ( ! cq_count ( kb_queue ) )
		;
	    c = cq_remove ( kb_queue );
	    printf ( " %02x\n", c );
	    if ( kb_tran ( c ) == ESC )
	    	break;
	}
}

/* Sort of a junkyard.
 */
static void
kb_test_5 ( void )
{
	static long zoot;
	int i;

	vga_puts ( " cursor at: ");
	vga_hex4 ( vga_get_cursor ());
	vga_puts ( "\n" );

	printf ( "zoot: %08x\n", &zoot );

	for ( i=0; i<4; i++ ) {
	    vga_puts ( "Hello Paul " );
	    vga_hex4 ( i );
	    vga_puts ( "\n" );
	}

#ifdef notdef
	flash ();
#endif
}

#endif	/* ARCH_X86 */

/* THE END */
