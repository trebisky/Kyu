/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * test_io.c
 * Tom Trebisky
 *
 * began as user.c -- 11/17/2001
 * made into tests.c  9/15/2002
 * added to kyu 5/11/2015
 * split into test_io.c  --  6/23/2018
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"
#include "arch/cpu.h"

#include "tests.h"

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* IO tests -- anything that should not be run in a loop */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/* These go in the "i" menu which is kind of a grab bag
 * of mostly architecture dependent tests.
 *
 * Don't run these tests automatically.
 * Don't run them in a repeat loop either.
 */
static void test_sort ( long );
static void test_ran ( long );
static void test_malloc ( long );
static void test_wait ( long );
static void test_unroll ( long );
static void test_fault ( long );
static void test_zdiv ( long );
static void test_gpio ( long );
static void test_clock ( long );
static void test_regs ( long );

#ifdef BOARD_BBB
static void test_adc ( long );
static void test_fault2 ( long );
#endif

#if defined(BOARD_ORANGE_PI) || defined(BOARD_FIRE3)
static void test_cores ( long );
#endif

#ifdef BOARD_ORANGE_PI
static void test_wdog ( long );
static void test_thermal ( long );
static void test_uart ( long );
static void test_ram1 ( long );
static void test_ram2 ( long );
static void test_button ( long );
static void test_blink_s ( long );
static void test_blink ( long );
#endif

#if defined(BOARD_ORANGE_PI64)
static void test_timer ( long );
#endif

static void test_blink_d ( long );
static void test_clear ( long );
static void test_cache ( long );

/* Here is the IO test menu */
/* arguments are now ignored */

struct test io_test_list[] = {
	test_sort,	"Thread sort test",	0,
	test_ran,	"Random test",		0,
	test_malloc,	"malloc test",		0,
	test_wait,	"wait for [n] seconds",	0,
	test_unroll,	"stack traceback",	0,
	test_fault,	"Fault test",		0,
	test_zdiv,	"Zero divide test",	0,
	test_gpio,	"GPIO test",		0,
	test_clock,	"CPU clock test",	0,
	test_regs,	"Show registers",	0,

#ifdef BOARD_BBB
	test_adc,	"BBB adc test",		0,
	test_fault2,	"data abort probe",	0,
#endif

#if defined(BOARD_ORANGE_PI) || defined(BOARD_FIRE3)
	test_cores,	"Multiple core test",	0,
#endif

#ifdef BOARD_ORANGE_PI
	test_wdog,	"Watchdog test",	0,
	test_thermal,	"H3 thermal test",	0,
	test_uart,	"uart test",		0,
	test_ram1,	"Opi low ram test",	0,
	test_ram2,	"Opi ram test",		0,
	test_button,	"Opi button",		0,

	test_blink,	"start LED blink test",	0,
	test_blink_s,	"stop LED blink test",	0,
#endif

#if defined(BOARD_ORANGE_PI64)
	test_timer,	"Timer test",		0,
#endif

	test_blink_d,	"LED blink test (via delay)",	0,
	test_clear,	"clear memory test",	0,
	test_cache,	"cache test",		0,

	0,		0,			0
};

/* -------------------------------------------- */

/* On the Orange Pi there is SRAM at 0-0xffff.
 * also supposed to be at 0x44000 to 0x4Bfff (I see this)
 * also supposed to be at 0x10000 to 0x1afff, but not for me.
 */

/* I swear this worked once on the Orange Pi */
static void
test_ram1 ( long xxx )
{
	printf ( "Memory test - verify 0 to 0x10000\n" );
	mem_verify ( 0x0, 0x10000 );
}

