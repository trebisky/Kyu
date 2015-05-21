#include "kyu.h"
#include "thread.h"

static volatile int wally = 0;
static volatile int count1 = 0;
static volatile int count2 = 0;
static struct sem *lock_sem;

#define HZ	10000

void
racer1 ( int xx )
{
	for ( ;; ) {
	    thr_delay ( 1 );

	    sem_block ( lock_sem );
	    wally++;
	    count1++;
	    sem_unblock ( lock_sem );
	}
}

void
racer2 ( int xx )
{
	int races = 0;
	int got_race;

	while ( races < 10 && count1 < 60*HZ ) {

	    sem_block ( lock_sem );
	    wally++;
	    count2++;
	    got_race = (wally != count1 + count2 );
	    sem_unblock ( lock_sem );

	    if ( got_race ) {
		printf ( "Race after %d / %d (%d)\n", count1, count2, wally );
		wally = count1 + count2;
		races++;
	    }
	}
	printf ( "Done\n" );
}

void
user_init ( int xx )
{
	timer_rate_set ( HZ );
	lock_sem = sem_mutex_new ( SEM_PRIO );
	(void) thr_new ( "racer1", racer1, (void *) 0, 20, 0 );
	(void) thr_new ( "racer2", racer2, (void *) 0, 30, 0 );
}
