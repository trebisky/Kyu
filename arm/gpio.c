/* gpio.c
 *
 * Simple driver for the GPIO
 * 
 */

#define GPIO0_BASE      ( (struct gpio *) 0x44E07000 )
#define GPIO1_BASE      ( (struct gpio *) 0x4804C000 )
#define GPIO2_BASE      ( (struct gpio *) 0x48A1C000 )
#define GPIO3_BASE      ( (struct gpio *) 0x48A1E000 )

#include <omap_gpio.h>

struct gpio {
	volatile unsigned long id;
	long _pad0[3];
	volatile unsigned long config;
	long _pad1[3];
	volatile unsigned long eoi;

	volatile unsigned long irqs0r;
	volatile unsigned long irqs1r;
	volatile unsigned long irqs0;
	volatile unsigned long irqs1;

	volatile unsigned long irqset0;
	volatile unsigned long irqset1;
	volatile unsigned long irqclear0;
	volatile unsigned long irqclear1;

	volatile unsigned long waken0;
	volatile unsigned long waken1;
	long _pad2[50];
	volatile unsigned long status;
	long _pad3[6];
	volatile unsigned long control;
	volatile unsigned long oe;
	volatile unsigned long datain;
	volatile unsigned long dataout;
	volatile unsigned long level0;
	volatile unsigned long level1;
	volatile unsigned long rising;
	volatile unsigned long falling;
	volatile unsigned long deb_ena;
	volatile unsigned long deb_time;
	long _pad4[14];
	volatile unsigned long clear_data;
	volatile unsigned long set_data;
};

static struct gpio *gpio_bases[] = {
    GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE
};

#define LED0	1<<21
#define LED1	1<<22
#define LED2	1<<23
#define LED3	1<<24

#define ALL_LED (LED0 | LED1 | LED2 | LED3)

void
gpio_led_init ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	setup_led_mux ();

	/* Setting the direction is essential */
	p->oe &= ~ALL_LED;
	/*
	printf ( "gpio1 OE = %08x\n", p->oe );
	*/
	p->clear_data = ALL_LED;
}

void
gpio_set_bit ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->set_data = 1 << (bit % 32);
}

void
gpio_clear_bit ( int bit )
{
	int gpio = bit / 32;
	gpio_bases[gpio]->clear_data = 1 << (bit % 32);
}

/* --------------------- */

int led_bits[] = { LED0, LED1, LED2, LED3 };

void
gpio_led_set ( int val )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;

	for ( i=0; i<4; i++ ) {
	    if ( val & 0x01 )
		p->set_data = led_bits[i];
	    else
		p->clear_data = led_bits[i];
		val >>= 1;
	}
}

void
gpio_init ( void )
{
	gpio_led_init ();
}


int pork[] = { LED0, LED1, LED2, LED3 };

/* Cute light show */
void
gpio_test ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;
	int n;

	/*
	struct gpio *p = (struct gpio *) 0;
	printf ( "waken1 at %08x\n", &p->waken1 );
	printf ( "status at %08x\n", &p->status );
	printf ( "set_data at %08x\n", &p->set_data );
	*/

	p->clear_data = ALL_LED;
	i=0;
	p->set_data = pork[i];
	n = 1;

	for ( ;; ) {
	    if ( i == 0 )
		printf ( "GPIO test, pass %d\n", n++ );
	    p->clear_data = pork[i];
	    i = (i+1) % 4;
	    p->set_data = pork[i];
	    delay10 ();
	}
}

/* Cute light show.
 *  use timer interrupts
 */
void
gpio_test2 ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;
	int i;
	int n;

	/*
	struct gpio *p = (struct gpio *) 0;
	printf ( "waken1 at %08x\n", &p->waken1 );
	printf ( "status at %08x\n", &p->status );
	printf ( "set_data at %08x\n", &p->set_data );
	*/

	p->clear_data = ALL_LED;
	i=0;
	p->set_data = pork[i];
	n = 1;

	for ( ;; ) {
	    if ( i == 0 )
		printf ( "GPIO test, pass %d\n", n++ );
	    p->clear_data = pork[i];
	    i = (i+1) % 4;
	    p->set_data = pork[i];
	    thr_delay ( 5 );
	}
}

int users[] = { USER_LED0, USER_LED1, USER_LED2, USER_LED3 };

/* Simple light blinking
 *  use timer interrupts
 */
void
gpio_test3 ( void )
{
	int who = 0;

	gpio_clear_bit ( GPIO_0_2 );

	for ( ;; ) {
	    gpio_clear_bit ( users[who] );
	    who = (who+1) % 4;
	    gpio_set_bit ( users[who] );
	    thr_delay ( 3 );
	}
}

/* Cute light show.
 *  hooks for test menu
 */

static int gpio_test_count;

void
gpio_test_init ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	p->clear_data = ALL_LED;

	p->set_data = pork[0];
	gpio_test_count = 0;
}

void
gpio_test_run ( void )
{
	struct gpio *p = (struct gpio *) GPIO1_BASE;

	p->clear_data = pork[gpio_test_count];
	gpio_test_count = (gpio_test_count+1) % 4;
	p->set_data = pork[gpio_test_count];
}

/* THE END */
