/* tests.c
 * Tom Trebisky
 *
 * began as user.c -- 11/17/2001
 * made into tests.c  9/15/2002
 * added to kyu 5/11/2015
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"

#define HZ	100

extern struct thread *cur_thread;

#define PRI_DEBUG	5
#define PRI_TEST	10
#define PRI_WRAP	20

#define USER_DELAY	10
#define AUTO_DELAY	1

static int usual_delay = USER_DELAY;

/* Don't run these tests automatically
 */
static void test_sort ( int );
static void test_ran ( int );

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

static void test_cv1 ( int );

/* First automatic test follows ... */

static void test_basic ( int );

#ifdef WANT_SETJMP
static void test_setjmp ( int );
#endif

static void test_timer ( int );
static void test_delay ( int );
static void test_contin1 ( int );
static void test_contin2 ( int );

static void test_thread0 ( int );
static void test_thread1 ( int );
static void test_thread2 ( int );
static void test_thread3 ( int );
static void test_thread4 ( int );
static void test_thread5 ( int );
static void test_fancy ( int );
static void test_easy ( int );
static void test_hard ( int );
static void test_join ( int );
static void test_mutex ( int );

/* XXX should be in arch dependent menu */
static void test_gpio ( int );
static void test_fault ( int );

#ifdef WANT_NET
static void test_net ( int );
static void test_netarp ( int );
static void test_netping ( int );
static void test_bootp ( int );
static void test_dns ( int );
static void test_arp ( int );
static void test_netshow ( int );
#endif

struct test {
	tfptr	func;
	char	*desc;
	int	arg;
};

/* These are the tests we run in the automatic regression set
 */
struct test std_test_list[] = {
	test_basic,	"Basic tests",		0,

#ifdef WANT_SETJMP
	test_setjmp,	"Setjmp test",		0,
#endif

	test_timer,	"Timer test",		5,
	test_delay,	"Delay test",		5,
	test_contin1,	"Continuation test",	0,
	test_contin2,	"Delay (cont) test",	5,

	test_thread0,	"Thread starting test",		0,
	test_thread1,	"Multiple thread test",		4,
	test_thread2,	"Reentrant thread test",	0,

	test_thread3,	"Ping-pong Semaphore test",	0,
	test_thread4,	"Silly thread test",		8,
	test_thread5,	"Multi semaphore test",		6,

	test_fancy,	"Fancy semaphore test",	5,
	test_easy,	"Easy interrupt test",	5,
	test_hard,	"Hard interrupt test",	5,
	test_cv1,	"CV test",		0,
	test_join,	"Join test",		0,
	test_mutex,	"Mutex test",		0,
	test_fault,	"Fault test",		0,
	test_gpio,	"BBB gpio test",	0,
	0,		0,			0
};

struct test io_test_list[] = {
	test_sort,	"Thread sort test",	5,
	test_ran,	"Random test",		0,
#ifdef ARCH_X86
	test_cv,	"cv lockup test",	0,
	test_pci,	"PCI scan",		0,
	test_keyboard,	"Keyboard test",	0,
	test_siow,	"sio write test",	0,
	test_sior,	"sio read test",	0,
	test_siol,	"sio loopback test",	0,
	test_das,	"das16 test",		0,
#endif
	0,		0,			0
};

#ifdef WANT_NET 
struct test net_test_list[] = {
	test_netshow,	"Net show",		0,
	test_netarp,	"ARP ping",		0,
	test_bootp,	"test BOOTP",		0,
	test_netping,	"Net ping",		0,
	test_dns,	"Test DNS",		0,
	test_arp,	"one gratu arp",	1,
	test_arp,	"8 gratu arp",		8,
	test_net,	"Net init",		0,
	0,		0,			0
};
#endif

struct test *cur_test_list = std_test_list;

static int test_debug = 0;

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

	kbd_sem = safe_sem_new ( CLEAR );
	kb_hookup ( 0x3c, kbd_sem );

	for ( ;; ) {
	    sem_block ( kbd_sem );
	    printf ( "KB debug keypress seen\n" );
	}
}
#endif

int
is_irq_disabled ( void )
{
	if ( get_cpsr() & 0x80 )
	    return 1;
	return 0;
}

/*
 * -------------------------------------------------
 */

struct test_info {
	tfptr func;
	int arg;
	struct sem *sem;
	int times;
};

/* This gets launched as a thread to run
 * each of the basic tests */
void
basic_wrapper ( int arg )
{
	struct test_info *ip = (struct test_info *) arg;

	(*ip->func) ( ip->arg );
	sem_unblock ( ip->sem );
}

/* lauch a wrapper thread to run a test.
 * the main purpose of doing this is to be
 * notified when the test finishes.
 *
 * We almost don't need another thread here.
 * The new scheduler will pass control to the higher
 * priority thread, immediately -- effectively never
 * returning from thr_new.
 */
void
run_test ( tfptr func, int arg )
{
	struct test_info info;

	info.func = func;
	info.arg = arg;
	info.sem = safe_sem_new ( CLEAR );
	info.times = 1;	/* ignored */

	(void) safe_thr_new ( 
	    "bwrapper", basic_wrapper, (void *) &info, PRI_WRAP, 0 );

	sem_block ( info.sem );
	sem_destroy ( info.sem );

#ifdef ARCH_X86
	if ( (getfl() & 0x200) == 0 ) {
	    printf ("Interrupts mysteriously disabled\n");
	    printf ( "flags: %08x\n", getfl() );
	    panic ( "interrupts lost" );
	}
#endif
}

/*
 * -------------------------------------------------
 * -------------------------------------------------
 */

