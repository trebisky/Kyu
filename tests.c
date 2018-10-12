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

extern struct test kyu_test_list[];
extern struct test io_test_list[];
extern struct test net_test_list[];
extern struct test tcp_test_list[];

/* This file had turned into a 3000+ line disaster before being broken
 * up into several file in June of 2018.
 * Now we have one main "menu" and several submenus,
 * each collected into its own file.
 */

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */

struct test_info {
	tfptr func;
	int arg;	/* only used for regression tests */
	int times;
};

/* This gets launched as a thread to run each regression test
 *  when they are being selected one at a time.
 *  It repeats the test as indicated.
 */
static void
r_wrapper ( long arg )
{
	struct test_info *ip = (struct test_info *) arg;
	int i;

	for ( i=0; i< ip->times; i++ ) {
	    (*ip->func) ( ip->arg );
	}
}

static struct test_info r_info;

/* Run a single regression test, perhaps with looping
 */
static void
single_regression ( struct test *tp, int n, int repeat )
{
	char *desc;

	desc = tp->desc;
	if ( repeat > 1 ) {
	    printf ( "looping test %d (%s), %d times\n", n, desc, repeat );
	    set_delay_auto ();
	} else {
	    printf ( "Running test %d (%s)\n", n, desc );
	    set_delay_user ();
	}

	/* Having this on the stack, calling a thread,
	 * then returning from this function is very bad,
	 * if the thread we pass it to expects this data
	 * to be stable.
	 */
	// struct test_info r_info;

	r_info.func = tp->func;
	r_info.arg = tp->arg;
	r_info.times = repeat;

	(void) safe_thr_new ( "r_wrapper", r_wrapper, (void *) &r_info, PRI_WRAP, 0 );

/* This nice synchronization does not give us
 * the desired result when a thread faults.
 * So we launch the test and return to the
 * command line.
 */
#ifdef notdef
	/* Or we could do a join */
	sem_block ( r_info.sem );
	sem_destroy ( r_info.sem );
#endif
}

/* For regression tests only.
 * Note that this does not run each in its own thread.
 * The idea is that if a given test is giving trouble,
 * It should be run by itself, which would be done in
 * a dedicated thread.
 */
static void
all_tests ( int repeat )
{
	struct test *tp;
	int i;

	if ( repeat < 1 )
	    repeat = 1;

	for ( i=0; i<repeat; i++ )
	    for ( tp = &kyu_test_list[0]; tp->func; tp++ ) {
		(*tp->func) ( tp->arg );
	    }
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */

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

/* This gets launched as a thread to run each test
 *  when they are being selected one at a time.
 *  The test will do its own repeats if it implements that.
 */
static void
n_wrapper ( long arg )
{
	struct test_info *ip = (struct test_info *) arg;

	(*ip->func) ( ip->times );
}

static struct test_info n_info;

static void
run_test ( struct test *tp, int repeat )
{
	n_info.func = tp->func;
	n_info.times = repeat;

	// (*tp->func) ( repeat );
	(void) safe_thr_new ( "n_wrapper", n_wrapper, (void *) &n_info, PRI_WRAP, 0 );
}

static void
submenu ( struct test *test_list, char **wp, int nw )
{
	struct test *tp;
	int nt;
	int test;
	int repeat = 1;

	for ( nt = 0, tp = test_list; tp->desc; ++tp )
	    ++nt;

	/* Just the letter gets help */
	if ( nw == 1 ) {
	    help_tests ( test_list, nt );
	    return;
	}

	test = atoi ( wp[1] );

	/* We allow a repeat for the regression suite */
	if ( nw == 3 )
	    repeat = atoi ( wp[2] );

	/* only run all tests for the regression test suite */
	if ( test == 0 && test_list == kyu_test_list ) {
	    all_tests ( repeat );
	    return;
	}

	if ( test < 1 || test > nt ) {
	    printf ( " ... No such test.\n" );
	    return;
	}

	tp = &test_list[test-1];

	/* It is easy to run a single test if this is
	 * not the regression suite.
	 * we pass the repeat count to the test,
	 *  which in most cases ignores it.
	 */
	if ( test_list != kyu_test_list ) {
	    // (*tp->func) ( test_list[test-1].arg );
	    // (*tp->func) ( repeat );
	    run_test ( tp, repeat );
	    return;
	}

	/* run single test for regression case ...
	 */
	single_regression ( tp, test, repeat );
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */

static void
main_help ( void )
{
	printf ( "R - reboot board.\n" );
	printf ( "l - show thread list.\n" );
	printf ( "x func [args] - call C function.\n" );
	printf ( "k [num] [repeat] - Kyu thread regression tests.\n" );
	printf ( "i [num] - IO test menu.\n" );
	printf ( "n [num] - Network test menu.\n" );
	printf ( "t [num] - TCP test menu.\n" );
	printf ( "u name - details on thread by name\n" );
}

#define MAXB	64
#define MAXW	4

/* This normally gets run as the test thread.
 */
void
test_main ( long xx )
{
	char buf[MAXB];
	char *wp[MAXW];
	int n, nw, nl;
	int i;
	char *p;
	reg_t val;

	/* Allow the lower priority user thread to
	 * run (and exit), not that this matters all
	 * that much, but it does make debugging less
	 * confusing.
	 */
	thr_yield ();

	/*
	printf ( "TJT in test_main\n" );
	get_SP ( val );
	printf ( " test_main() starting with stack: %016lx\n",  val );
	*/

	for ( ;; ) {

	    printf ( "Kyu, ready> " );
	    getline ( buf, MAXB );
	    if ( ! buf[0] )
	    	continue;

	    nw = split ( buf, wp, MAXW );

	    if ( **wp == 'h' )
		main_help ();

	    /* We use this more than anything else in
	     * this menu.  It usually calls some board specific
	     * reboot function.
	     */
	    if ( **wp == 'R' ) {
	    	/* Reboot the machine */
		printf ( "Rebooting\n" );
	    	reset_cpu ();
	    }

	    /* Show threads (l = "list")
	     * this gets used a lot too
	     */
	    // if ( **wp == 'l' && nw == 1 ) {
	    if ( **wp == 'l' ) {
	    	thr_show ();
	    }

	    /* Run a C function by name
	     */
	    if ( **wp == 'x' ) {
		shell_x ( &wp[1], nw-1 );
	    }

	    /* kyu thread test submenu
	     *  (regression tests)
	     */
	    if ( **wp == 'k' )
		submenu ( kyu_test_list, wp, nw );

	    /* IO test submenu
	     */
	    if ( **wp == 'i' )
		submenu ( io_test_list, wp, nw );

#ifdef WANT_NET
	    /* network test submenu
	     */
	    if ( **wp == 'n' )
		submenu ( net_test_list, wp, nw );

	    if ( **wp == 't' )
		submenu ( tcp_test_list, wp, nw );
#endif
	    if ( **wp == 'y' ) {
		kyu_debugger ();
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

	    /* Turn on Kyu thread debugging */
	    if ( **wp == 'o' ) {
		if ( nw > 1 )
		    thr_debug ( atoi (wp[1]) );
		else
		    thr_debug ( 0 );
	    }

	    /* Show a specific thread given its name.
	     */
	    if ( **wp == 'u' && nw == 2 ) {
	    	thr_show_name ( wp[1] );
	    }

	}
}

/* THE END */
