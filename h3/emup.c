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

/* ================================================================== */
/* The following is specific for my IO interface.
 */

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

static void		emup_write ( int, int );
static int		emup_read ( int );

static void		set_vcc_dac ( int );
static void		set_vhh_dac ( int );
static void		set_vop_dac ( int );

/* ================================================================== */
/* The following is generic for the EMUP
 */

#define SEL_TTL0		0xe0
#define SEL_TTL1		0xe1
#define SEL_TTL2		0xe2
#define SEL_TTL3		0xe3
#define SEL_TTL4		0xe4

#define SEL_VOP_DAC		0xe5
#define SEL_VHH_DAC		0xe6
#define SEL_VCC_DAC		0xe7

#define SEL_VCC_EN0		0xed
#define SEL_VCC_EN1		0xee

#define SEL_OTHER		0xef

#define SEL_MYSTERY		0xf7
#define SEL_SECRET		0xf8

/* bits in VCC control regs */
/* These are pin numbers on the 40 pin socket */

#define VCC0_PIN40		0x01
#define VCC0_PIN36		0x02
#define VCC0_PIN34		0x04	// for 28 pin device
#define VCC0_PIN32		0x08	// for 24 pin device
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

/* just in case, we set the bit values both before
 * and after configuring them as outputs.
 * This is probably unnecessary.
 * It is necessary for the 8 data bits.
 */
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

		/* belt and suspenders
		gpio_clear_bit ( EMUP_SEL );
		gpio_clear_bit ( EMUP_DATA );
		gpio_clear_bit ( EMUP_READ );

		/* release reset */
		gpio_set_bit ( EMUP_RESET );
		gpio_out_init ( EMUP_RESET );
		gpio_set_bit ( EMUP_RESET );

		read_setup ();

		/* DIR as if reading
		 * High (set) is writing.
		 */
		gpio_clear_bit ( EMUP_DIR );
		gpio_out_init ( EMUP_DIR );
		gpio_clear_bit ( EMUP_DIR );

		/* Enable the 245 */
		gpio_clear_bit ( EMUP_ENA );
		gpio_out_init ( EMUP_ENA );
		gpio_clear_bit ( EMUP_ENA );
}

static void
emup_io_init ( void )
{
		set_vcc_dac ( 0 );
		set_vhh_dac ( 0 );
		set_vop_dac ( 0 );

		emup_write ( SEL_OTHER, 0 );

		emup_write ( SEL_TTL0, 0 );
		emup_write ( SEL_TTL1, 0 );
		emup_write ( SEL_TTL2, 0 );
		emup_write ( SEL_TTL3, 0 );
		emup_write ( SEL_TTL4, 0 );

		emup_write ( SEL_VCC_EN0, 0 );
		emup_write ( SEL_VCC_EN1, 0 );

		/* should initialize everything */
		// XXX
}

/* Don't call this directly, call post_data()
 */
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

/* For some reason, setting the data first, then switching
 * the ports to outputs yielded all ones.
 * This works.
 * We don't need to play this game for the control lines,
 * but they always remain setup as outputs.
 */
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

/* All the dac routines get called with volts * 100
 * as the argument.
 */

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

/* 15.3 full range with .06 resolution
 * Call with 1530 to get 15.3
 */
static void
set_vhh_dac ( int volts )
{
		int val = volts / 6;
		val += 15;
		if ( val > 0xff )
			val = 0xff;
		// printf ( "VHH dac value: %x\n", val );
		emup_write ( SEL_VHH_DAC, val );
}

/* 25.5 full range with .1 resolution
 * Call with 2550 to get 25.5
 */
