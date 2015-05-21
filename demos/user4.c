#include "kyu.h"
#include "thread.h"

static volatile int wally = 0;
static volatile int count1 = 0;
static volatile int count2 = 0;
static struct sem *lock_sem;

void
racer1 ( int xx )
{
	for ( ;; ) {
	    thr_delay ( 1 );
	    count1++;
	    wally++;
	}
}

void
racer2 ( int xx )
{
	int races = 0;

	/*
	show_my_regs ();
	printf ( "got %d,", wally );
	printf ( "expected %d", orig+5 );
	printf ( "\n" );
	*/

	while ( races < 10 ) {

	    wally++;
	    count2++;
	    if ( wally != count1 + count2 ) {
		printf ( "Race after %d / %d (%d)\n", count1, count2, wally );
		wally = count1 + count2;
		races++;
		/*
		show_my_regs ();
		printf ( "got %d,", wally );
		printf ( "expected %d", orig+5 );
		printf ( "\n" );
		*/
	    }

	    /*
	    sem_block ( lock_sem );
	    sem_unblock ( lock_sem );
	    */
	}
	printf ( "Done\n" );
}

void
user_init ( int xx )
{
	timer_rate_set ( 10000 );
	lock_sem = sem_mutex_new ( SEM_PRIO );
	(void) thr_new ( "racer1", racer1, (void *) 0, 20, 0 );
	(void) thr_new ( "racer2", racer2, (void *) 0, 30, 0 );

	thr_show ();
}
