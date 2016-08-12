/* user.c
 * $Id: user.c,v 1.15 2005/03/25 20:52:50 tom Exp $
 * Tom Trebisky  11/17/2001
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

#define PRI_BOSS	30

void test_main ( int );

/* Introduced 7-17-2016 while playing with
 * ARM timers and the ARM mmu
 */
#define MMU_SIZE (4*1024)

static void
mmu_scan ( unsigned long *mmubase )
{
	int i;

	/* There is a big zone (over RAM with 0x0c0e */
	for ( i=0; i<MMU_SIZE; i++ ) {
	    if ( (mmubase[i] & 0xffff) != 0x0c12 )
		printf ( "%4d: %08x: %08x\n", i, &mmubase[i], mmubase[i] );
	}
}

void
toms_debug ( void )
{
	unsigned long *mmubase;

	// show_bogus_timer ();

	printf ( "SCTRL = %08x\n", get_sctrl() );

	mmubase = (unsigned long *) get_mmu ();
	if ( ! mmubase )
	    printf ( "MMU not initialized !\n" );
	else
	    printf ( "MMU base = %08x\n", get_mmu() );

	if ( get_prot () )
	    printf ( "Protection unit base = %08x\n", get_prot() );

	if ( mmubase ) {
	    // mmu_scan ( mmubase );
	    // printf ( "mmu checking done\n" );
	}

	// show_cpsw_debug ();

#ifdef notdef
	peek ( 0x44E30000 );	/* CM */
	peek ( 0x44E35000 );	/* WDT1 */
	peek ( 0x44E31000 ); 	/* Timer 1 */
#endif
}

/*
 * user_init is the hook that allows the user to
 * set up his threads and such, to customize the
 * system for his application.
 */
void
user_init ( int xx )
{

#ifdef notdef
	timer_rate_set ( 100 );
#endif

	toms_debug ();

	(void) safe_thr_new ( "test", test_main, (void *)0, PRI_BOSS, 0 );

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