static void
all_tests ( int nl )
{
	int i, t, nt;

	if ( cur_test_list != std_test_list )
	    return;

	nt = sizeof ( std_test_list ) / sizeof(struct test) - 1;
	if ( nl < 1 )
	    nl = 1;

	for ( i=0; i<nl; i++ )
	    for ( t=0; t<nt; t++ )
		(*std_test_list[t].func) ( std_test_list[t].arg );
}

/* A global that endless tests can check to discover
 * that they ought to quit.
 */
static int test_running;

/* This gets launched as a thread to run each test
 *  repeating the test as many times as indicated.
 */
static void
wrapper ( int arg )
{
	struct test_info *ip = (struct test_info *) arg;
	int i;

	for ( i=0; i< ip->times; i++ ) {
	    if ( ! test_running )
		break;
	    (*ip->func) ( ip->arg );
	}

	/*
	sem_unblock ( ip->sem );
	*/
}

static void
single_test ( struct test *tstp, int times )
{
	struct test_info info;

	info.func = tstp->func;
	info.arg = tstp->arg;
	/*
	info.sem = safe_sem_new ( CLEAR );
	*/
	info.times = times;

	test_running = 1;

	(void) safe_thr_new ( 
	    "wrapper", wrapper, (void *) &info, PRI_WRAP, 0 );

/* This nice synchronization does not give us
 * the desired result when a thread faults.
 * So we launch the test and return to the
 * command line.
 */
#ifdef notdef
	/* Or we could do a join */
	sem_block ( info.sem );
	sem_destroy ( info.sem );
#endif
}

/* ---------------------------------------------------------------
 */

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
#endif

void
malloc_test ()
{
	char *p;

	p = malloc ( 1024 );
	printf ( "Malloc gives: %08x\n", p );
	memset ( p, 0, 1024 );

	free ( p );

	p = malloc ( 1024 );
	printf ( "Malloc gives: %08x\n", p );
	memset ( p, 0, 1024 );
}

static void
help_tests ( struct test *tp, int nt )
{
	int i;

	printf ( "%d tests\n", nt );

	for ( i=0; ; i++ ) {
	    if ( ! tp->desc )
		break;
	    printf ( "Test %2d: %s\n", i+1, tp->desc );
	    ++tp;
	}
}

/* Any old thing */
static void
x_test ( void )
{
	printf ( "A %08x\n", cur_thread->regs );
	printf ( "B %08x\n", cur_thread->regs.regs );
	printf ( "C %08x\n", &cur_thread->regs );
	printf ( "D %08x\n", cur_thread );
	printf ( "E %08x\n", cur_thread->iregs );
	printf ( "F %08x\n", &cur_thread->iregs );
}

static void
bbb_blinker ( int xx )
{
	/* Writing a "1" does turn the LED on */
	gpio_led_set ( 1 );
	thr_delay ( 100 );
	gpio_led_set ( 0 );
}

static struct thread *bbb_tp;

static void
start_bbb_blink ( void )
{
	gpio_led_init ();
	bbb_tp = thr_new_repeat ( "blinker", bbb_blinker, 0, 10, 0, 1000 );
}

static void
stop_bbb_blink ( void )
{
	printf ( "Stop the blink\n" );
	thr_repeat_stop ( bbb_tp );
}

#define MAXB	64
#define MAXW	4

