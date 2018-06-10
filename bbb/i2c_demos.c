/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
#include <kyu.h>
#include "i2c.h"

#ifdef ARCH_ESP8266
#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#endif

#ifdef ARCH_ARM
#include "gpio.h"
#define os_printf	printf
#define ICACHE_FLASH_ATTR
#define os_delay_us(x)	delay_us ( (x) )
#endif

#define  BMP_ADDR 0x77

#define  REG_CALS		0xAA
#define  REG_CONTROL	0xF4
#define  REG_RESULT		0xF6
#define  REG_ID			0xD0

#define  CMD_TEMP 0x2E
#define  TDELAY 4500

/* OSS can have values 0, 1, 2, 3
 * representing internal sampling of 1, 2, 4, or 8 values
 * note that sampling more will use more power.
 *  (since each conversion takes about the same dose of power)
 */

#define  OSS_ULP	0			/* 16 bit - Ultra Low Power */
#define  CMD_P_ULP  0x34
#define  DELAY_ULP  4500
#define  SHIFT_ULP  8

#define  OSS_STD	1			/* 17 bit - Standard */
#define  CMD_P_STD  0x74
#define  DELAY_STD  7500
#define  SHIFT_STD	7

#define  OSS_HR		2			/* 18 bit - High Resolution */
#define  CMD_P_HR   0xB4
#define  DELAY_HR   13500
#define  SHIFT_HR	6

#define  OSS_UHR	3			/* 19 bit - Ultra High Resolution */
#define  CMD_P_UHR  0xF4
#define  DELAY_UHR  25500
#define  SHIFT_UHR	5

/* This chooses from the above */
#define  OSS 		OSS_STD
#define  CMD_PRESS	CMD_P_STD
#define  PDELAY 	DELAY_STD
#define  PSHIFT 	SHIFT_STD
// #define  PSHIFT 	(8-OSS)

#define NCALS	11

struct bmp_cals {
	short ac1;
	short ac2;
	short ac3;
	unsigned short ac4;
	unsigned short ac5;
	unsigned short ac6;
	short b1;
	short b2;
	short mb;
	short mc;
	short md;
} bmp_cal;

#define	AC1	bmp_cal.ac1
#define	AC2	bmp_cal.ac2
#define	AC3	bmp_cal.ac3
#define	AC4	bmp_cal.ac4
#define	AC5	bmp_cal.ac5
#define	AC6	bmp_cal.ac6
#define	B1	bmp_cal.b1
#define	B2	bmp_cal.b2
#define	MB	bmp_cal.mb	/* never used */
#define	MC	bmp_cal.mc
#define	MD	bmp_cal.md

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* BMP180 specific routines --- */

#ifdef OLD_BMP
/* ID register -- should always yield 0x55
 */
static int ICACHE_FLASH_ATTR
read_id ( void )
{
	int id;

	id = iic_read ( BMP_ADDR, REG_ID );

	return id;
}

/* For extra diagnostics */
static int ICACHE_FLASH_ATTR
read_idx ( void )
{
	int id;

	id = iic_readx ( BMP_ADDR, REG_ID );

	return id;
}

static int ICACHE_FLASH_ATTR
read_temp ( void )
{
	int rv;

	(void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_TEMP );

	os_delay_us ( TDELAY );

	rv = iic_read_16 ( BMP_ADDR, REG_RESULT );

	return rv;
}

/* Pressure reads 3 bytes
 */

static int ICACHE_FLASH_ATTR
read_pressure ( void )
{
	int rv;

	(void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_PRESS );

	os_delay_us ( PDELAY );

	rv = iic_read_24 ( BMP_ADDR, REG_RESULT );

	return rv >> PSHIFT;
}

static void ICACHE_FLASH_ATTR
read_cals ( unsigned short *buf )
{
	/* Note that on the ESP8266 just placing the bytes into
	 * memory in the order read out does not yield an array of shorts.
	 * This is because the BMP180 gives the MSB first, but the
	 * ESP8266 is little endian.  So we use our routine that gives
	 * us an array of shorts and we are happy.
	 */
	// iic_read_n ( BMP_ADDR, REG_CALS, (unsigned char *) buf, 2*NCALS );
	iic_read_16n ( BMP_ADDR, REG_CALS, buf, NCALS );
}
#endif /* OLD_BMP */

/* ---------------------------------------------- */
/* ---------------------------------------------- */