static void
set_vop_dac ( int volts )
{
		int val = volts / 10;
		val += 15;
		if ( val > 0xff )
			val = 0xff;
		// printf ( "VOP dac value: %x\n", val );
		emup_write ( SEL_VOP_DAC, val );
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

/* This needs to be generalized to all
 * TTL registers. XXX
 */
static int ttl3_val;

static void ttl3_set ( int mask, int val )
{
		ttl3_val &= ~mask;
		ttl3_val |= val;
		emup_write ( SEL_TTL3, val );
}


static void
assert_OE ( void )
{
		ttl3_set ( 0x08, 0 );
}

static void
deassert_OE ( void )
{
		ttl3_set ( 0x08, 1 );
}

static void
assert_CE ( void )
{
		ttl3_set ( 0x02, 0 );
}

static void
deassert_CE ( void )
{
		ttl3_set ( 0x02, 1 );
}

/* Read byte from 28 pin EPROM */
static int
read28 ( int addr )
{
		int val;
		int val2, val3;
		int cval;

		emup_write ( SEL_TTL0, 0 );
		emup_write ( SEL_TTL1, 0 );
		emup_write ( SEL_TTL2, 0 );
		emup_write ( SEL_TTL3, 0 );
		emup_write ( SEL_TTL4, 0 );
		ttl3_val = 0;

		emup_write ( SEL_TTL0, 0x40 );	// Vpp high

		/* Set address */
		val = 0;
		if ( addr & 1 )
			val |= 0x80;	// A0
		if ( addr & 2 )
			val |= 0x40;	// A1
		if ( addr & 4 )
			val |= 0x20;	// A2
		if ( addr & 8 )
			val |= 0x10;	// A3
		if ( addr & 0x10 )
			val |= 0x08;	// A4
		if ( addr & 0x20 )
			val |= 0x04;	// A5
		if ( addr & 0x40 )
			val |= 0x02;	// A6
		if ( addr & 0x80 )
			val |= 0x01;	// A7

		emup_write ( SEL_TTL1, val );

		deassert_CE ();
		deassert_OE ();

		/* Turn on power */
		emup_write ( SEL_VCC_EN0, VCC0_PIN34 );
		set_vcc_dac ( 500 );

		// To read, you must write 1 first
		// to all bits you intend to read from
		val = 0xff;
		val &= ~0x8;	// skip ground
		emup_write ( SEL_TTL2, val );	// D0 .. D6
		// emup_write ( SEL_TTL3, 0x1 );	// D7
		ttl3_set ( 0x1, 0x1 );

		// not needed, same results without it
		// thr_delay ( 10 );

		assert_CE ();
		assert_OE ();

		val2 = emup_read ( SEL_TTL2 );
		val3 = emup_read ( SEL_TTL3 );

		val = (val2 & 0xf0) >> 1;
		val |= val2 &0x7;
		if ( val3 & 0x1 )
			val |= 0x80;

		cval = val;
		if ( val < ' ' )
			cval = '-';
		if ( val > '~' )
			cval = '-';

		printf ( "Read at %04x: %02x %c  (%02x %02x)\n", addr, val, cval, val2, val3 );

		set_vcc_dac ( 0 );
		emup_write ( SEL_VCC_EN0, 0 );

		emup_write ( SEL_TTL0, 0 );
		emup_write ( SEL_TTL1, 0 );
		emup_write ( SEL_TTL2, 0 );
		emup_write ( SEL_TTL3, 0 );
		emup_write ( SEL_TTL4, 0 );

		return 0;
}

/* There are other bits in the Mystery port that we
 * don't yet understand.
 */
static void
emup_good_on ( void )
{
		emup_write ( SEL_MYSTERY, 0x8 );
}

static void
emup_good_off ( void )
{
		emup_write ( SEL_MYSTERY, 0 );
}

static void
emup_user ( void )
{
		char buf[64];
		char *wp[10];
		int nw;
		int val;
		int addr;
		int i;

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
				// emup_write ( SEL_VCC_EN0, VCC0_PIN40 );
				emup_write ( SEL_VCC_EN0, VCC0_PIN34 );
				set_vcc_dac ( 500 );
			}
			if ( **wp == 'c' ) {
				printf ( "Turn off Vcc\n" );
				set_vcc_dac ( 0 );
			}
			if ( **wp == 'l' ) {
				printf ( "Turn on GOOD\n" );
				emup_good_on ();
				// emup_write ( SEL_MYSTERY, 0x8 );
			}
			if ( **wp == 'm' ) {
				printf ( "Turn off GOOD\n" );
				emup_good_off ();
				// emup_write ( SEL_MYSTERY, 0x0 );
			}
			if ( **wp == 's' ) {
				printf ( "Monitor secret port\n" );
				// Always reads as 0xff
				for ( ;; ) {
					val = emup_read ( SEL_SECRET );
					printf ( "Secret: %04x\n", val );
					thr_delay ( 400 );
				}
			}
			/* Vcc DAC mut be on for the TTL registers
			 * to do anything.
			 */
			if ( **wp == 't' && nw == 2 ) {
				val = hextoi ( wp[1] );
				printf ( "Write %04x to TTL0\n", val );
				emup_write ( SEL_TTL0, val );
			}
			if ( **wp == 't' && nw == 3 ) {
				i = atoi ( wp[1] );
				val = hextoi ( wp[2] );
				if ( i < 0 || i > 4 ) {
					printf ( "Sorry\n" );
					continue;
				}
				printf ( "Write %04x to TTL%d\n", val, i );
				emup_write ( SEL_TTL0 + i, val );
			}
			if ( **wp == 'r' ) {
				for ( i=0; i<256; i++ )
					val = read28 ( i );
				// printf ( "Read gives: %02x at %04x\n", val, addr );
			}

		}

		emup_io_init ();
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
		emup_io_init ();

		emup_user ();
}

/* THE END */
