/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* user.c
 * $Id: user.c,v 1.15 2005/03/25 20:52:50 tom Exp $
 * Tom Trebisky  11/17/2001
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

#define PRI_SHELL	40

void test_main ( long );

#ifdef notdef
volatile double q;

static void
floater ( void )
{
	double x;
	double y;

	x = 3.141593;
	y = 1.0 / 3.0;
	q /= x + y;

	// printf ( "Pi = %.7f\n", x );
}
#endif

/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( long xx )
{

#ifdef notdef
	timer_rate_set ( 100 );
#endif

	// printf ( "In user_init\n" );

#ifdef WANT_MMT_PADDLE
	mmt_paddle_init ();
#endif

	/* Run the test "shell" as the user thread */
	(void) safe_thr_new ( "shell", test_main, (void *)0, PRI_SHELL, 0 );

	/*
	thr_show();
	*/
}

#ifdef TEST_DELAY_A
/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( long val )
{
	printf ( "toot = %d\n", val );
	thr_delay_a ( 1, user_init, val+1 );
	printf ( "Exits\n" );
}
#endif

#ifdef TEST_DELAY_Q
static int tooter = 0;

/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( long xx )
{
	++tooter;
	printf ( "toot = %d\n", tooter );
	/*
	thr_delay_q ( 25 );
	*/
	thr_delay_a ( 25, user_init, 0 );
	printf ( "Exits\n" );
}
#endif

/* THE END */
