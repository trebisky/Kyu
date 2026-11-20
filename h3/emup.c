/*
 * Copyright (C) 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* emup.c
 *
 * Tom Trebisky  Kyu project  11-13-2026
 *   Happy Birthday Ingrid!
 *
 */
#include "kyu.h"
#include "gpio.h"

/* The schematic uses this pins, so we want to verify
 * that there are no issues writing to them.
 */
static int all_bits[] = {
	GPIO_A_6,
	GPIO_A_7, GPIO_A_8, GPIO_A_9, GPIO_A_10,
	GPIO_A_11, GPIO_A_12, GPIO_A_13, GPIO_A_14,
	GPIO_C_0, GPIO_C_1, GPIO_C_2, GPIO_C_3, GPIO_C_4
};

/* Here D0 pertains to the labeling on the ISA bus card.
 */
#define EMUP_D0	GPIO_A_7
#define EMUP_D1	GPIO_A_8
#define EMUP_D2	GPIO_A_9
#define EMUP_D3	GPIO_A_10
#define EMUP_D4	GPIO_A_11
#define EMUP_D5	GPIO_A_12
#define EMUP_D6	GPIO_A_13
#define EMUP_D7	GPIO_A_14

static int data_bits[] = {
	EMUP_D0, EMUP_D1, EMUP_D2, EMUP_D3,
	EMUP_D4, EMUP_D5, EMUP_D6, EMUP_D7
};

/* enable line for the 245 chip */
#define EMUP_ENA	GPIO_A_6

#define EMUP_SEL	GPIO_C_0
#define EMUP_DATA	GPIO_C_1
#define EMUP_READ	GPIO_C_2

#define EMUP_DIR	GPIO_C_3
#define EMUP_RESET	GPIO_C_4

/* ==========================================
 * Diagnostics
 */

static void
flip ( int nbits )
{
		int i;

		for ( i=0; i<nbits; i++ ) {
			gpio_set_bit ( all_bits[i] );
		}
		for ( i=0; i<nbits; i++ ) {
			gpio_clear_bit ( all_bits[i] );
		}
}

/* This was used before sending out the PCB to verify
 * that all the GPIO ports could be manipulated.
 *
 * Typically this hogs the CPU which eventually leads to:
 * PANIC: netbuf_alloc_i
 *
 * no surprise.  The network driver is constantly receiving
 * packets and expects them to get processed.
 */
static void
emup_scope1 ( void )
{
		int nbits = sizeof(all_bits) / sizeof(int);
		int i;
		int nn = 0;

		printf ( "%d emup bits\n", nbits );

		/* Configure for output */
		for ( i=0; i<nbits; i++ )
			gpio_out_init ( all_bits[i] );

		/* Scope loop */
		/* The printf yields a delay so that we
		 * see fast pulses every millisecond */
		for ( ; ; ) {
			// thr_delay ( 2 );
			printf ("%d\n", nn++ );
			flip ( nbits );
		}
}

static void
emup_scope2 ( void )
{
#define TEST EMUP_ENA
// #define TEST EMUP_DIR
		gpio_out_init ( TEST );

		printf ( "EMUP scope loop on ENA\n" );
		/* yields 581 Hz */
		for ( ; ; ) {
			gpio_set_bit ( TEST );
			delay_us ( 10 );
			gpio_clear_bit ( TEST );
			delay_us ( 100 );
		}
}

static void
emup_test1 ( void )
{
		// printf ( "EMUP enable READ\n" );
		// gpio_set_bit ( EMUP_READ );

		// printf ( "EMUP enable SEL\n" );
		// gpio_set_bit ( EMUP_SEL );

		// printf ( "EMUP enable DATA\n" );
		// gpio_set_bit ( EMUP_DATA );
}

/* ==========================================
 * Basic IO
 */

static void
read_setup ( void )
{
		int i;

		for ( i=0; i<8; i++ )
			gpio_in_init ( data_bits[i] );
}

static void
write_setup ( void )
{
		int i;

		for ( i=0; i<8; i++ )
			gpio_out_init ( data_bits[i] );
}

