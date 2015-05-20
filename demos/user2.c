#include <kyu.h>
#include <thread.h>

static struct thread *user_thread;
static int count;

static void
ticker ( void )
{
	++count;
	if ( count > 50 ) {
	    thr_unblock ( user_thread );
	    count = 0;
	}
}

static int led = 0;

static void
blinker ( int xx )
{
	gpio_led_set ( led );
	led = (led+1) % 2;
	thr_block_q ( WAIT );
}

void
user_init ( int xx )
{

	count = 0;
	user_thread = thr_self ();
	gpio_led_init ();
	timer_hookup ( ticker );

	thr_block_c ( WAIT, blinker, 0 );
}
