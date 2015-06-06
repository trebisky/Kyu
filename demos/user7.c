#include "kyu.h"
#include "thread.h"

static int led = 0;

static void
blinker ( int xx )
{
	gpio_led_set ( led );
	led = (led+1) % 2;
}

void
user_init ( int xx )
{

	gpio_led_init ();
	thr_new_repeat ( "blinker", blinker, 0, 10, 0, 500 );
}