static void
emup_hw_init ( void )
{
		/* Make all 3 inactive */
		gpio_clear_bit ( EMUP_SEL );
		gpio_clear_bit ( EMUP_DATA );
		gpio_clear_bit ( EMUP_READ );

		gpio_out_init ( EMUP_SEL );
		gpio_out_init ( EMUP_DATA );
		gpio_out_init ( EMUP_READ );

		/* release reset */
		gpio_set_bit ( EMUP_RESET );
		gpio_out_init ( EMUP_RESET );

		read_setup ();

		/* DIR as if reading
		 * High (set) is writing.
		 */
		gpio_clear_bit ( EMUP_DIR );
		gpio_out_init ( EMUP_DIR );

		/* Enable the 245 */
		gpio_clear_bit ( EMUP_ENA );
		gpio_out_init ( EMUP_ENA );
}

static void
set_data ( int val )
{
		int i;

		/* lsb first */
		for ( i=0; i<8; i++ ) {
			if ( val & 1 )
				gpio_set_bit ( data_bits[i] );
			else
				gpio_clear_bit ( data_bits[i] );
			val >>= 1;
		}
}

static void
post_data ( int val )
{
		set_data ( val );
		write_setup ();
		set_data ( val );
}

static int
fetch_data ( void )
{
		int i;
		int rv;
		int bit;

		rv = 0;
		for ( i=0; i<8; i++ ) {
			bit = gpio_read_bit ( data_bits[i] );
			rv = rv<<1 | bit;
		}
		return rv;
}

static void
emup_sel ( int val )
{
		post_data ( val );

		gpio_set_bit ( EMUP_DIR );

		gpio_set_bit ( EMUP_SEL );
		gpio_clear_bit ( EMUP_SEL );

		gpio_clear_bit ( EMUP_DIR );
		read_setup ();
}

static void
emup_send ( int val )
{
		post_data ( val );

		gpio_set_bit ( EMUP_DIR );

		gpio_set_bit ( EMUP_DATA );
		gpio_clear_bit ( EMUP_DATA );

		gpio_clear_bit ( EMUP_DIR );
		read_setup ();
}

static int
emup_recv ( void )
{
		int rv;

		/* DIR is always ready to receive in
		 * out current protocol.
		 * Also the 8 gpio for data are also
		 * kept ready to read.
		 */

		gpio_set_bit ( EMUP_READ );

		rv = fetch_data ();

		gpio_clear_bit ( EMUP_READ );
		return rv;
}

static int
emup_read ( int reg )
{
		emup_sel ( reg );
		return emup_recv ();
}

static void
emup_write ( int reg, int val )
{
		emup_sel ( reg );
		emup_send ( val );
}

// ====================================================

#define SEL_VOP_DAC		0xe5
#define SEL_VHH_DAC		0xe6
#define SEL_VCC_DAC		0xe7

#define SEL_VCC_EN0		0xed
#define SEL_VCC_EN1		0xee

#define SEL_OTHER		0xef

#define SEL_MYSTERY		0xf7

/* bits in VCC control regs */

#define VCC0_PIN40		0x01
#define VCC0_PIN36		0x02
#define VCC0_PIN34		0x04
#define VCC0_PIN32		0x08
#define VCC0_PIN31		0x10
#define VCC0_PIN30		0x20
#define VCC0_PIN29		0x40
#define VCC0_PIN28		0x80

#define VCC1_PIN27		0x01
#define VCC1_PIN26		0x02
#define VCC1_PIN9		0x04
#define VCC1_PIN7		0x08
#define VCC1_PIN5		0x10
#define VCC1_PIN1		0x20
/* upper 2 bits unused */

#define OTHER_11_30		0x01	/* pins 11, 30 are ground (low is 20) */
#define OTHER_VOP_7		0x02	/* VOP to pin 7 */
#define OTHER_VOP_25	0x04	/* VOP to pin 25 */
#define OTHER_OSC_2		0x08	/* Oscillator to pins 2,3 */
#define OTHER_OSC_18	0x10	/* Oscillator to pins 18,19 */
#define OTHER_OSC_HI	0x20	/* Oscillator 4.432 (low is 2.216 Mhz) */
/* upper 2 bits unused */

/* .04 volts per unit.
 * Call with 500 to get 5 volts.
 * This yields 125 (0x7d), then we add 15
 *  to get 140 (0x8C)
 *
 * I see 4.73 volts on my original EMUP unit.
 */
