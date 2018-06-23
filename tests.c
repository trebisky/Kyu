/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * tests.c
 * Tom Trebisky
 *
 * began as user.c -- 11/17/2001
 * made into tests.c  9/15/2002
 * added to kyu 5/11/2015
 * split into test*.c  --  6/23/2018
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"
#include "arch/cpu.h"

#include "netbuf.h"

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

extern struct thread *cur_thread;

static int timer_rate;	/* ticks per second */

#define PRI_DEBUG	5
#define PRI_TEST	10
#define PRI_WRAP	20

#define USER_DELAY	10
#define AUTO_DELAY	1

static int usual_delay = USER_DELAY;

static void check_clock ( void );

void pkt_arm ( void );
void pkt_arrive ( void );

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
static void test_cancel ( int );

struct test {
	tfptr	func;
	char	*desc;
	int	arg;
};

/* These are the tests we run in the automatic regression set
 * Don't put anything ugly in here we cannot run in a loop.
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
	test_cancel,	"Timer cancel test",	0,
	0,		0,			0
};

/* These go in the "i" menu which is kind of a grab bag
 * of mostly architecture dependent tests.
 *
 * Don't run these tests automatically.
 * Don't run them in a repeat loop either.
 */
static void test_sort ( int );
static void test_ran ( int );
static void test_malloc ( int );
static void test_wait ( int );
static void test_unroll ( int );
static void test_fault ( int );
static void test_zdiv ( int );
static void test_gpio ( int );
static void test_clock ( int );

#ifdef BOARD_BBB
static void test_adc ( int );
static void test_fault2 ( int );
#endif

#ifdef BOARD_ORANGE_PI
static void test_wdog ( int );
static void test_cores ( int );
static void test_thermal ( int );
static void test_uart ( int );
#endif

static void test_clear ( int );
static void test_blink ( int );
static void test_blink_d ( int );

/* Here is the IO test menu */

struct test io_test_list[] = {
	test_sort,	"Thread sort test",	5,
	test_ran,	"Random test",		0,
	test_malloc,	"malloc test",		0,
	test_wait,	"wait for 5 seconds",	0,
	test_unroll,	"stack traceback",	0,
	test_fault,	"Fault test",		0,
	test_zdiv,	"Zero divide test",	0,
	test_gpio,	"GPIO test",		0,
	test_clock,	"CPU clock test",	0,

#ifdef BOARD_BBB
	test_adc,	"BBB adc test",		0,
	test_fault2,	"data abort probe",	0,
#endif

#ifdef BOARD_ORANGE_PI
	test_wdog,	"Watchdog test",	0,
	test_cores,	"Opi cores test",	0,
	test_thermal,	"H3 thermal test",	0,
	test_uart,	"uart test",		0,
#endif

	test_blink,	"start LED blink test",	0,
	test_blink,	"stop LED blink test",	1,
	test_blink_d,	"LED blink test (via delay)",	0,

	test_clear,	"clear memory test",	0,

	0,		0,			0
};


#ifdef WANT_NET
static void test_netshow ( int );
static void test_netarp ( int );
static void test_bootp ( int );
static void test_dhcp ( int );
static void test_icmp ( int );
static void test_dns ( int );
static void test_arp ( int );
void test_tftp ( int );
static void test_udp ( int );
static void test_netdebug ( int );
static void test_udp_echo ( int );
static void test_tcp_echo ( int );

void
test_tcp ( int xxx )
{
#ifdef WANT_TCP_XINU
	printf ( "Testing Xinu TCP\n" );
	test_xinu_tcp ();
#else
	printf ( "Testing Kyu TCP\n" );
	test_kyu_tcp ();
#endif
}


