#include <kyu.h>
#include <thread.h>

static struct thread *user_thread;
static int count;

static void
ticker ( void )
{
	++count;
	if ( count > 49 ) {
	    thr_unblock ( user_thread );
	    count = 0;
	}
}

void
user_init ( int xx )
{
	int led = 0;

	count = 0;
	user_thread = thr_self ();
	gpio_led_init ();
	timer_hookup ( ticker );

	for ( ;; ) {
	    thr_block ( WAIT );
	    gpio_led_set ( led );
	    led = (led+1) % 2;
	}
}