static void
tester ( void )
{
	char buf[MAXB];
	char *wp[MAXW];
	int n, nw, nl;
	int nt, nnt;
	int i;
	char *p;
	struct test *tp;
	char *desc;

	/* Allow the lower priority user thread to
	 * run (and exit), not that this matters all
	 * that much, but it does make debugging less
	 * confusing.
	 */
	thr_yield ();

#ifdef ARCH_X86
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

	for ( ;; ) {

	    for ( nt = 0, tp = cur_test_list; tp->desc; ++tp )
		++nt;

#ifdef WANT_NET
	    for ( nnt = 0, tp = net_test_list; tp->desc; ++tp )
		++nnt;
#endif

	    printf ( "Kyu, ready> " );
	    getline ( buf, MAXB );
	    if ( ! buf[0] )
	    	continue;

	    nw = split ( buf, wp, MAXW );

	    if ( **wp == 'h' ) {
		help_tests ( cur_test_list, nt );
	    }

	    /* Select special IO test menu */
	    if ( **wp == 'i' ) {
		printf ( "select io test menu\n" );
	    	cur_test_list = io_test_list;
	    }
	    /* Restore standard test menu */
	    if ( **wp == 'q' ) {
		printf ( "select standard test menu\n" );
	    	cur_test_list = std_test_list;
	    }

#ifdef notdef
	    /* Silly and useless now */
	    if ( **wp == 'e' ) {
	    	printf ( "nw = %d\n", nw );
	    	printf ( "usual delay = %d\n", usual_delay );
		for ( i=0; i<nw; i++ ) {
		    printf ( "%d: %s\n", i, wp[i] );
		}
	    }
#endif
	    if ( **wp == 'e' ) {
		printf ( "Test told to stop\n" );
		test_running = 0;
	    }

	    if ( **wp == 'y' ) {
		kyu_debugger ();
	    }

#ifdef ARCH_ARM
	    if ( **wp == 'r' ) {
	    	/* restart the software */
		printf ( "Restarting\n" );
	    	kyu_startup ();
	    }

	    if ( **wp == 'R' ) {
	    	/* Reboot the machine */
		printf ( "Rebooting\n" );
	    	reset_cpu ();
	    }

	    if ( **wp == 's' ) {
		show_my_regs ();
	    }

	    if ( **wp == 'x' ) {
		x_test ();
	    }

#endif

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

#endif

	    /* Run BBB blink test */
	    if ( **wp == 'b' && nw == 1 ) {
		start_bbb_blink ();
	    }

	    /* Stop BBB blink test */
	    if ( **wp == 'b' && nw == 2 ) {
		stop_bbb_blink ();
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'b' && nw == 3 ) {
	    	dump_b ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'w' && nw == 3 ) {
	    	dump_w ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( **wp == 'd' && nw == 3 ) {
	    	dump_l ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( **wp == 'w' && nw == 2 ) {
		nw = atoi(wp[1]);
		printf ("Delay for %d seconds\n", nw );
		thr_delay ( nw * HZ );
	    }

#ifdef notdef
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

#ifdef WANT_NET
	    /* network test submenu
	     */
	    if ( **wp == 'n' && nw == 1 ) {
		help_tests ( net_test_list, nnt );
	    }
	    if ( **wp == 'n' && nw > 1 ) {
		n = atoi ( wp[1] );

		if ( n < 1 || n > nnt ) {
		    printf ( " ... No such test.\n" );
		    continue;
		}

		/* run single test ...
		 */
		(*net_test_list[n-1].func) ( net_test_list[n-1].arg );
	    }
#endif

#ifdef ARCH_X86
	    if ( **wp == 'l' && nw == 2) {
		read_ports ( atoi(wp[1]), 256 );
	    }

	    if ( **wp == 'p' && nw == 2) {
		show_ports ( atoi(wp[1]), 16 );
	    }
	    if ( **wp == 'p' && nw == 3) {
		show_ports ( atoi(wp[1]), atoi(wp[2]) );
	    }
#endif

	    if ( **wp == 'o' ) {
		if ( nw > 1 )
		    thr_debug ( atoi (wp[1]) );
		else
		    thr_debug ( 0 );
	    }

	    /* Show threads */
	    /* Conflicts with t for test mode below */
	    if ( **wp == 't' && nw == 1 ) {
	    	thr_show ();
	    }

	    if ( **wp == 'u' && nw == 2 ) {
	    	thr_show_name ( wp[1] );
	    }

	    /* Run a test or tests */
	    if ( **wp == 't' && nw > 1 ) {
		n = atoi ( wp[1] );

		nl = 1;
		if ( nw == 3 )
		    nl = atoi ( wp[2] );
		if ( nl < 1 )
		    nl = 1;

		if ( n == 0 ) {
		    all_tests ( nl );
		    continue;
		}

		if ( n < 1 || n > nt ) {
		    printf ( " ... No such test.\n" );
		    continue;
		}

		desc = cur_test_list[n-1].desc;
		if ( nl > 1 ) {
		    printf ( "looping test %d (%s), %d times\n", n, desc, nl );
		    usual_delay = AUTO_DELAY;
		} else {
		    printf ( "Running test %d (%s)\n", n, desc );
		    usual_delay = USER_DELAY;
		}

		/* run single test, perhaps several times
		 */
		single_test ( &cur_test_list[n-1], nl );
	    }

	    /* provoke a divide by zero */
	    if ( **wp == 'z' ) {
		volatile int a = 1;
		int b = 0;

		printf ("Lets try a divide by zero ...\n");
		a = a / b;
		printf ("... All done!\n");
	    }

	    if ( **wp == 'm' ) {
		malloc_test ();
	    }

	    /* cpsw test */
	    if ( **wp == 'c' ) {
		if ( nw < 2 )
		    eth_test ( 0 );
		else
		    eth_test ( atoi(wp[1]) );
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
#endif

#ifdef ARCH_X86
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
#endif


#ifdef notdef
	    printf ( "%s\n", buf );
	    for ( i=0; i<nw; i++ ) {
		printf ("word %d: %s", i+1, wp[i] );
		p = wp[i];
		if ( *p >= '0' && *p <= '9' ) {
		    n = atoi ( p );
		    printf ( ", val = %d", n );
		}
		printf ( "\n" );
	    }
#endif
	}
}

/* The test thread starts here.
 * wrapper for the above.
 * Here so we can examine a simple stack right after
 * this thread gets launched.
 */
void
test_main ( int xx )
{
	/*
	thr_show ();
	show_my_regs ();
	spin ();
	*/

	tester ();
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

/* -------------------------------------------------
 */

static volatile int ez;

void f_ez ( int arg )
{
	ez = arg;
}

/* -------------------------------------------------
 */

/* Here is a thread to run a sequence of diagnostics.
 * At one time, this was the whole show (up until
 * we coded up an interactive test facility).
 * Being historical, it predates semaphores,
 *  and it would be a bad idea to recode it to use them.
 * But they crept in anyhow.
 *
 * It is a good basic sanity test, checking:
 *  1) that we can launch a thread.
 *  2) that we can use the thread launcher,
 *     (which uses semaphores)
 */
static void
test_basic ( int xx )
{
	int i;
	int arg = 123;

	/*
	cpu_enter ();
	kyu_debugger ();
	*/

	if ( is_irq_disabled() )
	    printf ("basic -- no IRQ\n" );

	printf ( "Basic tests thread started.\n");
	/*
	printf ( "  (This is good in itself,\n");
	printf ( "   as it means a thread will start.)\n");
	*/

	thr_yield ();
	printf ( "OK, thr_yield doesn't hang.\n");

	ez = 0;
	(void) safe_thr_new ( "ez", f_ez, (void *)arg, PRI_TEST, 0 );

	for ( i=0; i<10000; i++ ) {
	    if ( ez == arg )
		break;
	}

	if ( ez == arg ) {
	    printf ( "Wow, preemptive sheduling works!\n" );
	} else {
	    printf ( "Preemptive sheduling doesn't work yet.\n" );
	    printf ( " (Don't sweat this, ... for now.)\n" );
	}

	printf ( "Ready! ");
	ez = 0;
	(void) safe_thr_new ( "ez", f_ez, (void *)arg, PRI_TEST, 0 );

	/* XXX - At this time, we must call thr_yield() in
	 * the following loop, or the "ez" thread will
	 * never run, someday this will change (and is
	 * a great test of fully preemptive scheduling.
	 */
	for ( i=0; i<10000; i++ ) {
	    thr_yield ();
	    if ( ez == arg )
		break;
	}

	if ( ez != arg )
	    panic ( "start thread" );

	printf ("Set (%d) ", i );

	run_test ( f_ez, arg );

	if ( ez != arg )
	    panic ( "test thread" );

	printf (" Go!!\n");

#ifdef notdef
	/* OK, now we are ready to start tests in  earnest.
	 */
	run_test ( test_timer, 5 );
	run_test ( test_delay, 5 );
	run_test ( test_contin, 0 );

	run_test ( test_thread0, 0 );
	run_test ( test_thread1, 4 );
	run_test ( test_thread2, 0 );
	run_test ( test_thread3, 0 );
	run_test ( test_thread4, 8 );
	run_test ( test_thread5, 6 );
	run_test ( test_fancy, 5 );

	run_test ( test_easy, 5 );
	run_test ( test_hard, 5 );
#endif

	printf ( " ........ Basic diagnostics OK\n" );
}

/*
 * -------------------------------------------------
 * individual tests follow ...
 * -------------------------------------------------
 *
 */

/* --------------------------------------------
 * Timer test.
 *
 * This tests that we are getting timer interrupts
 *  and that the timer hookup call works.
 *
 * Gets ugly if we have a serial console and try
 * to do printf from interrupt level!
 *
 */
static volatile int ticks;

static void my_tick1 ( void )
{
	static int count = 0;

	if ( (++count % usual_delay) == 0 ) {
	    ++ticks;
	    /*
	    printf ( "Tick %d %08x %08x\n", ticks, getfl(), getsp() );
	    */
	    printf ( " %d", ticks );
#ifdef notdef
	    /* This routine is called at interrupt level
	     * (hence with interrupts masked).
	     * If we do the following (unmask interrupts and then
	     * spin so as to stay in the interrupt routine), we will
	     * get endless nested interrupts.
	     */
	    sti ();
	    for ( ;; ) ;
#endif
	}
}

static void
test_timer ( int count )
{
	printf ("Now let's test the timer.\n");

	ticks = 0;
	timer_hookup ( my_tick1 );

	while ( ticks < count )
	    ;

	timer_hookup ( 0 );
	printf ( " ... OK!\n" );
}

/* --------------------------------------------
 */

/* This test is not so hard.
 * We do expect activations from interrupt timer events,
 * but the purpose here is to test the thr_delay() call.
 */

void
test_delay ( int count )
{
	int tick = 0;
	int i;

	printf ( "Now let's test thread delays.\n" );

	for ( i=0; i<count; i++) {
	    thr_delay ( usual_delay );
	    tick++;
	    printf ( " %d", tick );
	}

	printf ( " ... OK!\n" );
}

/* --------------------------------------------
 * Single continuation test.
 */

static void alarm ( int );
static void slave1 ( int );
static void slave2 ( int );

static struct thread *cont_thread;
static struct thread *cont_slave;

static volatile int ready;

/* This little thread is just a timer to fire a
 * continuation at the other thread.
 */
static void
alarm ( int xx )
{
	thr_delay ( usual_delay );
	thr_unblock ( cont_slave );
}

static void
slave1 ( int xx )
{
	ready = 1;
	thr_block_c ( WAIT, slave2, 0 );

	panic ( "slave1, oops, shouldn't be here." );
	thr_unblock ( cont_thread );
}

static void
slave2 ( int yy )
{
	printf ( " OK, cool -- got a continuation\n" );
	thr_unblock ( cont_thread );
}

static void
test_contin1 ( int xx )
{
	printf ("Let's try a continuation.\n");
	ready = 0;
	cont_slave = safe_thr_new ( "slave", slave1, (void *)0, PRI_TEST, 0 );

	while ( ! ready )
	    thr_yield ();

	(void) safe_thr_new ( "alarm", alarm, (void *)0, PRI_TEST, 0 );

	printf ( "Waiting ..." );
	cont_thread = thr_self ();
	thr_block ( WAIT );
}
/* --------------------------------------------
 * Delay via continuation test.
 */

static volatile int c2_count;
static volatile int c2_done;

static void c2_s1 ( int );
static void c2_s2 ( int );

static void
c2_s1 ( int limit )
{
	/*
	thr_show ();
	show_my_regs();
	printf ( "\n" );
	*/

	++c2_count;
	printf ( " %d", c2_count );

	if ( c2_count < limit )
	    thr_delay_c ( usual_delay, c2_s1, limit );
	c2_done = 1;

	/*
	printf ( "\n" );
	show_my_regs();
	*/
}

/* First part of the test, test thr_delay_c() only.
 * Thread starts here to launch things.
 * Bounces control to the above.
 */
static void
c2_slave1 ( int limit )
{
	/*
	thr_show ();
	show_my_regs();
	printf ( "\n" );
	*/

	thr_delay_c ( usual_delay, c2_s1, limit );
}

/* -- */

static void
c2_s2 ( int limit )
{
	++c2_count;
	printf ( " %d", c2_count );

	if ( c2_count < limit )
	    thr_delay_q ( usual_delay );
	c2_done = 1;
}

/* Second part of the test, test thr_delay_c() and thr_delay_q().
 *  starts here -- bounces control to the above.
 */
static void
c2_slave2 ( int limit )
{
	thr_delay_c ( usual_delay, c2_s2, limit );
}

static void
test_contin2 ( int limit )
{

	c2_count = 0;
	c2_done = 0;

	/* test the _c call only */
	printf ("Ready ...");
	(void) safe_thr_new ( "slave1", c2_slave1, (void *)limit, PRI_TEST, 0 );

	while ( ! c2_done )
	    thr_yield ();

	printf (" ... OK\n");

	c2_count = 0;
	c2_done = 0;

	/* test the _c and _q calls */
	printf ("Again ...");
	(void) safe_thr_new ( "slave2", c2_slave2, (void *)limit, PRI_TEST, 0 );

	while ( ! c2_done )
	    thr_yield ();

	printf (" ... OK\n");
}

/* -------------------------------------------- */
/* Thread test 0 ...
 * Can we just start a thread at all?
 */
static volatile int t0;

static void
t0_func ( int arg )
{
	t0 = arg;
}

static void
test_thread0 ( int xx )
{
	int arg = 789;

	t0 = 0;

	printf ( "Thread starting test ...");
	(void) safe_thr_new ( "t0f", t0_func, (void *) arg, PRI_TEST, 0 );

	while ( t0 != arg )
	    thr_yield ();
	printf ( " OK!\n" );
}

/* -------------------------------------------- */
/* Thread test 1 ...
 * The idea here is that we have 2 threads
 * that each increment a shared variable and
 * then yield to the scheduler.  The behaviour
 * may change as we introduce new policy and
 * (in particular, priority handling) into the
 * scheduler.  However, during early testing,
 * this alternates between the two threads.
 */

struct thread *t1, *t2;
static volatile int t1_count;
static volatile int done1;

static void
t1_f1 ( int max )
{
	for ( ;; ) {
	    ++t1_count;
	    printf ( " f1(%d)", t1_count );
	    if ( t1_count > max )
		break;
	    thr_yield ();
	}
	++done1;
}

static void
t1_f2 ( int max )
{
	for ( ;; ) {
	    ++t1_count;
	    printf ( " f2(%d)", t1_count );
	    if ( t1_count > max )
		break;
	    thr_yield ();
	}
	++done1;
}

void
test_thread1 ( int count )
{
	done1 = 0;
	t1_count = 0;

	printf ( "Run multiple thread test.\n");
	t1 = safe_thr_new ( "t1f1", t1_f1, (void *) count, PRI_TEST, 0 );
	t2 = safe_thr_new ( "t1f2", t1_f2, (void *) count, PRI_TEST, 0 );

	while ( done1 < 2 )
	    thr_yield ();

	printf ( " OK!\n" );
}

/* -------------------------------------------- */
/* Thread test 2 ...
 * The idea here is that we have 2 threads
 * that share a reentrant function.
 * The function increments a shared variable,
 * then yields to the scheduler.  The behaviour
 * may change as noted above for test 1, but
 * during early testing the two threads alternate.
 */
static volatile int t2_count;
static volatile int done2;

void
t2_func ( int id )
{
	while ( t2_count++ < 5 ) {
	    printf ( " f%d(%d)", id, t2_count );
	    thr_yield ();
	}
	++done2;
}

static void
test_thread2 ( int xx )
{
	done2 = 0;
	t2_count = 0;

	printf ( "Starting reentrant thread test.\n");

	(void) safe_thr_new ( "func", t2_func, (void *) 1, PRI_TEST, 0 );
	(void) safe_thr_new ( "func", t2_func, (void *) 2, PRI_TEST, 0 );

	while ( done2 < 2 )
	    thr_yield ();

	printf ( " OK!\n" );
}

/* -------------------------------------------- */
/* Thread test 3 ...
 *
 * Now we have semaphores!
 *
 * Again, 2 threads share a reentrant function.
 * In a screwy arrangement, I use a pair of
 * semaphores so that when one function is done
 * it uses a semaphore to signal the other.
 *
 * Which function runs first is dependent on
 * scheduler behavior in a meaningless way.
 * The function increments a shared variable,
 * but once things get started, the alternation
 * is strictly deterministic.
 */

static volatile int t3_count;
static volatile int t3_done;
static volatile int t3_start;

struct sem *sem1;
struct sem *sem2;

static void
t3_func ( int id, struct sem *self, struct sem *peer )
{
	++t3_start;
	while ( t3_count <= 5 ) {
	    sem_block ( self );
	    ++t3_count;
	    printf ( " f%d-%d", id, t3_count );
	    sem_unblock ( peer );
	}

	t3_done++;
}

static void
t3_f1 ( int xx )
{
	t3_func ( 1, sem1, sem2 );
}

static void
t3_f2 ( int xx )
{
	t3_func ( 2, sem2, sem1 );
}

void
test_thread3 ( int xx )
{
	t3_done = 0;
	t3_count = 0;
	t3_start = 0;

	printf ( "Starting semaphore test\n");

	sem1 = safe_sem_new ( CLEAR );
	sem2 = safe_sem_new ( CLEAR );

	(void) safe_thr_new ( "t3f1", t3_f1, 0, PRI_TEST, 0 );
	(void) safe_thr_new ( "t3f2", t3_f2, 0, PRI_TEST, 0 );

	while ( t3_start < 2 )
	    thr_yield ();

	sem_unblock ( sem1 );

	while ( t3_done < 2 )
	    thr_yield ();

	sem_destroy ( sem1 );
	sem_destroy ( sem2 );

	printf (" OK!\n");
}

/* -------------------------------------------- */

/* Thread test 4 ...
 *
 * This test seems pretty silly now,
 * but at one time it tested the amazing ability
 * to pass an argument to a thread function, and
 * also to return from a thread function
 * into thr_exit()
 */

static int t4_count = 0;

static void
t4_func ( int count )
{
	/*
	int x = getsp ();
	printf ( "func%d starting\n", count );
	dump_l ( (void *) x, 4 );
	printf ( "sp = %08x\n", x );
	printf ( "thr_exit = %08x\n", thr_exit );
	thr_show_one_all ( cur_thread, "t4_func" );
	*/

	for ( ;; ) {
	    if ( ++t4_count > count )
		break;
	    printf ( " %d/%d", t4_count, count );
	    thr_yield ();
	}

	t4_count = 999;
	/* should return to thr_exit()
	 */
}

void
test_thread4 ( int count )
{
	printf ( "Running silly thread test.\n" );

	t4_count = 0;
	(void) safe_thr_new ( "t4f", t4_func, (void *) count, PRI_TEST, 0 );

	while ( t4_count < 100 )
	    thr_yield ();
	printf ( " OK!\n" );
}

/* -------------------------------------------- */

/* thread test 5 */
/* This test is primarily designed to exercise the
 * new ability to have multiple threads blocked on
 * a single semaphore.  They start in no particular
 * order, but thereafter, the semaphore queue gives
 * nice sharing.
 */

static struct sem *t5_sem;
static volatile int t5_run;
static volatile int t5_tick;

void
t5_ticker ( void )
{
	static int count = 0;

	if ( (++count % usual_delay) == 0 ) {
	    ++t5_tick;
	}
}

void
t5_slave ( int id )
{
	for ( ;; ) {
	    sem_block ( t5_sem );
	    if ( ! t5_run )
	    	break;
	    printf ( " s%d(%d)", id, t5_tick );
	}
}

void
tdelay ( void )
{
	int ltick = t5_tick;

	while ( t5_tick == ltick )
	    thr_yield ();
}

void
t5_master ( int count )
{
	int i;

	tdelay ();

	for ( i=0; i<count; i++ ) {
	    sem_unblock ( t5_sem );
	    tdelay ();
	}

	/* cleanup */
	t5_run = 0;
	sem_unblock ( t5_sem );
	sem_unblock ( t5_sem );
	sem_unblock ( t5_sem );
}

void
test_thread5 ( int count )
{
	printf ( "Running multi semaphore test.\n" );

	t5_run = 1;
	t5_tick = 0;
	timer_hookup ( t5_ticker );

	t5_sem = safe_sem_new ( CLEAR );

	(void) safe_thr_new ( "slave", t5_slave, (void *) 1, PRI_TEST, 0 );
	(void) safe_thr_new ( "slave", t5_slave, (void *) 2, PRI_TEST, 0 );
	(void) safe_thr_new ( "slave", t5_slave, (void *) 3, PRI_TEST, 0 );

	(void) safe_thr_new ( "mastr", t5_master, (void *) count, PRI_TEST, 0 );

	while ( t5_run )
	    thr_yield ();

	timer_hookup ( 0 );
	sem_destroy ( t5_sem );

	printf ( " Done.\n" );
}

/* -------------------------------------------- */

/* This test is a refinement of test 5.
 * It still exercises the ability to have multiple
 * threads blocked on a single semaphore.
 * The interesting new thing is that it relies on
 * the ability to unblock a semaphore from an
 * interrupt routine.
 * It is much more elegant in doing away with the
 * master thread and busy waiting on the timer.
 */

static struct sem *t6_sem;
static volatile int t6_tick;
static volatile int t6_done;
static volatile int t6_max;

void
t6_ticker ( void )
{
	static int count = 0;

	if ( (++count % usual_delay) == 0 ) {
	    ++t6_tick;
	    sem_unblock ( t6_sem );
	}
}

void
t6_slave ( int id )
{
	for ( ;; ) {
	    sem_block ( t6_sem );
	    if ( t6_tick > t6_max )
	    	break;
	    printf ( " s%d(%d)", id, t6_tick );
	}
	++t6_done;
}

void
test_fancy ( int count )
{
	printf ( "Running fancy multi semaphore test.\n" );

	t6_done = 0;
	t6_tick = 0;
	t6_max = count;

	timer_hookup ( t6_ticker );

	t6_sem = safe_sem_new ( CLEAR );

	(void) safe_thr_new ( "slave", t6_slave, (void *) 1, PRI_TEST, 0 );
	(void) safe_thr_new ( "slave", t6_slave, (void *) 2, PRI_TEST, 0 );
	(void) safe_thr_new ( "slave", t6_slave, (void *) 3, PRI_TEST, 0 );

	while ( t6_done < 3 )
	    thr_yield ();

	timer_hookup ( 0 );

	sem_destroy ( t6_sem );

	printf ( " Done.\n" );
}

/* -------------------------------------------- */
/* test 7 */

/* This test is a hard one, thus extremely valuable
 *
 * One thread is in a tight computation loop,
 * and the other one expects to receive activations
 * via semaphores from an interrupt.
 *
 * This was tricky on the x86 and revealed some issues
 * on the ARM port as well.  Here is how things go:
 *
 * 1) we start with just the test thread as always,
 *	running at priority 30.
 * 2) the "slim" thread is launched, it runs at
 *	priority 10 until it blocks on a semaphore.
 * 3) back to the test thread which launches
 *	the "busy" thread at priority 11
 * 4) the busy thread goes into a loop, perhaps
 *	calling thr_yield() in the easy case.
 * 5) a timer interrupt happens and the ticker
 *	routine is called on a timer hook at
 *	interrupt level, it does an unblock on
 *	the semaphore, which marks the slim thread
 *	as ready to run.
 * This is where things get interesting.  We are
 *	in the timer interrupt, but we don't want
 *	to resume the "busy" thread (which is the
 *	current thread.  We want to switch to the
 *	slim thread.
 * The problem on the ARM was that the resume mode
 *	for the slim thread was JUMP, and this was
 *	not ready to be done from interrupt level.
 *
 * This test continued to expose tricky problems in
 *   the ARM context switching code.  The last was
 *   corrupting the condition codes in the PSR
 *   during interrupts.  5-19-2015
 */

static struct sem *t7_sem;
static volatile int t7_tick;
static volatile int t7_run;
static volatile long t7_sum;

static void
t7_ticker ( void )
{
	static int count = 0;

	if ( (++count % usual_delay) == 0 ) {
	    ++t7_tick;
	    sem_unblock ( t7_sem );
	}
}

static void
slim79 ( int count )
{
	/*
	printf ( "slim starting\n" );
	*/
	for ( ;; ) {
	    sem_block ( t7_sem );
	    if ( is_irq_disabled() )
		printf ("slim -- no IRQ\n" );
	    if ( t7_tick > count )
	    	break;
	    printf ( " %d(%d)", t7_tick, t7_sum );
	    t7_sum = 0;
	}
	t7_run = 0;
}

static void
busy79 ( int nice )
{
	int psr;

	/*
	printf ( "busy starting\n" );
	*/
	while ( t7_run ) {
	    if ( nice )
		thr_yield ();
	    ++t7_sum;
	    /* if ( is_irq_disabled() ) */
	    psr = get_cpsr();
	    if ( psr & 0x80 )
		printf ("busy -- no IRQ (%08x)\n", psr );
	}
}

static void
test_79 ( int count, int nice )
{
	t7_run = 1;
	t7_tick = 0;
	t7_sem = safe_sem_new ( CLEAR );

	timer_hookup ( t7_ticker );

	/* the busy loop must be at a lower priority than
	 * the "slim" loop that expects the activations.
	 * Also, if we start busy first, it runs right away,
	 * and the slim thread never even starts, so the
	 * semaphores coming from the interrupt routine
	 * are useless.
	 */
	(void) safe_thr_new ( "slim", slim79, (void *) count, PRI_TEST, 0 );
	(void) safe_thr_new ( "busy", busy79, (void *) nice, PRI_TEST+1, 0 );

	while ( t7_run ) {
	    if ( is_irq_disabled() )
		printf ("test -- no IRQ\n" );
	    thr_yield ();
	}

	timer_hookup ( 0 );
	sem_destroy ( t7_sem );
}

static void
test_easy ( int count )
{
	printf ( "Easy interrupt activation test.\n" );

	test_79 ( count, 1 );

	printf ( " OK\n" );
}

static void
test_hard ( int count )
{
	printf ( "Hard interrupt activation test.\n" );

	test_79 ( count, 0 );

	printf ( " OK\n" );
}

/* -------------------------------------------- */
/* Check out condition variables.
 */

static volatile int predicate;
static struct cv *cv1;
static struct sem *cv1_mutex;

static void
cv1_func ( int x )
{
	int m;

	sem_block ( cv1_mutex );
	while ( ! predicate ) {
	    printf ( "Wait " );
	    cv_wait ( cv1 );
	    printf ("Out ");
	    /* This should fail, we should already have
	     * the lock, if this works, we acquire it
	     * below, which is wrong.
	     */
	    m = sem_block_try ( cv1_mutex );
	    if ( m )
	    	printf ( "Oops " );
	}
	sem_unblock ( cv1_mutex );
	printf ( "Bye! " );
}

static void
test_cv1 ( int count )
{
	struct thread *tp;

	printf ( "CV test: " );

	predicate = 0;
	cv1_mutex = sem_mutex_new ( SEM_PRIO );

	cv1 = cv_new ( cv1_mutex );
	if ( ! cv1 )
	    printf ( "No dice!\n" );

	tp = safe_thr_new ( "cvf", cv1_func, (void *) 0, 10, 0 );
	thr_delay ( 5 );
	predicate = 1;
	cv_signal ( cv1 );
	thr_delay ( 5 );

	thr_join ( tp );
	cv_destroy ( cv1 );
	sem_destroy ( cv1_mutex );

	printf ( " OK\n" );
}

/* -------------------------------------------- */
/* Check out join.
 */

#define JDELAY 10

static void
join_func ( int time )
{
	if ( time )
	    thr_delay ( time );
	printf ( "exit " );
}

static void
test_join ( int count )
{
	struct thread *new;

	printf ( "Join test: " );

	printf ("(" );
	new =  safe_thr_new ( "jf1", join_func, (void *) JDELAY, 10, 0 );
	printf ( "join " );
	thr_join ( new );
	printf ( ") ");

	printf ("(" );
	new =  safe_thr_new ( "jf2", join_func, (void *) 0, 10, 0 );
	thr_delay ( JDELAY );
	printf ( "join " );
	thr_join ( new );

	printf ( ") OK\n");
}
/* -------------------------------------------- */
/* Check out a mutex semaphore.
 */

static struct sem *mutex;

static void
mutex_func ( int time )
{
	printf ( "Thread " );
	sem_block ( mutex );
	if ( sem_block_try ( mutex ) )
	    printf ( "BAD " );
	else
	    printf ( "IN " );
	sem_unblock ( mutex );
	printf ( "Exit " );
}

static void
test_mutex ( int count )
{
	struct thread *new;

	printf ( "Mutex test: " );

	mutex = sem_mutex_new ( SEM_FIFO );
	if ( ! mutex ) {
	    printf ( "Cannot create mutex semaphore\n" );
	    panic ( "mutex sem new" );
	}

	printf ("Start " );
	sem_block ( mutex );
	printf ("Locked " );

	new =  safe_thr_new ( "tmutex", mutex_func, (void *) 0, 10, 0 );

	thr_delay ( 10 );

	printf ( "Unlock " );
	sem_unblock ( mutex );

	thr_join ( new );
	printf ( "Join OK\n");

	sem_destroy ( mutex );
}
/* -------------------------------------------- */

/* Generate a data fault */
static void
test_fault ( int xxx )
{
	volatile int junk;
	char *p = (char *) 0;

	junk = *p;
}

/* -------------------------------------------- */

/* Test gpio on BBB */
static void
test_gpio ( int count )
{
	gpio_test3 ();
#ifdef notdef
	gpio_test_init ();

	while ( test_running ) {
	    thr_delay ( 4 );
	    gpio_test_run ();
	}
#endif
}

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

/* -------------------------------------------- */
/* Random numbers.
 */

#define NB 20

static void
test_ran ( int count )
{
	int i, n, x;
	char buf[MAXB];
	int bins[NB];

	gb_init_rand ( 0x163389 );

	for ( i=0; i<20; i++ ) {
	    printf ( "%d\n", gb_next_rand() );
	}

	printf ( "More ... " );
	getline ( buf, MAXB );

	for ( i=0; i<NB; i++ )
	    bins[i] = 0;
	x = 0;

	for ( i=0; i<200000; i++ ) {
	    n = gb_unif_rand(NB);
	    if ( n < 0 || n >= NB )
	    	x++;
	    else
	    	bins[n]++;
	}

	for ( i=0; i<NB; i++ ) {
	    printf ( "%d: %d\n", i, bins[i] );
	}
	printf ( "%d: %d\n", 99, x++ );
}

/* -------------------------------------------- */

static void f_croak ( int junk )
{
	printf ( "Gone!\n");
}

static void f_linger ( int time )
{
	/*
	thr_delay ( time * HZ );
	*/
	thr_delay_c ( time * HZ, f_croak, 0 );
	printf ( "Exit!\n");
}

/* This test just verifies that threads get inserted into
 * the ready list in numerical order.
 * At first we just left the threads blocked and there
 * was no way to get rid of them.  Then I got the idea
 * of using thr_delay to have them go away after a while.
 *
 * This exposed a bug (the keyboard no longer worked after
 * the timeout, well at least not from the prompt, the
 * CAPS lock would still do a reboot.).  This is only the
 * case when the CV option is in use for the keyboard, and
 * when the current thread is not the one delayed.
 * This bug was fixed 8/22/2002 (it was in thr_unblock)
 */
static void
test_sort ( int count )
{
	/*
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 13, TF_BLOCK );
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 18, TF_BLOCK );
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 15, TF_BLOCK );
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 11, TF_BLOCK );
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 17, TF_BLOCK );
	(void) safe_thr_new ( 0, f_ez, (void *) 0, 22, TF_BLOCK );
	*/

	(void) safe_thr_new ( 0, f_linger, (void *) 9, 13, 0 );
	(void) safe_thr_new ( 0, f_linger, (void *) 9, 18, 0 );
	(void) safe_thr_new ( 0, f_linger, (void *) 9, 15, 0 );
	(void) safe_thr_new ( 0, f_linger, (void *) 9, 11, 0 );
	(void) safe_thr_new ( 0, f_linger, (void *) 9, 17, 0 );
	(void) safe_thr_new ( 0, f_linger, (void *) 9, 22, 0 );
}

/* -------------------------------------------- */

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
	    thr_delay ( 5 * HZ );
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

/* -------------------------------------------- */

#ifdef WANT_SETJMP
/* Test a single setjmp/longjmp
 * first panic is after the setjmp,
 * then you must continue to get the
 * longjmp, if you continue from there
 * you drop into the scheduler idle loop.
 */
#include "setjmp.h"

static void
jmp_test1 ( void )
{
	static jmp_buf jbuf;

	if ( setjmp ( jbuf ) ) {
	    printf ( "OK\n" );
	    return;
	}

	/*
	dump_l ( (void *) jbuf, 4 );
	*/
	printf ( "Testing setjmp ... " );

	longjmp ( jbuf );
	/* NOTREACHED */

	panic ( "longjmp test" );
}

static void
jmp_test2 ( void )
{
	static struct jmp_regs jbuf;

	if ( save_j ( &jbuf ) ) {
	    printf ( "OK\n" );
	    return;
	}

	/*
	dump_l ( (void *) &jbuf, 4 );
	*/
	printf ( "Testing save_j ... " );

	resume_j ( &jbuf );
	/* NOTREACHED */

	panic ( "resume_j test" );
}

static void
test_setjmp ( int xx )
{
	printf ( "Running setjmp test\n" );
	jmp_test1 ();
	jmp_test2 ();
}
#endif

/* -------------------------------------------- */

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

/* --------------- network tests ------------------ */

#ifdef WANT_NET
static void
test_net ( int test )
{
	/*
	*/
}

static void
test_netshow ( int test )
{
	net_show ();
}

extern unsigned long test_ip;

static void
test_netarp ( int test )
{
	show_arp_ping ( test_ip );
}

static void
test_netping ( int test )
{
	ping ( test_ip );
}

/* Should broadcast BOOTP packet */
static void
test_bootp ( int test )
{
	bootp_test();
}

static void
test_dns ( int test )
{
	dns_test();
}

static void
test_arp ( int count )
{
	int i;

	for ( i=0; i < count; i++ ) {
	    thr_delay ( 10 );
	    arp_announce ();
	}
}

#endif	/* WANT_NET */

/* THE END */
