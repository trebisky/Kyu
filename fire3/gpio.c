/* gpio.c
 * Simple driver for the s5p6818 GPIO
 * Tom Trebisky 8-31-2018
 */

// typedef volatile unsigned int vu32;

#include <arch/types.h>

void blink_init ( void );
void blink_run ( void );

/* ----------------------------------------- */

struct gpio_regs {
	vu32 out;	/* 00 */
	vu32 oe;	/* 04 */
	vu32 dm0;	/* 08 */
	vu32 dm1;	/* 0c */
	vu32 ie;	/* 10 */
	vu32 xdet;	/* 14 */
	vu32 xpad;	/* 18 */
	int  __pad0;	/* 1c */
	vu32 alt0;	/* 20 */
	vu32 alt1;	/* 24 */
	vu32 dmx;	/* 28 */
	int  __pad1[4];
	vu32 dmen;	/* 3c */
	vu32 slew;	/* 40 */
	vu32 slewdis;	/* 44 */
	vu32 drv1;	/* 48 */
	vu32 drv1dis;	/* 4c */
	vu32 drv0;	/* 50 */
	vu32 drv0dis;	/* 54 */
	vu32 pull;	/* 58 */
	vu32 pulldis;	/* 5c */
	vu32 pullen;	/* 60 */
	vu32 pullendis;	/* 64 */
};

#define GPIOB_BASE	((struct gpio_regs *) 0xC001B000)

#define LED_BIT	(1<<12)

void
gpio_led_init ( void )
{
	struct gpio_regs *gp = GPIOB_BASE;

	/* Configure the GPIO */
	gp->alt0 &= ~(3<<24);
	gp->alt0 |= 2<<24;
	gp->oe = LED_BIT;

	// gp->out = LED_BIT;	/* off */
	gp->out = 0;		/* on */
}

void
gpio_init ( void )
{
}

void
status_on ( void )
{
	struct gpio_regs *gp = GPIOB_BASE;

	gp->out = 0;		/* on */
}

void
status_off ( void )
{
	struct gpio_regs *gp = GPIOB_BASE;

	gp->out = LED_BIT;	/* off */
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */

/*
 * These may have been OK with D cache disabled.
#define LONG  800000
#define MEDIUM  200000
#define SHORT 100000
*/

/* The above, times 100 seems about right */
#define LONG  80000000
#define MEDIUM  20000000
#define SHORT 10000000

void
delay ( int count )
{
	while ( count )
	    count--;
}

static void
pulse ( void )
{
	struct gpio_regs *gp = GPIOB_BASE;

	gp->out = LED_BIT;
	delay ( MEDIUM );
	gp->out = 0;
	delay ( SHORT );
	gp->out = LED_BIT;
}

static void
pulses ( int num )
{
	while ( num-- )
	    pulse ();
}

/* Called from test menu */
void
gpio_test ( void )
{
	printf ( "Use reset button to end test\n" );

	for ( ;; ) {
	    delay ( LONG );
	    pulses ( 2 );
	}
}

#ifdef notdef
/* This is an experiment to find out what EL the first
 * bit of 64 bit code runs at.
 * see u-boot: arch/arm/include/asm/system.h
 */

static inline unsigned int
get_el(void)
{
        unsigned int val;

        asm volatile("mrs %0, CurrentEL" : "=r" (val) : : "cc");
        return val >> 2;
}

void
blink_run (void)
{
	int count;

	count = get_el();

	for ( ;; ) {
	    delay ( LONG );

	    pulses ( count );
	}
}
#endif

/* THE END */
