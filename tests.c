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

#include "tests.h"

extern struct test net_test_list[];
extern struct test kyu_test_list[];
extern struct test io_test_list[];

struct test *cur_test_list = kyu_test_list;

static int test_debug = 0;

struct test_info {
	tfptr func;
	int arg;
	struct sem *sem;
	int times;
};

/* Wrapper function to catch troubles when making new semaphores.
 */
struct sem *
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

static void
all_tests ( int nl )
{
	struct test *tp;
	int i;

	if ( cur_test_list != kyu_test_list )
	    return;

	if ( nl < 1 )
	    nl = 1;

	for ( i=0; i<nl; i++ )
	    for ( tp = &kyu_test_list[0]; tp->func; tp++ ) {
		(*tp->func) ( tp->arg );
	    }
}

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
	info.times = times;

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
	    	cur_test_list = kyu_test_list;
	    }

	    if ( **wp == 'y' ) {
		kyu_debugger ();
	    }

#ifdef ARCH_ARM
	    if ( **wp == 'R' ) {
	    	/* Reboot the machine */
		printf ( "Rebooting\n" );
	    	reset_cpu ();
	    }

	    if ( **wp == 's' ) {
		show_my_regs ();
	    }
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

		if ( n == 0 && cur_test_list == kyu_test_list ) {
		    all_tests ( nl );
		    continue;
		}

		/* No looping except for standard tests */
		if ( cur_test_list != kyu_test_list )
		    nl = 1;

		if ( n < 1 || n > nt ) {
		    printf ( " ... No such test.\n" );
		    continue;
		}

		desc = cur_test_list[n-1].desc;
		if ( nl > 1 ) {
		    printf ( "looping test %d (%s), %d times\n", n, desc, nl );
		    // usual_delay = AUTO_DELAY;
		    set_delay_auto ();
		} else {
		    printf ( "Running test %d (%s)\n", n, desc );
		    // usual_delay = USER_DELAY;
		    set_delay_user ();
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

/* THE END */