static void
test_ram2 ( long xxx )
{
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

#ifdef BOARD_ORANGE_PI
/* Test Orange Pi button */
static void
test_button ( long xxx )
{
	gpio_test_button ();
}
#endif

static void
test_malloc ( long xxx )
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

/* Wait for N seconds */
/* This runs in its own thread,
 * which can be interesting.
 */
static void
test_wait ( long secs )
{
	printf ( "Waiting ...\n" );
	thr_delay ( secs * timer_rate_get() );
	printf ( "Done waiting\n" );
}

/* -------------------------------------------- */

/* test ARM stack backtrace */
static void
test_unroll ( long xxx )
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
test_fault ( long xxx )
{
	fault_addr_zero ();
	fault_align ();
}

static void
test_zdiv ( long xxx )
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
test_fault2 ( long xxx )
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

#if defined(BOARD_ORANGE_PI) || defined(BOARD_FIRE3)
void
test_cores ( long xxx )
{
	/* official test */
	test_core ();

	/* the crazy business */
	// check_core ();
}
#endif

#ifdef BOARD_ORANGE_PI
void
test_thermal ( long xxx )
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
led_blinker ( long xx )
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
led_blinker ( long xx )
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
test_uart ( long arg )
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

unsigned long ram_next ( void );
unsigned long ram_size ( void );

static void
test_clear ( long arg )
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

static void
test_cache ( long arg )
{
	arch_cache_test ();
}

#ifdef BOARD_ORANGE_PI
static struct thread *blink_tp;

/* start the blink */
static void
test_blink ( long xxx )
{
	printf ( "Start the blink\n" );
	blink_tp = thr_new_repeat ( "blinker", led_blinker, 0, 10, 0, BLINK_RATE );

	led_norm ();
}

/* stop the blink */
static void
test_blink_s ( long xxx )
{
	printf ( "Stop the blink\n" );
	thr_repeat_stop ( blink_tp );

	led_norm ();
}
#endif

#if defined(BOARD_ORANGE_PI64)
static void
test_timer ( long xxx )
{
	reg_t val;

	get_DAIF ( val );
	printf ( "DAIF = %08x\n", val );

	timer_check1 ();
	timer_check2 ();

	INT_lock;
	get_DAIF ( val );
	printf ( "DAIF (locked) = %08x\n", val );

	INT_unlock;
	get_DAIF ( val );
	printf ( "DAIF (unlocked) = %08x\n", val );
}
#endif

#if defined(BOARD_ORANGE_PI) || defined(BOARD_FIRE3) || defined(BOARD_ORANGE_PI64)
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

/* XXX - should have BBB provide a "status" function as above */
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
test_blink_d ( long arg )
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

static void check_clock ( void );

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


static void test_clock ( long count ) {
	check_clock ();
}

static void test_gpio ( long count ) { gpio_test (); }

#ifdef BOARD_ORANGE_PI
/* Test watchdog on Orange Pi */
static void test_wdog ( long count ) { wd_test (); }
#endif

#ifdef BOARD_BBB

/* Test adc on BBB */
static void test_adc ( long count ) { adc_test (); }
#endif

/* -------------------------------------------- */
/* Random numbers.
 */

#define NB 20
#define MAXB 64

static void
test_ran ( long count )
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

static void f_croak ( long junk )
{
	printf ( "thr_sort: going bye bye (OK)!\n");
}

static void f_linger ( long time )
{
	/*
	thr_delay ( time * timer_rate_get() );
	*/
	thr_delay_c ( time * timer_rate_get(), f_croak, 0 );

	/* Should never see this */
	printf ( "thr_sort: Exit!\n");
}

/* This test just verifies that threads get inserted into
 * the ready list in numerical order.
 * At first we just left the threads blocked and there
 * was no way to get rid of them.  Then I got the idea
 * of using thr_delay to have them go away after a while.
 * A "while" is 9 seconds.  Use the "l" command to look at
 * the ready list before this happens and visually confirm
 * proper order.
 *
 * This exposed a bug (the keyboard no longer worked after
 * the timeout, well at least not from the prompt, the
 * CAPS lock would still do a reboot.).  This is only the
 * case when the CV option is in use for the keyboard, and
 * when the current thread is not the one delayed.
 * This bug was fixed 8/22/2002 (it was in thr_unblock)
 */
static void
test_sort ( long xxx )
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

static void
test_regs ( long xxx )
{
	show_my_regs ();
}

/* THE END */