#ifdef notdef
/* This raw pressure value is already shifted by 8-oss */
int rawt = 25179;
int rawp = 77455;

int ac1 = 8240;
int ac2 = -1196;
int ac3 = -14709;
int ac4 = 32912;	/* U */
int ac5 = 24959;	/* U */
int ac6 = 16487;	/* U */
int b1 = 6515;
int b2 = 48;
int mb = -32768;
int mc = -11786;
int md = 2845;
#endif

int b5;

/* Yields temperature in 0.1 degrees C */
int
conv_temp ( int raw )
{
	int x1, x2;

	x1 = ((raw - AC6) * AC5) >> 15;
	x2 = MC * 2048 / (x1 + MD);
	b5 = x1 + x2;
	return (b5 + 8) >> 4;
}

/* Yields pressure in Pa */
int
conv_pressure ( int raw )
{
	int b3, b6;
	unsigned long b4, b7;
	int x1, x2, x3;
	int p;

	b6 = b5 - 4000;
	x1 = B2 * (b6 * b6 / 4096) / 2048;
	x2 = AC2 * b6 / 2048;
	x3 = x1 + x2;
	b3 = ( ((AC1 * 4 + x3) << OSS) + 2) / 4;
	x1 = AC3 * b6 / 8192;
	x2 = (B1 * (b6 * b6 / 4096)) / 65536;
	x3 = (x1 + x2 + 2) / 4;

	b4 = AC4 * (x3 + 32768) / 32768;
	b7 = (raw - b3) * (50000 >> OSS);

	p = (b7 / b4) * 2;
	x1 = (p / 256) * (p / 256);
	x1 = (x1 * 3038) / 65536;
	x2 = (-7357 * p) / 65536;
	p += (x1 + x2 + 3791) / 16;

	return p;
}

#ifdef notdef
int
conv_tempX ( int raw )
{
	int x1, x2;

	x1 = (raw - ac6) * ac5 / 32768;
	x2 = mc * 2048 / (x1 + md);
	b5 = x1 + x2;
	return (b5+8) / 16;
}

int
conv_pressureX ( int raw, int oss )
{
	int b3, b6;
	unsigned long b4, b7;
	int x1, x2, x3;
	int p;

	b6 = b5 - 4000;
	x1 = b2 * (b6 * b6 / 4096) / 2048;
	x2 = ac2 * b6 / 2048;
	x3 = x1 + x2;
	b3 = ( ((ac1 * 4 + x3) << oss) + 2) / 4;
	x1 = ac3 * b6 / 8192;
	x2 = (b1 * (b6 * b6 / 4096)) / 65536;
	x3 = (x1 + x2 + 2) / 4;

	b4 = ac4 * (x3 + 32768) / 32768;
	b7 = (raw - b3) * (50000 >> oss);

	p = (b7 / b4) * 2;
	x1 = (p / 256) * (p / 256);
	x1 = (x1 * 3038) / 65536;
	x2 = (-7357 * p) / 65536;
	p += (x1 + x2 + 3791) / 16;

	return p;
}
#endif

// #define MB_TUCSON	84.0
#define MB_TUCSON	8400

int
convert ( int rawt, int rawp )
{
	int t;
	int p;
	//int t2;
	//int p2;
	int tf;
	int pmb_sea;

	// double tc;
	// double tf;
	// double pmb;
	// double pmb_sea;

	t = conv_temp ( rawt );
	//t2 = conv_tempX ( rawt );
	// tc = t / 10.0;
	// tf = 32.0 + tc * 1.8;
	tf = t * 18;
	tf = 320 + tf / 10;

	// os_printf ( "Temp = %d\n", t );
	// os_printf ( "Temp (C) = %.3f\n", tc );
	// os_printf ( "Temp (F) = %.3f\n", tf );
	// os_printf ( "Temp = %d (%d)\n", t, tf );

	p = conv_pressure ( rawp );
	// p2 = conv_pressureX ( rawp, OSS );
	// pmb = p / 100.0;
	// pmb_sea = pmb + MB_TUCSON;
	pmb_sea = p + MB_TUCSON;

	// os_printf ( "Pressure (Pa) = %d\n", p );
	// os_printf ( "Pressure (mb) = %.2f\n", pmb );
	// os_printf ( "Pressure (mb, sea level) = %.2f\n", pmb_sea );
	// os_printf ( "Pressure (mb*100, sea level) = %d\n", pmb_sea );

	os_printf ( "Temp = %d -- Pressure (mb*100, sea level) = %d\n", tf, pmb_sea );
}

