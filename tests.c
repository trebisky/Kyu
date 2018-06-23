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

/* This had turned into a 3000+ line disaster before being broken
 * up into several file in June of 2018.
 * Now we have one main "menu" and 3 submenus,
 * each collected into its own file.
 */

struct test_info {
	tfptr func;
	int arg;
	struct sem *sem;
	int times;
};

/* This gets launched as a thread to run each regression test
 *  when they are being selected one at a time.
 *  It repeats the test as indicated.
 */
static void
wrapper ( int arg )
{
	struct test_info *ip = (struct test_info *) arg;
	int i;

	for ( i=0; i< ip->times; i++ ) {
	    (*ip->func) ( ip->arg );
	}
}

static struct test_info info;

/* Used only for a single regression test */
static void
single_test ( struct test *tp, int repeat )
{
	/* Having this on the stack, calling a thread,
	 * then returning from this function is very bad,
	 * if the thread we pass it to expects this data
	 * to be stable.
	 */
	// struct test_info info;

	info.func = tp->func;
	info.arg = tp->arg;
	info.times = repeat;

	(void) safe_thr_new ( "wrapper", wrapper, (void *) &info, PRI_WRAP, 0 );

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

	single_test ( tp, repeat );
}

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

static void
submenu ( struct test *test_list, char **wp, int nw )
{
	struct test *tp;
	int nt;
	int n;
	int repeat = 1;

	for ( nt = 0, tp = test_list; tp->desc; ++tp )
	    ++nt;

	/* Just the letter gets help */
	if ( nw == 1 ) {
	    help_tests ( test_list, nt );
	    return;
	}

	n = atoi ( wp[1] );

	/* We allow a repeat for the regression suite */
	if ( nw == 3 )
	    repeat = atoi ( wp[2] );

	/* only run all tests for the regression test suite */
	if ( n == 0 && test_list == kyu_test_list ) {
	    all_tests ( repeat );
	    return;
	}

	if ( n < 1 || n > nt ) {
	    printf ( " ... No such test.\n" );
	    return;
	}

	/* It is easy to run a single test if this is
	 * not the regression suite.
	 * This ignores any repeat.
	 */
	if ( test_list != kyu_test_list ) {
	    (*test_list[n-1].func) ( test_list[n-1].arg );
	    return;
	}

	/* run single test for regression case ...
	 */
	single_regression ( &test_list[n-1], n, repeat );
}

static void
main_help ( void )
{
	printf ( "R - reboot board.\n" );
	printf ( "t - show thread list.\n" );
	printf ( "x func [args] - call C function.\n" );
	printf ( "k [num] [repeat] - Kyu thread regression tests.\n" );
	printf ( "i [num] - IO test menu.\n" );
	printf ( "n [num] - Network test menu.\n" );
	printf ( " .. and much more.\n" );
}

#define MAXB	64
#define MAXW	4

static void
tester ( void )
{
	char buf[MAXB];
	char *wp[MAXW];
	int n, nw, nl;
	int i;
	char *p;

	/* Allow the lower priority user thread to
	 * run (and exit), not that this matters all
	 * that much, but it does make debugging less
	 * confusing.
	 */
	thr_yield ();

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

	    /* Show threads.
	     * this gets used a lot too
	     */
	    if ( **wp == 't' && nw == 1 ) {
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
#endif
	    if ( **wp == 'y' ) {
		kyu_debugger ();
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

#ifdef notdef
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
 */
void
test_main ( int xx )
{
	tester ();
}

/* THE END */
