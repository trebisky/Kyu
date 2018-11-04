#include "kyu.h"
#include "thread.h"

#include "netbuf.h"
#include "board/gpio.h"

/* mmt_paddle.c
 * to build Kyu and include this, edit kyu.h
 * and define WANT_MMT_PADDLE
 *
 * Tom Trebisky  11-1-2018
 */

#define PADDLE_LED	GPIO_P8_7

#define PADDLE_P51	GPIO_P8_8	/* grey (f) */
#define PADDLE_P52	GPIO_P8_9	/* white (u) */
#define PADDLE_P53	GPIO_P8_10	/* yellow (d) */

/* This uses a Sparkfun COM-10411 "Wearable Keypad" for
 * a telescope paddle.
 *
 * Note that the MMT reverses UDLR from schematic.
 * This code uses nomenclature as per schematic.
 * This is correct with cable exiting top of pad.
 * MMT lingo is with cable exiting bottom of pad,
 *  which is apparently how the operator likes to use it.
 */

#define FLAME	0

/* Currently the shell runs at 40 and polls the uart,
 * so we get starved unless we run at a higher priority.
 */
#define PADDLE_PRI	35
#define CHECK_PRI	36

#define MOUNT_HOST	"192.168.0.5"
#define MOUNT_PORT	1234

/* Our port to receive UDP */
#define PADDLE_PORT	1234

/* Note that gpio_read_bit() either returns 0,
 *  or something non-zero (but not necessarily 1)
 */
void
get_button_bits ( int *bits )
{
	// gpio_in_up ( PADDLE_P51 );
	// gpio_in_up ( PADDLE_P52 );
	// gpio_in_up ( PADDLE_P53 );

	/* FUD */
	bits[0] = gpio_read_bit ( PADDLE_P51 ) ? 0 : 1;
	bits[1] = gpio_read_bit ( PADDLE_P52 ) ? 0 : 1;
	bits[2] = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;

	/* If D is pushed, reading L and R is meaningless */
	if ( bits[2] ) {
	    bits [3] = 0;
	    bits [4] = 0;
	    return;
	}

	/* Without the delays below we get a constant false L reading */

	/* R */
	gpio_out_init ( PADDLE_P52 );
	gpio_clear_bit ( PADDLE_P52 );
	thr_delay ( 5 );
	bits[3] = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;
	gpio_in_up ( PADDLE_P52 );

	/* L */
	gpio_out_init ( PADDLE_P51 );
	gpio_clear_bit ( PADDLE_P51 );
	thr_delay ( 5 );
	bits[4] = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;
	gpio_in_up ( PADDLE_P51 );
}

void
show_bits ( int *bits )
{
	char stat[6];
	char *p;

	stat[0] = bits[0] ? 'F' : '.';
	stat[1] = bits[1] ? 'U' : '.';
	stat[2] = bits[2] ? 'D' : '.';
	stat[3] = bits[3] ? 'R' : '.';
	stat[4] = bits[4] ? 'L' : '.';
	stat[5] = '\0';

	printf ( " %s\n", stat );
}

static unsigned long mount_ip;

void
mmt_paddle ( long xxx )
{
	int bits[5];

	(void) net_dots ( MOUNT_HOST, &mount_ip );

	gpio_out_init ( PADDLE_LED );

#ifdef STARTUP_BLINK
	/* Blink the LED twice on startup */
	gpio_set_bit ( PADDLE_LED );
	thr_delay ( 500 );
	gpio_clear_bit ( PADDLE_LED );

	thr_delay ( 500 );

	gpio_set_bit ( PADDLE_LED );
	thr_delay ( 500 );
	gpio_clear_bit ( PADDLE_LED );
#endif

	/* Turn off the LED */
	gpio_clear_bit ( PADDLE_LED );

	// jam_gpios ();
	// diag_l ();
	// diag_r ();
	// diag ();

	// Make these inputs
	// with pullup enabled
	gpio_in_up ( PADDLE_P51 );
	gpio_in_up ( PADDLE_P52 );
	gpio_in_up ( PADDLE_P53 );

	for ( ;; ) {
	    get_button_bits ( bits );
	    show_bits ( bits );
	    if ( bits[FLAME] ) {
		printf ( "Send\n" );
		udp_send ( mount_ip, PADDLE_PORT, MOUNT_PORT, "ping", 4 );
	    }
	    thr_delay ( 500 );
	}

	// blink the LED
	// thr_new_repeat ( "blinker", blinker, 0, 10, 0, 1000 );
}