void
show_cals ( void *cals )
{
		short *calbuf = (short *) cals;
		unsigned short *calbuf_u = (unsigned short *) cals;

		os_printf ( "cal 1 = %d\n", calbuf[0] );
		os_printf ( "cal 2 = %d\n", calbuf[1] );
		os_printf ( "cal 3 = %d\n", calbuf[2] );
		os_printf ( "cal 4 = %d\n", calbuf_u[3] );
		os_printf ( "cal 5 = %d\n", calbuf_u[4] );
		os_printf ( "cal 6 = %d\n", calbuf_u[5] );
		os_printf ( "cal 7 = %d\n", calbuf[6] );
		os_printf ( "cal 8 = %d\n", calbuf[7] );
		os_printf ( "cal 9 = %d\n", calbuf[8] );
		os_printf ( "cal 10 = %d\n", calbuf[9] );
		os_printf ( "cal 11 = %d\n", calbuf[10] );
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* MCP23008 driver follows
 * The MCP23017 expands from 8 to 16 bits
 * each register has the "extension" at addr + 0x10
 * (so MCP_DIR is at 0x00 and 0x10)
 */

#define MCP_ADDR	0x20

#define MCP_DIR		0x00
#define MCP_GPIO	0x09
#define MCP_OLAT	0x0a

static int ICACHE_FLASH_ATTR
read_mcp ( struct i2c *ip )
{
	return i2c_read_8 ( ip, MCP_ADDR, MCP_GPIO );
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* MCP4725 driver follows
 * This device is peculiar in that it does not have registers
 * You either read or write.
 * read always returns 3 bytes.
 * write has 4 flavors.
 */
#define DAC_ADDR	0x60

static void ICACHE_FLASH_ATTR
dac_write ( struct i2c *ip, unsigned int val )
{
	unsigned char iobuf[2];

	iobuf[0] = (val >> 8) & 0xf;
	iobuf[1] = val & 0xff;
	i2c_send ( ip, DAC_ADDR, iobuf, 2 );
}

static void ICACHE_FLASH_ATTR
dac_write_ee ( struct i2c *ip, unsigned int val )
{
	unsigned char iobuf[3];

	iobuf[0] = 0x60;
	iobuf[1] = val >> 4;
	// iobuf[2] = (val & 0xf) << 4;
	iobuf[2] = val << 4;
	i2c_send ( ip, DAC_ADDR, iobuf, 3 );
}

/* returns up to 5 bytes.
 * "status" byte
 * DAC value (2 bytes)
 * EEPROM value (2 bytes)
 */
static void ICACHE_FLASH_ATTR
dac_read ( struct i2c *ip, unsigned char *buf, int n )
{
	iic_recv ( ip, DAC_ADDR, buf, n );
}

static unsigned int ICACHE_FLASH_ATTR
dac_read_val ( struct i2c *ip )
{
	unsigned char iobuf[3];

	iic_recv ( ip, DAC_ADDR, iobuf, 3 );
	return (iobuf[1] << 4) | (iobuf[2] >> 4);
}
/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* HDC1008 driver
 */
#define HDC_ADDR	0x40

#define HDC_TEMP	0x00
#define HDC_HUM		0x01
#define HDC_CON		0x02
#define HDC_SN1		0xFB
#define HDC_SN2		0xFC
#define HDC_SN3		0xFD

/* bits/fields in config register */
#define HDC_RESET	0x8000
#define HDC_HEAT	0x2000
#define HDC_BOTH	0x1000
#define HDC_BSTAT	0x0800

#define HDC_TRES14	0x0000
#define HDC_TRES11	0x0400

#define HDC_HRES14	0x0000
#define HDC_HRES11	0x0100
#define HDC_HRES8	0x0200

/* Delays in microseconds */
#define CONV_8		2500
#define CONV_11		3650
#define CONV_14		6350

#define CONV_BOTH	12700

/* Read gets both T then H */
static void ICACHE_FLASH_ATTR
hdc_con_both ( struct i2c *ip )
{
	unsigned char buf[3];

	//(void) iic_write16 ( HDC_ADDR, HDC_CON, HDC_BOTH );
	buf[0] = HDC_CON;
	buf[1] = HDC_BOTH >> 8;
	buf[2] = HDC_BOTH & 0xff;
	i2c_send ( ip, HDC_ADDR, buf, 3 );
}

/* Read gets either T or H */
static void ICACHE_FLASH_ATTR
hdc_con_single ( struct i2c *ip )
{
	unsigned char buf[3];

	// (void) iic_write16 ( HDC_ADDR, HDC_CON, 0 );
	buf[0] = HDC_CON;
	buf[1] = 0;
	buf[2] = 0;
	i2c_send ( ip, HDC_ADDR, buf, 3 );
}

static void ICACHE_FLASH_ATTR
hdc_read_both ( struct i2c *ip, unsigned short *buf )
{
	unsigned char bbuf[4];

	bbuf[0] = HDC_TEMP;
	i2c_send ( ip, HDC_ADDR, bbuf, 1 );

	os_delay_us ( CONV_BOTH );

	i2c_recv ( ip, HDC_ADDR, bbuf, 4 );
	buf[0] = bbuf[0] << 8 | bbuf[1];
	buf[1] = bbuf[2] << 8 | bbuf[3];
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */

#ifdef OLD_TESTS
static void ICACHE_FLASH_ATTR
upd_mcp ( void )
{
	int val;
	static int phase = 0;

	// val = read_gpio ();
	// os_printf ( "MCP gpio = %02x\n", val );

	if ( phase ) {
		iic_write ( MCP_ADDR, MCP_OLAT, 0 );
		phase = 0;
	} else {
		iic_write ( MCP_ADDR, MCP_OLAT, 0xff );
		phase = 1;
	}

}

static void ICACHE_FLASH_ATTR
read_bmp ( void )
{
	int t, p;

	t = read_temp ();
	p = read_pressure ();
	//os_printf ( "temp = %d\n", val );
	//os_printf ( "pressure = %d\n", val );
	//os_printf ( "raw T, P = %d  %d\n", t, p );

	convert ( t, p );
}

static void ICACHE_FLASH_ATTR
dac_show ( void )
{
	unsigned char io[5];

	dac_read ( io, 5 );

	os_printf ( "DAC status = %02x\n", io[0] );
	os_printf ( "DAC val = %02x %02x\n", io[1], io[2] );
	os_printf ( "DAC ee = %02x %02x\n", io[3], io[4] );
}

static unsigned int dac_val = 0;

static void ICACHE_FLASH_ATTR
dac_doodle ( void )
{
	dac_val += 16;
	if ( dac_val > 4096 ) dac_val = 0;
	dac_write ( dac_val);
}

static void ICACHE_FLASH_ATTR
hdc_test ( void )
{
	int val;

	/* This does not work.
	 * device does not autoincrement address
	 */
	// unsigned short iobuf[3];
	// (void) iic_read_16n ( HDC_ADDR, HDC_SN1, iobuf, 3 );

	val = iic_read_16 ( HDC_ADDR, HDC_SN1 );
	os_printf ( "HDC sn = %04x\n", val );
	val = iic_read_16 ( HDC_ADDR, HDC_SN2 );
	os_printf ( "HDC sn = %04x\n", val );
	val = iic_read_16 ( HDC_ADDR, HDC_SN3 );
	os_printf ( "HDC sn = %04x\n", val );

	val = iic_read_16 ( HDC_ADDR, HDC_CON );
	os_printf ( "HDC con = %04x\n", val );

	hdc_con_both ();
}

/* ---------------------------------------- */

static void
iic_tester ( void )
{
	static int first = 1;
	static int count = 0;
	int val;

	if ( first ) {
		val = read_id ();
		os_printf ( "BMP180 id = %02x\n", val );

		read_cals ( (unsigned short *) &bmp_cal );
		show_cals ( (void *) &bmp_cal );

		// iic_write ( MCP_ADDR, MCP_DIR, 0 );		/* outputs */

		// dac_show ();

		// hdc_test ();

		first = 0;
	}

	// Scope loop
	//for ( ;; ) {
	//	val = read_id ();
	//}

	val = read_id ();
	os_printf ( "BMP180 id = %02x\n", val );

	read_cals ( (unsigned short *) &bmp_cal );
	show_cals ( (void *) &bmp_cal );

	// read_bmp ();
	// upd_mcp ();
	// dac_doodle ();
	// hdc_read ();

	/*
	if ( ++count > 10 ) {
		os_printf ( "Finished\n" );
		os_timer_disarm(&timer);
	}
	*/
}

void
iic_tester2 ( void )
{
	int i;
	int val;

	// id = iic_read ( BMP_ADDR, REG_ID );

	for ( i=0; i<4; i++ )
		iic_diag ( BMP_ADDR, REG_ID );

	for ( i=0; i<4; i++ )
		iic_diag ( BMP_ADDR + 1, REG_ID );

	for ( i=0; i<4; i++ ) {
		val = read_id ();
		os_printf ( "BMP180 id = %02x\n", val );
	}

	val = read_idx ();
	os_printf ( "BMP180 id = %02x\n", val );
}

/* test for ARM */
void iic_test ( void )
{
	/* on the BBB,
	 *	sda = gpio_2_2 = P8 pin 7
	 *	scl = gpio_2_5 = P8 pin 9
	 */
	iic_init ( GPIO_2_2, GPIO_2_5 );

	iic_tester ();

	//iic_tester ();

	// iic_pulses ();
}

#endif /* OLD_TESTS */

#ifdef ARCH_ESP8266
static os_timer_t timer;

static void ICACHE_FLASH_ATTR
ticker ( void *arg )
{
	iic_tester ();
}

#define IIC_SDA		4
#define IIC_SCL		5

/* Reading the BMP180 sensor takes 4.5 + 7.5 milliseconds */
#define DELAY 1000 /* milliseconds */
// #define DELAY 2 /* 2 milliseconds - for DAC test */

void user_init(void)
{
	// Configure the UART
	// uart_init(BIT_RATE_9600,0);
	uart_div_modify(0, UART_CLK_FREQ / 115200);
	os_printf ( "\n" );

	iic_init ( IIC_SDA, IIC_SCL );

	// os_printf ( "BMP180 address: %02x\n", BMP_ADDR );
	// os_printf ( "OSS = %d\n", OSS );

	// Set up a timer to tick continually
	os_timer_disarm(&timer);
	os_timer_setfn(&timer, ticker, (void *)0);
	os_timer_arm(&timer, DELAY, 1);
}
#endif /* ARCH_ESP8266 */

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

/* Below here is for i2c interface testing */

/* ID register -- should always yield 0x55
 */
static int ICACHE_FLASH_ATTR
bmp_id ( struct i2c *ip )
{
	return i2c_read_8 ( ip, BMP_ADDR, REG_ID );
}

static int ICACHE_FLASH_ATTR
bmp_temp ( struct i2c *ip )
{
	int rv;

	// (void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_TEMP );
	i2c_write_8 ( ip, BMP_ADDR, REG_CONTROL, CMD_TEMP );

	os_delay_us ( TDELAY );

	// rv = iic_read_16 ( BMP_ADDR, REG_RESULT );
	rv = i2c_read_16 ( ip, BMP_ADDR, REG_RESULT );

	return rv;
}

/* Pressure reads 3 bytes
 */

static int ICACHE_FLASH_ATTR
bmp_pressure ( struct i2c *ip )
{
	int rv;

	// (void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_PRESS );
	i2c_write_8 ( ip, BMP_ADDR, REG_CONTROL, CMD_PRESS );

	os_delay_us ( PDELAY );

	// rv = iic_read_24 ( BMP_ADDR, REG_RESULT );
	rv = i2c_read_24 ( ip, BMP_ADDR, REG_RESULT );

	return rv >> PSHIFT;
}

static void ICACHE_FLASH_ATTR
bmp_cals ( struct i2c *ip, unsigned short *buf )
{
	char bbuf[2*NCALS];
	int i, j;

	/* Note that on the ESP8266 (or any little endian machine)
	 * just placing the bytes into memory in the order read out
	 * does not yield an array of shorts.
	 * This is because the BMP180 gives the MSB first, but the
	 * ESP8266 is little endian.
	 */

	i2c_read_reg_n ( ip, BMP_ADDR, REG_CALS, bbuf, 2*NCALS );

	for ( i=0; i<NCALS; i++ ) {
	    j = i + i;
	    buf[i] = bbuf[j]<<8 | bbuf[j+1];
	}
}

void
bmp_once ( struct i2c *ip )
{
	int t, p;

	t = bmp_temp ( ip );
	p = bmp_pressure ( ip );
	//os_printf ( "temp = %d\n", val );
	//os_printf ( "pressure = %d\n", val );
	//os_printf ( "raw T, P = %d  %d\n", t, p );

	convert ( t, p );
}

void
bmp_diag1 ( struct i2c *ip )
{
	int id;
	int tt, pp;
	unsigned short cals[11];
	int i;

	for ( i=0; i<8; i++ ) {
	    id = bmp_id ( ip );
	    printf ( "BMP ID = %02x\n", id );
	}

	for ( i=0; i<8; i++ ) {
	    tt = bmp_temp ( ip );
	    printf ( "BMP temp = %d\n", tt );
	}

	for ( i=0; i<8; i++ ) {
	    id = bmp_id ( ip );
	    printf ( "BMP ID = %02x\n", id );
	    tt = bmp_temp ( ip );
	    printf ( "BMP temp = %d\n", tt );
	}

	pp = bmp_pressure ( ip );
	printf ( "BMP pressure = %d\n", pp );

	bmp_cals ( ip, (unsigned short *) &bmp_cal );
	show_cals ( (void *) &bmp_cal );
}

void
bmp_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( 1 );

	bmp_diag1 ( ip );

	bmp_cals ( ip, (unsigned short *) &bmp_cal );

	// 		 label  func      arg pri flags interval
	thr_new_repeat ( "bmp", bmp_once, ip, 13, 0, 1000 );
}

static void
hdc_once ( struct i2c *ip )
{
	unsigned short iobuf[2];
	int t, h;
	int tf;

	hdc_read_both ( ip, iobuf );

	/*
	t = 99;
	h = 23;
	tf = 0;
	os_printf ( " Bogus t, tf, h = %d  %d %d\n", t, tf, h );
	*/
	// os_printf ( " HDC raw t,h = %04x  %04x\n", iobuf[0], iobuf[1] );

	t = ((iobuf[0] * 165) / 65536)- 40;
	tf = t * 18 / 10 + 32;
	// os_printf ( " -- traw, t, tf = %04x %d   %d %d\n", iobuf[0], iobuf[0], t, tf );

	//h = ((iobuf[1] * 100) / 65536);
	//os_printf ( "HDC t, tf, h = %d  %d %d\n", t, tf, h );

	h = ((iobuf[1] * 100) / 65536);
	// os_printf ( " -- hraw, h = %04x %d    %d\n", iobuf[1], iobuf[1], h );
	printf ( "HDC t,f = %d %d\n", tf, h );
}

void
hdc_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( 1 );

	thr_new_repeat ( "hdc", hdc_once, ip, 13, 0, 1000 );
}

static void
dac_show ( struct i2c *ip )
{
	unsigned char io[5];

	dac_read ( ip, io, 5 );

	os_printf ( "DAC status = %02x\n", io[0] );
	os_printf ( "DAC val = %02x %02x\n", io[1], io[2] );
	os_printf ( "DAC ee = %02x %02x\n", io[3], io[4] );
}

static unsigned int dac_val = 0;

static void
dac_once ( struct i2c *ip )
{
	dac_val += 16;
	if ( dac_val > 4096 ) dac_val = 0;
	dac_write ( ip, dac_val);
}

static void
dac_fast ( struct i2c *ip )
{
	dac_val = 0;

	for ( ;; ) {
	    dac_val += 16;
	    if ( dac_val > 4096 ) dac_val = 0;
	    dac_write ( ip, dac_val);
	    // os_delay_us ( 1 );
	}
}

/* This showed that with back to back writes like this,
 * I can update the dac every 319 microseconds.
 * This is a dac update rate of 3140 Hz.
 * And this is about right with a 100 kHz clock.
 * We send 3 bytes (address and two data) and if you
 * figure 10 bits on the wire for each byte this is
 * 30 bits so 100 khz / 30 = 3.3 khz
 */

static void
dac_pulse ( struct i2c *ip )
{
	for ( ;; ) {
	    dac_write ( ip, 0 );
	    dac_write ( ip, 0xfff );
	}
}


void
dac_test ( void )
{
	struct i2c *ip;

	ip = i2c_hw_new ( 1 );

	// dac_show ( ip );

	// thr_new_repeat ( "dac", dac_once, ip, 13, 0, 1 );
	// dac_fast ( ip );
	dac_pulse ( ip );
}

/* The tests are run via the shell "x" command, i.e.
 *
 * x bmp_test
 * x hdc_test
 * x dac_test
 */

/* THE END */
