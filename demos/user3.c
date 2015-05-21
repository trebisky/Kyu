#include "kyu.h"
#include "thread.h"

static struct sem *user_sem;
static int count;

static void
ticker ( void )
{
	++count;
	if ( count > 49 ) {
	    sem_unblock ( user_sem );
	    count = 0;
	}
}

static int led = 0;

static void
blinker ( int xx )
{
	gpio_led_set ( led );
	led = (led+1) % 2;

	sem_block_q ( user_sem );
}

void
user_init ( int xx )
{

	count = 0;
	user_sem = sem_signal_new ( SEM_PRIO );
	gpio_led_init ();
	timer_hookup ( ticker );

	sem_block_c ( user_sem, blinker, 0 );
}