static int mmt_alive = 1;

/* Any UDP packet received will indicate life */
void
heartbeat ( struct netbuf *nbp )
{
	mmt_alive = 1;
}

/* This runs once every few seconds to do the heartbeat check
 */
void
mmt_check ( long xxx )
{
	if ( mmt_alive ) {
	    gpio_set_bit ( PADDLE_LED );
	} else {
	    gpio_clear_bit ( PADDLE_LED );
	}
	mmt_alive = 0;
	udp_send ( mount_ip, PADDLE_PORT, MOUNT_PORT, "ping", 4 );
}

/* This is called from user_init when the paddle is configured.
 * It launches the paddle thread and returns.
 */
void
mmt_paddle_init ( void )
{
	udp_hookup ( PADDLE_PORT, heartbeat );

	(void) thr_new_repeat ( "mmt_check", mmt_check, 0, CHECK_PRI, 0, 2000 );
	(void) thr_new ( "mmt_paddle", mmt_paddle, (void *) 0, PADDLE_PRI, 0 );
}

/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
/* ------------------------------------------------------ */

/* Below here is cruft used for testing and experiments */

#ifdef notdef

static void
blinker ( long xx )
{
	gpio_set_bit ( PADDLE_LED );
	thr_delay ( 100 );
	gpio_clear_bit ( PADDLE_LED );
}

void
diag_l ( void )
{
	int bit;
	char stat;

	gpio_in_up ( PADDLE_P52 );
	gpio_in_up ( PADDLE_P53 );

	gpio_out_init ( PADDLE_P51 );
	gpio_clear_bit ( PADDLE_P51 );

	for ( ;; ) {
	    thr_delay ( 500 );
	    bit = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;
	    stat = bit ? 'L' : '.';
	    printf ( "Read: %c\n", stat );
	}
}

void
diag_r ( void )
{
	int bit;
	char stat;

	gpio_in_up ( PADDLE_P51 );
	gpio_in_up ( PADDLE_P53 );

	gpio_out_init ( PADDLE_P52 );
	gpio_clear_bit ( PADDLE_P52 );

	for ( ;; ) {
	    thr_delay ( 500 );
	    bit = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;
	    stat = bit ? 'R' : '.';
	    printf ( "Read: %c\n", stat );
	}
}

void
diag ( void )
{
	int bit;
	char stat;


	gpio_out_init ( PADDLE_P51 );
	gpio_clear_bit ( PADDLE_P51 );

	gpio_out_init ( PADDLE_P52 );
	gpio_clear_bit ( PADDLE_P52 );

	gpio_in_up ( PADDLE_P53 );

	for ( ;; ) {
	    thr_delay ( 500 );
	    bit = gpio_read_bit ( PADDLE_P53 ) ? 0 : 1;
	    stat = bit ? '$' : '.';
	    printf ( "Read: %c\n", stat );
	}
}

/* Used this with a meter to ensure that these could
 * correctly assert both high and low.
 */
void
jam_gpios ( void )
{
	gpio_out_init ( PADDLE_P51 );
	gpio_out_init ( PADDLE_P52 );
	gpio_out_init ( PADDLE_P53 );

	// gpio_clear_bit ( PADDLE_P51 );
	// gpio_clear_bit ( PADDLE_P52 );
	// gpio_clear_bit ( PADDLE_P53 );

	gpio_set_bit ( PADDLE_P51 );
	gpio_set_bit ( PADDLE_P52 );
	gpio_set_bit ( PADDLE_P53 );

	for ( ;; ) ;
}

void
diag ( void )
{
	int info;

	gpio_in_up ( PADDLE_P51 );
	gpio_in_up ( PADDLE_P52 );
	gpio_in_up ( PADDLE_P53 );

	for ( ;; ) {
	    thr_delay ( 500 );
	    info = gpio_read ( 2 );
	    printf ( "Read: %08x\n", info );
	}
}
#endif

/* THE END */