struct test net_test_list[] = {
	test_netshow,	"Net show",		0,
	test_netarp,	"ARP ping",		0,
	test_bootp,	"test BOOTP",		0,
	test_dhcp,	"test DHCP",		0,
	test_icmp,	"Test ICMP",		0,
	test_dns,	"Test DNS",		0,
	test_arp,	"one gratu arp",	1,
	test_arp,	"8 gratu arp",		8,
	test_tftp,	"Test TFTP",		0,
	test_udp,	"Test UDP",		0,
	test_tcp,	"Test TCP",		1,
	test_udp_echo,	"Endless UDP echo",	0,
	test_tcp_echo,	"Endless TCP echo",	0,
	test_netdebug,	"Debug interface",	0,
	0,		0,			0
};
#endif

struct test *cur_test_list = std_test_list;

static int test_debug = 0;

/* Wrapper function to catch troubles when making new semaphores.
 */
static struct sem *
safe_sem_signal_new ( void )
{
	struct sem *rv;

	rv = sem_signal_new ( SEM_FIFO );
	if ( ! rv ) {
	    printf ( "Cannot create new semaphore\n" );
	    panic ( "user sem new" );
	}

	return rv;
}


int
is_irq_disabled ( void )
{
	unsigned int reg;

	get_CPSR ( reg );
	if ( reg & 0x80 )
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
	info.sem = safe_sem_signal_new ();
	info.times = 1;	/* ignored */

	(void) safe_thr_new ( 
	    "bwrapper", basic_wrapper, (void *) &info, PRI_WRAP, 0 );

	sem_block ( info.sem );
	sem_destroy ( info.sem );

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

	// printf ( "&ip = %08x, ip = %08x, ip->times = %d\n", &ip, ip, ip->times );

	for ( i=0; i< ip->times; i++ ) {
	    // printf ( "RUNNING test (%d of %d)\n", i+1, ip->times);
	    // printf ( "Stack at %08x\n", get_sp() );

	    if ( ! test_running ) {
		break;
	    }

	    (*ip->func) ( ip->arg );

	    // printf ( "FINISH test (%d of %d)\n", i+1, ip->times);
	}

	// printf ( "&ip = %08x, ip = %08x, ip->times = %d\n", &ip, ip, ip->times );

	/*
	sem_unblock ( ip->sem );
	*/
}

static struct test_info info;

