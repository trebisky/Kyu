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

/* XXX should just move to arch/cpu.h */
#ifdef ARM64
/* DAIF comes shifted left 6 bits as in the PSR */
#define DAIF_IRQ_BIT	0x80
#define DAIF_FIQ_BIT	0x40

static int
is_irq_disabled ( void )
{
	unsigned int val;

	get_DAIF ( val );

	if ( val & DAIF_IRQ_BIT )
	    return 1;

	return 0;
}
#endif

#ifndef ARM64
static int
is_irq_disabled ( void )
{
	unsigned int reg;

	get_CPSR ( reg );
	if ( reg & 0x80 )
	    return 1;
	return 0;
}
#endif

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

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* Basic, i.e. "thread" tests */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/* First automatic test follows ... */

static void test_basic ( long );

#ifdef WANT_SETJMP
static void test_setjmp ( long );
#endif

static void test_timer ( long );
static void test_delay ( long );
static void test_contin1 ( long );
static void test_contin2 ( long );

static void test_thread0 ( long );
static void test_thread1 ( long );
static void test_thread2 ( long );
static void test_thread3 ( long );
static void test_thread4 ( long );
static void test_thread5 ( long );
static void test_fancy ( long );
static void test_easy ( long );
static void test_cv1 ( long );
static void test_hard ( long );
static void test_join ( long );
static void test_mutex ( long );
static void test_cancel ( long );

/* These are the tests we run in the automatic regression set
 * Don't put anything ugly in here we cannot run in a loop.
 */
struct test kyu_test_list[] = {
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

#define USER_DELAY	10
#define AUTO_DELAY	1

static int usual_delay = USER_DELAY;

/* Hooks for main test gadget */
void
set_delay_auto ( void )
{
	usual_delay = AUTO_DELAY;
}

void
set_delay_user ( void )
{
	usual_delay = USER_DELAY;
}

/* -------------------------------------------------
 */

static volatile int ez;

void f_ez ( long arg )
{
	ez = arg;
}

/* -------------------------------------------------
 */

struct test_info {
	tfptr func;
	int arg;
	struct sem *sem;
	int times;
};

struct test_info static_info;

/* This gets launched as a thread to run
 * each of the basic tests */
void
basic_wrapper ( long arg )
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
 *
 * We might not need to make info static given that
 * we block here on the semaphore as we do,
 * but it reminds us of the risk of having it on
 * the stack, launching the thread, then returning.
 */
void
run_test ( tfptr func, int arg )
{
	static_info.func = func;
	static_info.arg = arg;
	static_info.sem = safe_sem_signal_new ();
	static_info.times = 1;	/* ignored */

	(void) safe_thr_new ( 
	    "bwrapper", basic_wrapper, (void *) &static_info, PRI_WRAP, 0 );

	sem_block ( static_info.sem );
	sem_destroy ( static_info.sem );
}

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
test_basic ( long xx )
{
	int i;
	long arg = 123;

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
 * (unless we have a simple polled serial output).
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
test_timer ( long count )
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
 * (but it revealed broken INT_lock on armv8 10-2018)
 * We do expect activations from interrupt timer events,
 * but the purpose here is to test the thr_delay() call.
 */

void
test_delay ( long count )
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

#ifdef DEBUG_2018
/* A modified form */
void
test_delay ( long count )
{
	int tick = 0;
	int i;

	printf ( "Now let's test thread delays.\n" );

	for ( i=0; i<count; i++) {
	    printf ( "Start delay\n" );
	    thr_delay ( 1000 );
	    tick++;
	    printf ( "Delay done: %d\n", tick );
	}

	printf ( " ... OK!\n" );
}

/* A simple form */
/* Used 10-10-2018 when tracking down broken INT_lock */
void
test_delay ( long count )
{
	printf ( "Let's test thread delays.\n" );

	printf ( "Start delay\n" );
	// thr_delay ( 1 );
	thr_delay ( 5000 );
	printf ( "Delay done\n" );

	printf ( " ... OK!\n" );
}


#endif

/* --------------------------------------------
 * Single continuation test.
 */

static void alarm ( long );
static void slave1 ( long );
static void slave2 ( long );

// static struct thread *cont_thread;
static struct thread *cont_slave;

static volatile int ready;

/* This little thread is just a timer to fire a
 * continuation at the other thread.
 */
static void
alarm ( long xx )
{
	thr_delay ( usual_delay );
	thr_unblock ( cont_slave );
}

static void
slave1 ( long xx )
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
slave2 ( long yy )
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
test_contin1 ( long xx )
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

static void c2_s1 ( long );
static void c2_s2 ( long );

static void
c2_s1 ( long limit )
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
c2_slave1 ( long limit )
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
c2_s2 ( long limit )
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
c2_slave2 ( long limit )
{
	thr_delay_c ( usual_delay, c2_s2, limit );
}

static void
test_contin2 ( long limit )
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
t0_func ( long arg )
{
	t0 = arg;
}

static void
test_thread0 ( long xx )
{
	long arg = 789;

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
t1_f1 ( long max )
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
t1_f2 ( long max )
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
test_thread1 ( long count )
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
t2_func ( long id )
{
	while ( t2_count++ < 5 ) {
	    printf ( " f%d(%d)", id, t2_count );
	    thr_yield ();
	}
	++done2;
}

static void
test_thread2 ( long xx )
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
t3_f1 ( long xx )
{
	t3_func ( 1, sem1, sem2 );
}

static void
t3_f2 ( long xx )
{
	t3_func ( 2, sem2, sem1 );
}

void
test_thread3 ( long xx )
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
t4_func ( long count )
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
test_thread4 ( long count )
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
t5_slave ( long id )
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
t5_master ( long count )
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
test_thread5 ( long count )
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
t6_slave ( long id )
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
test_fancy ( long count )
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
slim79 ( long count )
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
busy79 ( long nice )
{
	unsigned int psr;

	/*
	printf ( "busy starting\n" );
	*/
	while ( t7_run ) {
	    if ( nice )
		thr_yield ();
	    ++t7_sum;
	    if ( is_irq_disabled() )
		printf ("busy -- no IRQ --\n" );
	}
}

static void
test_79 ( long count, long nice )
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
test_easy ( long count )
{
	printf ( "Easy interrupt activation test.\n" );

	test_79 ( count, 1 );

	printf ( " OK\n" );
}

static void
test_hard ( long count )
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
cv1_func ( long x )
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
test_cv1 ( long count )
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
join_func ( long time )
{
	if ( time )
	    thr_delay ( time );
	printf ( "exit " );
}

static void
test_join ( long count )
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
mutex_func ( long time )
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
test_mutex ( long count )
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
cancel_func ( long type )
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
test_cancel ( long count )
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
test_setjmp ( long xx )
{
	printf ( "Running setjmp test\n" );
	jmp_test1 ();
	jmp_test2 ();
}
#endif

/* THE END */
