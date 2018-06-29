/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * gpio.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  12-22-2016
 * Tom Trebisky  1/19/2017
 *
 * Driver for the H3 gpio
 * These are described in the datasheet in chapter 4-22 as "port controllers"
 */

#ifdef notdef
#define GPIO_A    0
#define GPIO_B    1	/* unpopulated */
#define GPIO_C    2
#define GPIO_D    3
#define GPIO_E    4
#define GPIO_F    5
#define GPIO_G    6

#define GPIO_H    7	/* unpopulated */
#define GPIO_I    8	/* unpopulated */

#define GPIO_J    9	/* R_PIO */
#endif

#include "gpio.h"

struct h3_gpio {
	volatile unsigned long config[4];
	volatile unsigned long data;
	volatile unsigned long drive[2];
	volatile unsigned long pull[2];
	long	_pad[119];
	volatile unsigned long int_config[4];	/* 0x200 */
	volatile unsigned long int_control;
	volatile unsigned long int_status;
	volatile unsigned long int_debounce;
};

/* In theory each gpio has 32 pins, but they are actually populated like so.
 */
// static int gpio_count[] = { 22, 0, 19, 18, 16, 7, 14, 0, 0, 12 };

static struct h3_gpio * gpio_base[] = {
    (struct h3_gpio *) 0x01C20800,		/* GPIO_A */
    (struct h3_gpio *) 0x01C20824,		/* GPIO_B */
    (struct h3_gpio *) 0x01C20848,		/* GPIO_C */
    (struct h3_gpio *) 0x01C2086C,		/* GPIO_D */
    (struct h3_gpio *) 0x01C20890,		/* GPIO_E */
    (struct h3_gpio *) 0x01C208B4,		/* GPIO_F */
    (struct h3_gpio *) 0x01C208D8,		/* GPIO_G */

    (struct h3_gpio *) 0x01C208FC,		/* GPIO_H */
    (struct h3_gpio *) 0x01C20920,		/* GPIO_I */

    (struct h3_gpio *) 0x01F02c00,		/* GPIO_J (R_PIO) */
};

/* Only A, G, and J (J = R_PIO) can interrupt */

/* GPIO pin function config (0-7) */
#define GPIO_INPUT        (0)
#define GPIO_OUTPUT       (1)
#define GPIO_F2           (2)
#define GPIO_F3           (3)
#define GPIO_EINT         (6)	/* only for A, G, and J */
#define GPIO_DISABLE      (7)

/* GPIO pin pull-up/down config (0-3)*/
#define GPIO_PULL_DISABLE	(0)
#define GPIO_PULL_UP		(1)
#define GPIO_PULL_DOWN		(2)
#define GPIO_PULL_RESERVED	(3)

/* As near as I can tell only PIO A, E, and J can generate interrupts */

/* There are 4 config registers,
 * each with 8 fields of 4 bits.
 */
static void
gpio_config ( int bit, int val )
{
	int gpio = bit / 32;
	int pin = bit % 32;

	struct h3_gpio *gp;
	int reg, shift, tmp;

	gp = gpio_base[gpio];
	reg = pin / 8;
	shift = (pin & 0x7) * 4;

	tmp = gp->config[reg] & ~(0xf << shift);
	gp->config[reg] = tmp | (val << shift);
}

/* There are two pull registers,
 * each with 16 fields of 2 bits.
 */
static void
gpio_pull ( int bit, int val )
{
	int gpio = bit / 32;
	int pin = bit % 32;

	struct h3_gpio *gp;
	int reg, shift, tmp;

	gp = gpio_base[gpio];
	reg = pin / 16;
	shift = (pin & 0xf) * 2;

	tmp = gp->pull[reg] & ~(0x3 << shift);
	gp->pull[reg] = tmp | (val << shift);
}

static void
gpio_output ( int gpio, int pin, int val )
{
	struct h3_gpio *gp = gpio_base[gpio];

	if ( val )
	    gp->data |= 1 << pin;
	else
	    gp->data &= ~(1 << pin);
}

static int
gpio_input ( int gpio, int pin )
{
	struct h3_gpio *gp = gpio_base[gpio];

	return (gp->data >> pin) & 1;
}

/* ----------------------------------------------------------- */
/* ----------------------------------------------------------- */

/* Interrupt modes */
#define INT_POSEDGE	0
#define INT_NEGEDGE	1
#define INT_HIGH	2
#define INT_LOW		3
#define INT_DBLEDGE	4

/* Bits in debounce register */
#define GPIO_CLOCK_24K		0
#define GPIO_CLOCK_32M		1
#define GPIO_PRESCALE_SHIFT	4
/* Prescaler is 3 bits (i.e. values from 0-7) */