static void
single_test ( struct test *tstp, int times )
{
	/* Having this on the stack, calling a thread,
	 * then returning from this function is very bad,
	 * if the thread we pass it to expects this data
	 * to be stable.
	 */
	// struct test_info info;

	info.func = tstp->func;
	info.arg = tstp->arg;
	/*
	info.sem = safe_sem_signal_new ();
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

#ifdef notdef
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
#endif

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

	timer_rate = timer_rate_get ();

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
		printf ( "select io test menu (q to quit)\n" );
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

	    /* Dubious usefulness these days */
	    if ( **wp == 'e' ) {
		printf ( "Test told to stop\n" );
		test_running = 0;
	    }

	    if ( **wp == 'y' ) {
		kyu_debugger ();
	    }

#ifdef ARCH_ARM
#ifdef notdef
/* Does not work, just use reboot */
	    if ( **wp == 'r' ) {
	    	/* restart the software */
		printf ( "Restarting\n" );
	    	kyu_startup ();
	    }
#endif

	    if ( **wp == 'R' ) {
	    	/* Reboot the machine */
		printf ( "Rebooting\n" );
	    	reset_cpu ();
	    }

	    if ( **wp == 's' ) {
		show_my_regs ();
	    }

#ifdef notdef
	    if ( **wp == 'x' ) {
		x_test ();
	    }
#endif

#endif

	    if ( **wp == 'x' ) {
		shell_x ( &wp[1], nw-1 );
	    }

/* On the Orange Pi there is SRAM at 0-0xffff.
 * also supposed to be at 0x44000 to 0x4Bfff (I see this)
 * also supposed to be at 0x10000 to 0x1afff, but not for me.
 */

	    if ( **wp == 'l' ) {
		printf ( "Memory test\n" );
		mem_verify ( 0x0, 0x10000 );
	    }

	    if ( **wp == 'm' ) {
		/* Orange Pi SRAM A1 */
		printf ( "Memory test 0 to 0x10000\n" );
		mem_test ( 0x0, 0x10000 );
		/* Orange Pi SRAM A2 */
		printf ( "Memory test 0x44000 to 0x4C000\n" );
		mem_test ( 0x44000, 0x4c000 );
		/* Orange Pi SRAM C */
		printf ( "Memory test 0x10000 to 0x20000\n" );
		mem_test ( 0x10000, 0x20000 );
	    }

	    /* Dump without ram safety check */
	    if ( wp[0][0] == 'd' && wp[0][1] == 'i' && nw == 3 ) {
		mem_dumper ( 'i', wp[1], wp[2] );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'b' && nw == 3 ) {
		mem_dumper ( 'b', wp[1], wp[2] );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'w' && nw == 3 ) {
		mem_dumper ( 'w', wp[1], wp[2] );
		continue;
	    }

	    if ( **wp == 'd' && nw == 3 ) {
		mem_dumper ( 'l', wp[1], wp[2] );
		continue;
	    }

#ifdef notdef
	    if ( **wp == 'w' && nw == 2 ) {
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

	    if ( **wp == 'o' ) {
		if ( nw > 1 )
		    thr_debug ( atoi (wp[1]) );
		else
		    thr_debug ( 0 );
	    }

	    /* Show a specific thread
	     */
	    if ( **wp == 'u' && nw == 2 ) {
	    	thr_show_name ( wp[1] );
	    }


	    /* Show threads */
	    /* Conflicts with t for test mode below */
	    if ( **wp == 't' && nw == 1 ) {
	    	thr_show ();
	    }

	    /* Run a test or tests */
	    if ( **wp == 't' && nw > 1 ) {
		n = atoi ( wp[1] );

		nl = 1;
		if ( nw == 3 )
		    nl = atoi ( wp[2] );
		if ( nl < 1 )
		    nl = 1;

		if ( n == 0 && cur_test_list == std_test_list ) {
		    all_tests ( nl );
		    continue;
		}

		/* No looping except for standard tests */
		if ( cur_test_list != std_test_list )
		    nl = 1;

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
	for ( ;; ) ;
	*/

	tester ();
}

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
	    for ( ;; )
		;
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

// static struct thread *cont_thread;
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

	//thr_block ( WAIT );
	//printf ( "slave1, oops, shouldn't be here." );

#ifdef notdef
	/* This is hardly race free */
	while ( ready )
	    thr_yield ();
	thr_unblock ( cont_thread );
#endif
}

static void
slave2 ( int yy )
{
	printf ( " OK, cool -- got a continuation." );

#ifdef notdef
	/* This is hardly race free */
	while ( ready )
	    thr_yield ();
	thr_unblock ( cont_thread );
#endif
}

/* Runs at PRI 20 in wrapper context.
 * The other boys run at PRI 10.
 * usual delay is 10 on single test, 1 when looping.
 */
static void
test_contin1 ( int xx )
{
	// cont_thread = thr_self ();

	printf ("Let's try a continuation.\n");
	ready = 0;
	cont_slave = safe_thr_new ( "slave", slave1, (void *)0, PRI_TEST, 0 );

	while ( ! ready )
	    thr_yield ();

	(void) safe_thr_new ( "alarm", alarm, (void *)0, PRI_TEST, 0 );

#ifdef notdef
	/* Sorta like a join, but with nasty races. */
	// printf ( "Waiting ..." );
	ready = 0;
	thr_block ( WAIT );
#endif

	thr_join ( cont_slave );

	printf ( " Done\n" );
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

	sem1 = safe_sem_signal_new ();
	sem2 = safe_sem_signal_new ();

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

	t5_sem = safe_sem_signal_new ();

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

	t6_sem = safe_sem_signal_new ();

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
	unsigned int psr;

	/*
	printf ( "busy starting\n" );
	*/
	while ( t7_run ) {
	    if ( nice )
		thr_yield ();
	    ++t7_sum;
	    /* if ( is_irq_disabled() ) */
	    // psr = get_cpsr();
	    get_CPSR ( psr );
	    if ( psr & 0x80 )
		printf ("busy -- no IRQ (%08x)\n", psr );
	}
}

static void
test_79 ( int count, int nice )
{
	t7_run = 1;
	t7_tick = 0;
	t7_sem = safe_sem_signal_new ();

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

static struct thread *waiter;
static struct sem *waiter_sem;

static void
cancel_func ( int type )
{
	thr_delay ( 50 );

	if ( type == 0 ) {
	    thr_unblock ( waiter );
	} else {
	    sem_unblock ( waiter_sem );
	}
}

/* Test timer cancels and sem with timeout */
static void
test_cancel ( int count )
{
	struct thread *new;
	int start_time;

	waiter = thr_self ();
	waiter_sem = safe_sem_signal_new ();

	/* First test delay */
	printf ("Delay ");
	start_time = get_timer_count_t ();
	new =  safe_thr_new ( "tcan", cancel_func, (void *) 0, 10, 0 );

	thr_delay ( 100 );
	// printf ( "Boom\n" );

	if ( get_timer_count_t () - start_time < 60 )
	    printf ( "OK" );
	else
	    printf ( "Fail" );

	/* Then test semaphore */
	printf (" Sem ");
	start_time = get_timer_count_t ();
	new =  safe_thr_new ( "tcan", cancel_func, (void *) 1, 10, 0 );

	sem_block_t ( waiter_sem, 25 );

	if ( get_timer_count_t () - start_time < 30 )
	    printf ( "OK" );
	else
	    printf ( "Fail" );

	printf ("\n");

	sem_destroy ( waiter_sem );
}

/* -------------------------------------------- */

static void
test_malloc ( int xxx )
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

/* Wait for 5 seconds */
/* This runs in its own thread,
 * which can be interesting.
 */
static void
test_wait ( int xxx )
{
	thr_delay ( 5 * timer_rate );
	printf ( "Done waiting\n" );
}

/* -------------------------------------------- */

/* test ARM stack backtrace */
static void
test_unroll ( int xxx )
{
	unroll_cur();
}

/* Generate a data abort on the BBB
 * On the BBB this causes a fault, but
 *  is perfectly fine on the Orange Pi.
 */
static void
fault_addr_zero ( void )
{
	volatile int data;
	char *p = (char *) 0;

	printf ( "Read byte from address zero\n" );
	data = *p;
	printf ( " Got: %02x\n", data );

	printf ( "Write byte to address zero\n" );
	*p = 0xAB;

	printf ( "Read it back\n" );
	data = *p;
	printf ( " Got: %02x\n", data );
}

static int fault_buf[2];

static void
fault_align ( void )
{
	int *p;
	short *sp;
	int data;

	fault_buf[0] = 0x12345678;
	fault_buf[1] = 0x9abcdef0;

	/* This should work */
	printf ( "Read nicely aligned 4 bytes\n" );
	p = fault_buf;

	printf ( "4 byte read from address: %08x\n", p );
	data = *p;
	printf ( "Fetched %08x\n", data );

	/* This should fail */
	printf ( "Read short aligned 4 bytes\n" );
	p = (int *) ((short *)p + 1);

	printf ( "4 byte read from address: %08x\n", p );
	data = *p;
	printf ( "Fetched %08x\n", data );

	/* This should fail also */
	printf ( "Read byte aligned 4 bytes\n" );
	p = (int *) ((char *)p + 1);

	printf ( "4 byte read from address: %08x\n", p );
	data = *p;
	printf ( "Fetched %08x\n", data );

	/* AND - what about 2 byte reads */

	/* This should work */
	printf ( "Read nicely aligned 2 bytes\n" );
	sp = (short *) fault_buf;

	printf ( "2 byte read from address: %08x\n", sp );
	data = *sp;
	printf ( "Fetched %08x\n", data );

	/* And so should this */
	sp++;

	printf ( "2 byte read from address: %08x\n", sp );
	data = *sp;
	printf ( "Fetched %08x\n", data );

	/* But probably not this */
	sp = (short *) fault_buf;
	sp = (short *) ((char *)sp + 1);

	printf ( "2 byte read from address: %08x\n", sp );
	data = *sp;
	printf ( "Fetched %08x\n", data );

}

void
test_fault ( int xxx )
{
	fault_addr_zero ();
	fault_align ();
}

static void
test_zdiv ( int xxx )
{
	volatile int a = 1;
	int b = 0;

	printf ("Lets try a divide by zero ...\n");
	a = a / b;
	printf ("... All done!\n");
}

#ifdef BOARD_BBB
/* Use data abort to poke at some fishy addresses on the BBB */

static void
prober ( unsigned int addr )
{
	int s;

	s = data_abort_probe ( addr );
	printf ( "Probe address %08x ", addr );
	if ( s )
	    printf ( "Fails\n" );
	else
	    printf ( "ok\n" );
}

void
test_fault2 ( int xxx )
{
	unsigned long s;
	char *p;

	prober ( 0x44e30000 );	/* CM */
	prober ( 0x44e35000 );	/* WDT1 */
	prober ( 0x44e31000 );	/* Timer 1 (ms) */
	prober ( 0x4a334000 );	/* PRU 0 iram */
	prober ( 0x4a338000 );	/* PRU 1 iram */
#define I2C0_BASE      0x44E0B000
	prober ( I2C0_BASE );
#define I2C1_BASE      0x4802A000
	prober ( I2C1_BASE );
#define I2C2_BASE      0x4819C000       /* Gets data abort */
	prober ( I2C2_BASE );

	p = (char *) &s;

	prober ( (unsigned int) p );
	prober ( (unsigned int) (p + 1) );
	prober ( (unsigned int) (p + 2) );
}
#endif

#ifdef BOARD_ORANGE_PI
void
test_cores ( int xxx )
{
	/* official test */
	test_core ();

	/* the crazy business */
	// check_core ();
}

void
test_thermal ( int xxx )
{
	// test_ths ();
}
#endif


/* -------------------------------------------- */

#ifdef BOARD_BBB
/* BBB blink test - also a good test of
 * mixing repeats and delays
 */
#define BLINK_RATE	1000

static void
led_blinker ( int xx )
{
	/* Writing a "1" does turn the LED on */
	gpio_led_set ( 1 );
	thr_delay ( 100 );
	gpio_led_set ( 0 );
}

static void
led_norm ( void )
{
	gpio_led_set ( 0 );
}

#endif

#ifdef BOARD_ORANGE_PI
int led_state = 0;
#define BLINK_RATE	500

static void
led_blinker ( int xx )
{
	printf ( "Blink: %d\n", led_state );
	if ( led_state ) {
	    pwr_on ();
	    status_off ();
	    led_state = 0;
	} else {
	    pwr_off ();
	    status_on ();
	    led_state = 1;
	}
}

static void
led_norm ( void )
{
	pwr_on ();
	status_off ();
}

#endif

#ifdef BOARD_ORANGE_PI
#define X_UART	3

static void
test_uart ( int arg )
{
	uart_init ( X_UART, 115200 );

	for ( ;; ) {
	    printf ( "." );
	    uart_putc ( X_UART, 'a' );
	    uart_putc ( X_UART, 'b' );
	    uart_putc ( X_UART, 'c' );
	    thr_delay ( 100 );
	}
}
#endif

static void
test_clear ( int arg )
{
	unsigned long *start;
	unsigned long size;
	unsigned long *p;
	int i;

	start = (unsigned long *) ram_next ();
	size = ram_size ();

	printf ( "Clearing ram, %d bytes from %08x\n", size, start );
	size /= sizeof(unsigned long);

	p = start;
	for ( i=0; i<size; i++ )
	    *p++ = 0;

	p = start;
	for ( i=0; i<size; i++ )
	    *p++ = 0xdeadbeef;

	printf ( "Done clearning ram\n" );
}

static struct thread *blink_tp;

static void
test_blink ( int arg )
{
	if ( arg == 0 ) {
	    printf ( "Start the blink\n" );
	    blink_tp = thr_new_repeat ( "blinker", led_blinker, 0, 10, 0, BLINK_RATE );
	} else {
	    printf ( "Stop the blink\n" );
	    thr_repeat_stop ( blink_tp );
	}
	led_norm ();
}

#ifdef BOARD_ORANGE_PI
static void
test_led_on ( void )
{
	status_on ();
}

static void
test_led_off ( void )
{
	status_off ();
}
#endif

#ifdef BOARD_BBB
static void
test_led_on ( void )
{
	gpio_led_set ( 1 );
}

static void
test_led_off ( void )
{
	gpio_led_set ( 0 );
}
#endif

/* Useful for seeing if D cache is enabled or not.
 * Should blink two quick pulses, 1 second apart.
 */
static void
test_blink_d ( int arg )
{
	int a = 100;
	int b = 1000;

        b -= 3*a;

        for ( ;; ) {
            test_led_on ();
            delay_ms ( a );

            test_led_off ();
            delay_ms ( a );

            test_led_on ();
            delay_ms ( a );

            test_led_off ();

            delay_ms ( b );
        }
}

/* -------------------------------------------- */

static void test_clock ( int count ) {
	check_clock ();
}

/* Test gpio on BBB or Orange Pi */
static void test_gpio ( int count ) { gpio_test (); }

#ifdef BOARD_ORANGE_PI
/* Test watchdog on Orange Pi */
static void test_wdog ( int count ) { wd_test (); }
#endif

#ifdef BOARD_BBB

/* Test adc on BBB */
static void test_adc ( int count ) { adc_test (); }
#endif

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
	printf ( "thr_sort: Gone!\n");
}

static void f_linger ( int time )
{
	/*
	thr_delay ( time * timer_rate );
	*/
	thr_delay_c ( time * timer_rate, f_croak, 0 );
	printf ( "thr_sort: Exit!\n");
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

/* --------------- network tests ------------------ */

#ifdef WANT_NET

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
test_icmp ( int test )
{
	ping ( test_ip );
}

static void
test_bootp ( int test )
{
	bootp_test();
}

static void
test_dhcp ( int test )
{
	dhcp_test();
}

static void
test_dns ( int test )
{
	dns_test();
}

static void
test_tcp_echo ( int test )
{
	tcp_echo_test();
}

static void
test_arp ( int count )
{
	int i;

	pkt_arm ();

	for ( i=0; i < count; i++ ) {
	    // thr_delay ( 10 );
	    pkt_arrive ();
	    arp_announce ();
	}
}

/* The following expects the UDP echo service to
 * be running on the selected host.
 * To get this going on a unix system:
 *   su
 *   yum install xinetd
 *   cd /etc/xinetd.d
 *   edit echo-dgram and enable it
 *   service xinetd restart
 * After this,
 *   "n 9" should run this test
 * I tried sending 1000 packets with a 10 ms delay, but this triggered some safeguard
 * on my linux server after 50 packets with the following messages:
 * Kyu, ready> n 9
 * sending 1000 UDP messages
 * 50 responses to 1000 messages
Aug  2 12:17:43 localhost xinetd[14745]: START: echo-dgram pid=0 from=::ffff:192.168.0.11
Aug  2 12:17:43 localhost xinetd[14745]: Deactivating service echo due to excessive incoming connections.  Restarting in 10 seconds.
Aug  2 12:17:43 localhost xinetd[14745]: FAIL: echo-dgram connections per second from=::ffff:192.168.0.11
Aug  2 12:17:53 localhost xinetd[14745]: Activating service echo
 *
 * In many ways the xinetd echo service is unfortunate.
 * Among other things it puts a line in the log for every packet it echos !!
 * And it throttles traffic.  So we found a simple UDP echo server online
 * and use it instead.
 */

#define UTEST_SERVER	"192.168.0.5"
#define UTEST_PORT	7

static int udp_echo_count;

static void
udp_test_rcv ( struct netbuf *nbp )
{
	// printf ( "UDP response\n" );
	udp_echo_count++;
}

/* With the xinetd server, we must delay at least 40 ms
 * between each packet.  Pretty miserable.
 */
// #define ECHO_COUNT 5000
// #define ECHO_DELAY 50 OK
// #define ECHO_DELAY 25 too fast
// #define ECHO_DELAY 40 OK

/* With a real echo server and 5 bytes messages,
 *  this runs in about 7 seconds.
 */
#define ECHO_COUNT 10000
#define ECHO_DELAY 1

#define UDP_TEST_SIZE	1024
static char udp_test_buf[UDP_TEST_SIZE];

#define UDP_BURST	5

/* With 1K packets and 5 packet bursts, we can run the test
 * in about 2 seconds with a decent server on the other end.
 */

static void
test_udp ( int xxx )
{
	int i;
	// char *msg = "hello";
	unsigned long test_ip;
	int local_port;
	int count;
	int len;

	// strcpy ( udp_test_buf, "hello" );
	// len = strlen ( udp_test_buf );

	len = UDP_TEST_SIZE;
	memset ( udp_test_buf, 0xaa, len );

	local_port = get_ephem_port ();
	(void) net_dots ( UTEST_SERVER, &test_ip );

	udp_hookup ( local_port, udp_test_rcv );

	count = ECHO_COUNT;
	udp_echo_count = 0;

	printf ("sending %d UDP messages\n", count );

	for ( i=0; i < count; i++ ) {
	    // printf ("sending UDP\n");
	    udp_send ( test_ip, local_port, UTEST_PORT, udp_test_buf, len );
	    if ( (i % UDP_BURST) == 0 )
		thr_delay ( ECHO_DELAY );
	}

	/* Allow time for last responses to roll in */
	thr_delay ( 100 );

	printf ( "%d responses to %d messages\n", udp_echo_count, ECHO_COUNT );
}

/* ---------------------------------------------------------- */
/* ---------------------------------------------------------- */

static struct {
	int arrive;
	int dispatch;
	int seen;
	int reply;
	int send;
	int finish;
} pk;

static int pk_limit = 0;

void pkt_arrive ()
{
	// We cannot all be using the ccnt and resetting it !!
	// This yielded some hard to debug issues with other parts
	// of the system using this for timing.  A basic lesson here.
	// reset_ccnt ();
	// pk.arrive = 0;
	pk.arrive = r_CCNT ();
	pk.dispatch = -1;
	pk.seen = -1;
	pk.reply = -1;
	pk.send = -1;
	pk.finish = -1;

}

void pkt_dispatch ()
{
	pk.dispatch = r_CCNT ();
}

void pkt_seen ()
{
	pk.seen = r_CCNT ();
}

void pkt_reply ()
{
	pk.reply = r_CCNT ();
}

void pkt_send ()
{
	pk.send = r_CCNT ();
}

// Called at interrupt level */
void pkt_finish ()
{
	pk.finish = r_CCNT ();

	if ( pk_limit == 0 )
	    return;
	pk_limit--;

	printf ( "arrive: %d\n", pk.arrive );
	printf ( "dispatch: %d\n", pk.dispatch );
	printf ( "seen: %d\n", pk.seen );
	printf ( "reply: %d\n", pk.reply );
	printf ( "send: %d\n", pk.send );
	printf ( "finish: %d\n", pk.finish );
}

void pkt_arm ()
{
	pk_limit = 5;
}

#define ENDLESS_SIZE	1024
static char endless_buf[ENDLESS_SIZE];
static int endless_count;
static int endless_port;
static unsigned long endless_ip;

unsigned long last_endless = 0;

/* Test this against the compiled C program in tools/udpecho.c
 * run that as ./udpecho 6789
 */
#define EECHO_PORT	6789

/* This is where all the action is when this gets going */
static void
endless_rcv ( struct netbuf *nbp )
{
	pkt_seen ();

	last_endless = * (unsigned long *) endless_buf;
	if ( last_endless != endless_count )
	    printf ( "Endless count out of sequence: %d %d\n", endless_count, last_endless );

	++endless_count;
	* (unsigned long *) endless_buf = endless_count;

	pkt_reply ();

	udp_send ( endless_ip, endless_port, EECHO_PORT, endless_buf, ENDLESS_SIZE );

	if ( endless_count < 2 )
	    printf ( "First UDP echo seen\n" );
	if ( (endless_count % 1000) == 0 ) {
	    printf ( "%5d UDP echos\n", endless_count );
#ifdef BOARD_ORANGE_PI
	    // emac_show_last ( 0 );
#endif
	}
}

/* Currently it takes 10 seconds to send 4000 exchanges,
 * i.e. 2.5 milliseconds per exchange.
 * So, we wait 3 milliseconds and then get concerned.
 * Suprisingly, this seems to work just fine.
 */
void
endless_watch ( int xxx )
{
	int count;
	int tmo = 0;

	while ( endless_count < 2 && tmo++ < 10 )
	    thr_delay ( 1 );

	if ( tmo > 8 ) {
	    printf ( "Never started\n" );
	    return;
	}
	printf ( "Watcher started\n" );

	for ( ;; ) {
	    count = endless_count;
	    // thr_delay ( 3 );
	    thr_delay ( 6 );
	    if ( count == endless_count )
		break;
	}
	printf ( "Stalled at %d\n", count );
#ifdef BOARD_ORANGE_PI
	capture_last ( 0 );
#endif
}

#ifdef notdef
#define NVALS 100

static void
check_clock_DETAIL ( void )
{
	unsigned int vals[NVALS];
	int secs;
	int delay = 100;
	unsigned int last;
	int count;
	int i;

	secs = NVALS * delay / 1000;

	printf ( "Collecting data for %d seconds\n", secs );
	last = r_CCNT ();

	for ( i=0; i< NVALS; i++ ) {
	    // reset_ccnt ();
	    thr_delay ( delay );
	    vals[i] = r_CCNT ();
	}

	for ( i=0; i< NVALS; i++ ) {
	    count = vals[i] - last;
	    last = vals[i];
	    printf ( "CCNT for 1 interval: %d %d\n", vals[i], count );
	}
}
#endif

/* 6-8-2018 -- this is actually quite bogus.
 * It would be fine if we were the only thing in the system
 * using the CCNT register for timing, but we aren't and only
 * one "thing" can use this at a time.
 */

#define NVALS 20

static void
check_clock ( void )
{
	int vals[NVALS];
	int i;
	int secs;
	int delay = 1000;

	secs = NVALS * delay / 1000;

	printf ( "Collecting data for %d seconds\n", secs );
	for ( i=0; i< NVALS; i++ ) {
	    reset_ccnt ();
	    thr_delay ( delay );
	    vals[i] = r_CCNT ();
	}

	for ( i=0; i< NVALS; i++ )
	    printf ( "CCNT for 1 sec: %d\n", vals[i] );
}

/* Endless echo of UDP packets
 * Call this to start the test.
 */
static void
test_udp_echo ( int test )
{

	// check_clock ();
	pkt_arm ();

	last_endless = 0;
	memset ( endless_buf, 0xaa, ENDLESS_SIZE );
	endless_count = 1;

	* (unsigned long *) endless_buf = endless_count;

	endless_port = get_ephem_port ();
	(void) net_dots ( UTEST_SERVER, &endless_ip );

	udp_hookup ( endless_port, endless_rcv );

	printf ( "Endless UDP test from our port %d\n", endless_port );

#ifdef BOARD_ORANGE_PI
	capture_last ( 1 );
#endif

	/* Kick things off with first message */
	udp_send ( endless_ip, endless_port, EECHO_PORT, endless_buf, ENDLESS_SIZE );

	(void) safe_thr_new ( "watcher", endless_watch, NULL, 24, 0 );

	/* No thread needed, the rest happens via interrupts */
}

/* Hook for board specific network statistics
 */
static void
test_netdebug ( int test )
{
	board_net_debug ();

	printf ( "last endless count = %d\n", last_endless );
}

#endif	/* WANT_NET */

/* THE END */
