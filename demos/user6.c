#include "kyu.h"
#include "thread.h"

static int led = 0;

static void
blink ( int xx )
{
	gpio_led_set ( led );
	led = (led+1) % 2;
}

static void
blinker ( int xx )
{
	gpio_led_init ();
	thr_repeat_c ( 500, blink, 0 );
}

void
user_init ( int xx )
{

	thr_new ( "blinker", blinker, 0, 10, 0 );
}
