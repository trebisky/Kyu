#include "kyu.h"
#include "thread.h"

static void
blinker ( int xx )
{
	/* Writing a "1" does turn the LED on */
	gpio_led_set ( 1 );
	thr_delay ( 100 );
	gpio_led_set ( 0 );
}

void
user_init ( int xx )
{

	gpio_led_init ();
	thr_new_repeat ( "blinker", blinker, 0, 10, 0, 1000 );
}