static void
set_vcc_dac ( int volts )
{
		int val = volts / 4;
		val += 15;
		if ( val > 0xff )
			val = 0xff;
		// printf ( "VCC dac value: %x\n", val );
		emup_write ( SEL_VCC_DAC, val );
}

// ====================================================

/* This is now IO test 21, so type "i 21" to run this.
 */

static void
reg_show ( int reg )
{
		int val;

		val = emup_read ( reg );
		printf ( "Reg %02x:  %02x\n", reg, val );
}

static void
emup_test2 ( void )
{
		reg_show ( 0xe0 );
		reg_show ( 0xe1 );
		reg_show ( 0xe2 );
		reg_show ( 0xe3 );
		reg_show ( 0xe4 );
		reg_show ( 0xe5 );
		reg_show ( 0xe6 );
		reg_show ( 0xe7 );

		reg_show ( 0xf8 );
}

static void
dac_scope ( void )
{
		printf ( "start DAC loop\n" );
		for ( ;; )
			set_vcc_dac ( 500 );
}

/* I see a 5 us high pulse
 *  every 75 us
 */
static void
sel_scope ( void )
{
		for ( ;; )
			emup_sel ( SEL_VCC_DAC );
}

static void
sel2_scope ( void )
{
		int val = SEL_VCC_DAC;

		printf ( "Set data: %02x\n", val );

#ifdef notdef
		/* I discovered that setting data, then doing the
		 * write setup yielded all ones on the output.
		 * following with setting the data again works!
		 * This doesn't makes sense, but we will run with
		 *  what works
		 */
		set_data ( val );
		write_setup ();
		set_data ( val );
#endif
		post_data ( val );

		gpio_set_bit ( EMUP_DIR );

		printf ( "start sel2 loop\n" );
		for ( ;; ) {
			gpio_set_bit ( EMUP_SEL );
			gpio_set_bit ( EMUP_SEL );
			gpio_set_bit ( EMUP_SEL );
			gpio_set_bit ( EMUP_SEL );

			gpio_clear_bit ( EMUP_SEL );
			gpio_clear_bit ( EMUP_SEL );
			gpio_clear_bit ( EMUP_SEL );
			gpio_clear_bit ( EMUP_SEL );
		}
}

static void
data_scope ( void )
{
		write_setup ();
		gpio_set_bit ( EMUP_DIR );

		for ( ;; ) {
			set_data ( 0x00 );
			set_data ( 0xAA );
		}
}

static void
emup_user ( void )
{
		char buf[64];
		char *wp[10];
		int nw;

		emup_write ( SEL_OTHER, 0 );	/* be sure */

		for ( ;; ) {
			printf ( "Emup> " );
			getline ( buf, 64 );
			if ( ! buf[0] )
				continue;
			nw = split ( buf, wp, 10 );
			if ( **wp == 'q' )
				break;
			if ( **wp == 'd' ) {
				printf ( "Turn on Vcc\n" );
				emup_write ( SEL_VCC_EN0, VCC0_PIN40 );
				set_vcc_dac ( 500 );
			}
			if ( **wp == 'c' ) {
				printf ( "Turn off Vcc\n" );
				set_vcc_dac ( 0 );
			}
		}

		set_vcc_dac ( 0 );
		emup_write ( SEL_VCC_EN0, 0 );
		printf ( "Done\n" );
}

/* Called from IO test menu, currently via "i 21"
 */
void
emup_test ( void *xxx )
{

#ifdef notdef
		printf ( "Turn on Vcc\n" );

		emup_write ( SEL_OTHER, 0 );	/* be sure */
		set_vcc_dac ( 500 );
		emup_write ( SEL_VCC_EN0, VCC0_PIN40 );

		printf ( "Done\n" );

		printf ( "start scope loop\n" );
		dac_scope ();
		// sel2_scope ();
		// data_scope ();

		emup_test2 ();


		emup_write ( SEL_MYSTERY, 0x8 );

#endif
}

/* Called from Kyu user.c in lieu of kyu shell.
 * See WANT_EMUP in kyu.h
 * If emup_user() returns the normal shell will launch.
 */
void
emup_init ( void )
{
		emup_hw_init ();

		emup_user ();
}

/* THE END */