/* Interrupt support.  The Config registers hold 8 4-bit
 * mode fields per register.  4 bits could hold 16 choices,
 * but only 5 are defined.
 */
void
gpio_int_mode ( int bit, int mode )
{
	int gpio = bit / 32;
	int pin = bit % 32;

	struct h3_gpio *gp;
	int reg, shift, tmp;

	gpio_config ( bit, GPIO_EINT );

	gp = gpio_base[gpio];
	reg = pin / 8;
	shift = (pin & 0x7) * 4;

	tmp = gp->int_config[reg] & ~(0xf << shift);
	gp->int_config[reg] = tmp | (mode << shift);
}

void
gpio_int_enable ( int bit )
{
	int gpio = bit / 32;
	int pin = bit % 32;

	struct h3_gpio *gp;
	gp = gpio_base[gpio];

	gp->int_control |= 1<<pin;
}

void
gpio_int_ack ( int bit )
{
	int gpio = bit / 32;
	int pin = bit % 32;

	struct h3_gpio *gp;
	gp = gpio_base[gpio];

	gp->int_status |= 1<<pin;
}


/* ----------------------------------------------------------- */
/* ----------------------------------------------------------- */
/* We only support the H3 for now,
 *   the code for the H5 is parked here for a rainy day.
 */
#define CHIP_H3

