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

#include "sklib.h"
#include "thread.h"
#include "intel.h"

#define PRI_BOSS	30

void ask_test ( int );

/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( int xx )
{

#ifdef notdef

#define SERV_PORT	0
#define SERV_PRIO	20

	printf ( "Starting Sserver on serial port %d\n", SERV_PORT );
	server_init ( SERV_PORT, SERV_PRIO );
#endif

#ifdef notdef
	printf ( "Starting terminal\n" );
	terminal_init ();
#endif

	tmr_rate_set ( 100 );
	(void) safe_thr_new ( "test", ask_test, (void *)0, PRI_BOSS, 0 );

	/*
	thr_show();
	*/
}

#ifdef notdef
void
weird_old_tests ( void )
{
	printf ( "mem_alloc = %08x\n", mem_alloc ( 128 ) );
	printf ( "mem_alloc = %08x\n", mem_alloc ( 128 ) );
	printf ( "mem_alloc = %08x\n", mem_alloc ( 128 ) );
	printf ( "mem_alloc = %08x\n", mem_alloc ( 128 ) );

	/*
	(void) safe_thr_new ( "debug", kb_debug, (void *) 0, PRI_DEBUG, 0 );
	*/
}
#endif

#ifdef TEST_DELAY_A
/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( int val )
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
user_init ( int xx )
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