void
uart_gpio_init ( int uart )
{
#ifdef CHIP_H3
	if ( uart == 0 ) {
	    gpio_config ( GPIO_A_4, GPIO_F2 );
	    gpio_config ( GPIO_A_5, GPIO_F2 );
	    gpio_pull ( GPIO_A_5, GPIO_PULL_UP );
	} else if ( uart == 1 ) {
	    gpio_config ( GPIO_G_6, GPIO_F2 );
	    gpio_config ( GPIO_G_7, GPIO_F2 );
	    gpio_pull ( GPIO_G_7, GPIO_PULL_UP );
	} else if ( uart == 2 ) {
	    gpio_config ( GPIO_A_0, GPIO_F2 );
	    gpio_config ( GPIO_A_1, GPIO_F2 );
	    gpio_pull ( GPIO_A_1, GPIO_PULL_UP );
	} else if ( uart == 3 ) {
	    gpio_config ( GPIO_A_13, GPIO_F3 );
	    gpio_config ( GPIO_A_14, GPIO_F3 );
	    gpio_pull ( GPIO_A_14, GPIO_PULL_UP );
	}

#else	/* H5 */
	gpio_config ( GPIO_A_4, GPIO_F2 );
	gpio_config ( GPIO_A_5, GPIO_F2 );
	gpio_pull ( GPIO_A_5, GPIO_PULL_UP );
#endif
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
/* These are "standard" API routines that
 * conform to what was done for the BBB gpio.c
 */

void
gpio_clear_bit ( int bit )
{
	gpio_output ( bit/32, bit%32, 0 );
}

void
gpio_set_bit ( int bit )
{
	gpio_output ( bit/32, bit%32, 1 );
}

int
gpio_read_bit ( int bit )
{
	return gpio_input ( bit/32, bit%32 );
}

void
gpio_out_init ( int bit )
{
	gpio_config ( bit, GPIO_OUTPUT );
}

void
gpio_in_init ( int bit )
{
	gpio_config ( bit, GPIO_INPUT );
}

/* These are used by the i2c driver to flip the line around.
 * On the BBB we had an output enable control to do this.
 * On the Orange Pi, they are identical to the in/out_init
 * routines above, but not yet tested.
 */
void
gpio_dir_out ( int bit )
{
	gpio_config ( bit, GPIO_OUTPUT );
}

void
gpio_dir_in ( int bit )
{
	gpio_config ( bit, GPIO_INPUT );
}


/* -------------------------------------------------- */
/* -------------------------------------------------- */
/* LED support */

/* Called from board.c */
void
gpio_led_init ( void )
{
	gpio_out_init ( POWER_LED );
	gpio_out_init ( STATUS_LED );
}

/* Called from board.c */
/* not implemented for Orange Pi */
/* well, it is now .... */
void gpio_init ( void )
{
	struct h3_gpio *gp = (struct h3_gpio *) 0;

	printf ( "GPIO INT at %08x\n", &gp->int_config[0] );
}
/* This is the green LED */
void
pwr_on ( void )
{
	// gpio_output ( GPIO_J, POWER_PIN, 1 );
	gpio_set_bit ( POWER_LED );
}

void
pwr_off ( void )
{
	// gpio_output ( GPIO_J, POWER_PIN, 0 );
	gpio_set_bit ( POWER_LED );
}

/* This is the red LED */
void
status_on ( void )
{
	// gpio_output ( GPIO_A, STATUS_PIN, 1 );
#ifdef BOARD_NANOPI_NEO
	gpio_set_bit ( STATUS_LED_NEO );
#else
	gpio_set_bit ( STATUS_LED );
#endif
}

void
status_off ( void )
{
	// gpio_output ( GPIO_A, STATUS_PIN, 0 );
#ifdef BOARD_NANOPI_NEO
	gpio_clear_bit ( STATUS_LED_NEO );
#else
	gpio_clear_bit ( STATUS_LED );
#endif
}

/* A reasonable delay for blinking an LED
 * (at least it is if the D cache is enabled)
 */
static void
__delay_blink ( void )
{
        // volatile int count = 50000000;
        volatile int count = 500000;

	//printf ( "Start delay\n" );
        while ( count-- )
            ;
	//printf ( "End delay\n" );
}

/* Blink red status light */
void
gpio_blink_red ( void )
{
        for ( ;; ) {
	    //printf ( "Red on\n" );
            status_on ();
            __delay_blink ();

	    // printf ( "Red off\n" );
            status_off ();
            __delay_blink ();
        }
}

/* Blink green "power" light */
void
gpio_blink_green ( void )
{
        for ( ;; ) {
            pwr_on ();
            __delay_blink ();

            pwr_off ();
            __delay_blink ();
        }
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include "h3_ints.h"

void
button_handler ( int xx )
{
	printf ( "Button pushed !!\n" );
	gpio_int_ack ( POWER_BUTTON );

	thr_show ();
	tcp_show ();
}

/* Should not hurt to call this twice.
 * Now called on every startup, so test is
 * not needed.
 */
void
gpio_button_enable ( void )
{
	irq_hookup ( IRQ_PIO_J, button_handler, 0 );

	gpio_int_mode ( POWER_BUTTON, INT_NEGEDGE );
	gpio_int_enable ( POWER_BUTTON );
}

/* Called from test menu.
 * Button is normally 1, 0 when pushed.
 * Works fine, enables the interrupt and returns.
 * 6-26-2018
 * Conflicts with button interrupt.
 */
void
gpio_test_button ( void )
{
#ifdef notdef
	int val;
	gpio_in_init ( POWER_BUTTON );

	for ( ;; ) {
	    val = gpio_read_bit ( POWER_BUTTON );
	    printf ( "Button: %08x\n", val );
	    thr_delay ( 250 );
	}
#endif
	printf ( "Already enabled, try pushing it\n" );
	//gpio_button_enable ();
}

#define TEST3_DELAY	100

/* Test 2 - 
 *
 * Simple light blinking - external LED
 *  use gpio routines and timer interrupts
 */
void
gpio_test2 ( void )
{
	gpio_out_init ( GPIO_A_6 );

        for ( ;; ) {
            gpio_clear_bit ( GPIO_A_6 );
            thr_delay ( TEST3_DELAY );
            gpio_set_bit ( GPIO_A_6 );
            thr_delay ( TEST3_DELAY );
        }
}

/* Test 3 - from bbb/gpio.c
 *
 * Simple light blinking
 *  use gpio routines and timer interrupts
 */
void
gpio_test3 ( void )
{
        for ( ;; ) {
            gpio_clear_bit ( STATUS_LED );
            thr_delay ( TEST3_DELAY );
            gpio_set_bit ( STATUS_LED );
            thr_delay ( TEST3_DELAY );
        }
}

/* Called from tests.c */
void
gpio_test ( void )
{
	// gpio_test3 ();
	gpio_test2 ();
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */


/* XXX XXX this CCM stuff doesn't belong here,
 *  but here it is for now.
 */

/* These are registers in the CCM (clock control module)
 */
#define CCM_GATE	((unsigned long *) 0x01c2006c)
#define CCM_RESET4	((unsigned long *) 0x01c202d8)

#define GATE_UART0	0x00010000
#define GATE_UART1	0x00020000
#define GATE_UART2	0x00040000
#define GATE_UART3	0x00080000

#define RESET4_UART0	0x00010000
#define RESET4_UART1	0x00020000
#define RESET4_UART2	0x00040000
#define RESET4_UART3	0x00080000

/* This is probably set up for us by U-boot,
 * true bare metal would need this.
 */
void
uart_clock_init ( int uart )
{
	if ( uart == 0 ) {
	    *CCM_GATE |= GATE_UART0;
	    *CCM_RESET4 |= RESET4_UART0;
	} else if ( uart == 1 ) {
	    *CCM_GATE |= GATE_UART1;
	    *CCM_RESET4 |= RESET4_UART1;
	} else if ( uart == 2 ) {
	    *CCM_GATE |= GATE_UART2;
	    *CCM_RESET4 |= RESET4_UART2;
	} else if ( uart == 3 ) {
	    *CCM_GATE |= GATE_UART3;
	    *CCM_RESET4 |= RESET4_UART3;
	}
}

/* THE END */
