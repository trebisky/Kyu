/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* Tom Trebisky
 * 5-11-2016 - begun for ESP8266
 * 5-29-2016 - begin port for ARM/BBB/Kyu
 * 6-22-2016 - integrated under new i2c.c
 *
 *  iic.c
 *
 *  Bit banging i2c library
 *    for the ESP8266 and for Kyu/BBB
 *
 * A key idea is that the sda and scl pins can be
 * specified as arguments to the initializer function.
 *
 * A fairly high level interface is presented as an API.
 *  low level code derived from i2c_master.c
 */
#include <kyu.h>

#ifdef ARCH_ESP8266
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#define printf	os_printf
#endif

#ifdef ARCH_ARM
#include "gpio.h"
#endif

static void iic_setdc ( int, int );
static void iic_dc ( int, int );
static void iic_dc_wait ( int, int, int );
static void iic_writeb ( int );
static int iic_readb ( void );
static void iic_setAck ( int );
static int iic_getAck ( void );

static void iic_start ( void );
static void iic_stop ( void );
static int iic_recv_byte ( int );
static int iic_send_byte ( int );

/* The following functions are all that a person
 * should ever need to use.
 */
void iic_init ( int, int );
int iic_send ( int, unsigned char *, int );
int iic_recv ( int, unsigned char *, int );

#ifndef ARCH_ESP8266
#define ICACHE_FLASH_ATTR
typedef unsigned char uint8;

#define os_delay_us(x)	delay_us ( (x) )
#endif

/* -------------------------------------------------- */

#ifdef ARCH_ESP8266
/* Part of a general GPIO facility. */

/* place holder for unused channels */
#define Z       0

/* This is an array of pin control register addresses.
 */
static const int mux[] = {
    PERIPHS_IO_MUX_GPIO0_U,     /* 0 - D3 */
    PERIPHS_IO_MUX_U0TXD_U,     /* 1 - uart */
    PERIPHS_IO_MUX_GPIO2_U,     /* 2 - D4 */
    PERIPHS_IO_MUX_U0RXD_U,     /* 3 - uart */
    PERIPHS_IO_MUX_GPIO4_U,     /* 4 - D2 */
    PERIPHS_IO_MUX_GPIO5_U,     /* 5 - D1 */
    Z,  /* 6 */
    Z,  /* 7 */
    Z,  /* 8 */
    PERIPHS_IO_MUX_SD_DATA2_U,  /* 9   - D11 (SD2) */
    PERIPHS_IO_MUX_SD_DATA3_U,  /* 10  - D12 (SD3) */
    Z,  /* 11 */
    PERIPHS_IO_MUX_MTDI_U,      /* 12 - D6 */
    PERIPHS_IO_MUX_MTCK_U,      /* 13 - D7 */
    PERIPHS_IO_MUX_MTMS_U,      /* 14 - D5 */
    PERIPHS_IO_MUX_MTDO_U       /* 15 - D8 */
};

/* These are the mux values that put a pin into GPIO mode
 */
static const uint8 func[] = { 0, 3, 0, 3,   0, 0, Z, Z,   Z, 3, 3, Z,   3, 3, 3, 3 };

static void ICACHE_FLASH_ATTR
gpio_iic_setup ( int gpio )
{
    int reg;

    PIN_FUNC_SELECT ( mux[gpio], func[gpio] );

    /* make this open drain */
    reg = GPIO_PIN_ADDR ( gpio );
    GPIO_REG_WRITE ( reg, GPIO_REG_READ( reg ) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE) );

    GPIO_REG_WRITE (GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << gpio) );
}
#endif

/* -------------------------------------------------- */

/* Some notes on the i2c protocol
 * both signals are open drain.
 *  a device can pull a line low, but can never drive it high.
 * an idle bus has both sda and scl high
 * a start sequence pulls SDA low, with SCL left high
 * a stop sequence first raises SCL, then raises SDA
 *  (avoid changing SDS with SCL high to avoid false stops)
 * data is asserted after SCL falls, is sampled when SCL rises.
 */
/* -------------------------------------------------- */

static uint8 sda_pin;
static uint8 scl_pin;	/* never used */

static uint8 cur_sda;
static uint8 cur_scl;

static uint8 sda_mask;
static uint8 scl_mask;
static uint8 all_mask;

#define MAX_BITS	28

static void ICACHE_FLASH_ATTR
iic_bus_init ( void )
{
    int i;

    iic_dc ( 1, 0 );

    iic_dc ( 0, 0 );
    iic_dc ( 1, 0 );

    for (i = 0; i < MAX_BITS; i++) {
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );
    }

    iic_stop();
}

static void ICACHE_FLASH_ATTR
iic_gpio_init ( int sda, int scl )
{
    sda_pin = sda;
    scl_pin = scl;

    sda_mask = 1 << sda;
    scl_mask = 1 << scl;
    all_mask = sda_mask | scl_mask;

#ifdef ARCH_ESP8266
    ETS_GPIO_INTR_DISABLE() ;
    gpio_iic_setup ( sda );
    gpio_iic_setup ( scl );

    ETS_GPIO_INTR_ENABLE() ;
#endif

#ifdef ARCH_ARM
    gpio_iic_init ( sda );
    gpio_iic_init ( scl );
#endif

    iic_setdc ( 1, 1 );
}

/* Arguments are gpio numbers, 0-15 */
void ICACHE_FLASH_ATTR
iic_init ( int sda_pin, int scl_pin )
{
    iic_gpio_init ( sda_pin, scl_pin );
    iic_bus_init ();
}

/* -------------------------------------------------- */

#ifdef ARCH_ESP8266
/* Could be a macro */
static int ICACHE_FLASH_ATTR
iic_get_bit ( void )
{
    // return GPIO_INPUT_GET ( SDA_GPIO );
    return ( gpio_input_get() >> sda_pin) & 1;
}

static void ICACHE_FLASH_ATTR
iic_setdc ( int sda, int scl )
{
    int high_mask;
    int low_mask;

    cur_sda = sda;
    cur_scl = scl;

    if ( sda ) {
        high_mask = sda_mask;
        low_mask = 0;
    } else {
        high_mask = 0;
        low_mask = sda_mask;
    }

    if ( scl )
        high_mask |= scl_mask;
    else
        low_mask |= scl_mask;

    gpio_output_set( high_mask, low_mask, all_mask, 0);
}
#endif

#ifdef ARCH_ARM
static int
iic_raw_bit ( void )
{
    return gpio_read_bit ( sda_pin ) ? 1 : 0;
}

static int
iic_get_bit ( void )
{
    int rv;

    gpio_dir_in ( sda_pin );
    rv = gpio_read_bit ( sda_pin );
    gpio_dir_out ( sda_pin );
    return rv ? 1 : 0;
}

static void
iic_setdc ( int sda, int scl )
{
    cur_sda = sda;
    if ( sda ) {
	gpio_set_bit ( sda_pin );
    } else {
	gpio_clear_bit ( sda_pin );
    }

    cur_scl = scl;
    if ( scl ) {
	gpio_set_bit ( scl_pin );
    } else {
	gpio_clear_bit ( scl_pin );
    }
}

static void
iic_setclk ( int scl )
{
    if ( scl ) {
	gpio_set_bit ( scl_pin );
    } else {
	gpio_clear_bit ( scl_pin );
    }
}
#endif

static void ICACHE_FLASH_ATTR
iic_dc ( int data, int clock )
{
    iic_setdc ( data, clock );
    os_delay_us ( 5 );
}

static void ICACHE_FLASH_ATTR
iic_dc_wait ( int data, int clock, int wait )
{
    iic_setdc ( data, clock );
    os_delay_us ( wait );
}

static void ICACHE_FLASH_ATTR
iic_start(void)
{
    iic_dc ( 1, cur_scl );
    iic_dc ( 1, 1 );
    iic_dc ( 0, 1 );
}

static void ICACHE_FLASH_ATTR
iic_stop(void)
{
    os_delay_us (5);

    iic_dc ( 0, cur_scl );
    iic_dc ( 0, 1 );
    iic_dc ( 1, 1 );
}

static void ICACHE_FLASH_ATTR
iic_setAck ( int level )
{
    iic_dc ( cur_sda, 0 );
    iic_dc ( level, 0 );
    iic_dc ( level, 1 );
    iic_dc ( level, 0 );
    iic_dc ( 1, 0 );
}

static int ICACHE_FLASH_ATTR
iic_getAck ( void )
{
    int rv;

    iic_dc ( cur_sda, 0 );
    iic_dc ( 1, 0 );
    iic_dc ( 1, 1 );

    rv = iic_get_bit ();
    os_delay_us (5);

    iic_dc ( 1, 0 );

    return rv;
}

#ifdef ARCH_ARM
/* XXX - why even manipulate the sda pin when reading ?? */
/* We send the Ack outside of this routine */
static int
iic_readb ( void )
{
    int rv = 0;
    int i;

    gpio_dir_in ( sda_pin );

    os_delay_us (5);

    iic_dc ( cur_sda, 0 );

    for (i = 0; i < 8; i++) {
        os_delay_us (5);
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );

        rv |= iic_get_bit() << (7-i);

	iic_dc_wait ( 1, 1, i == 7 ? 8 : 5 );
    }

    gpio_dir_out ( sda_pin );

    iic_dc ( 1, 0 );

    return rv;
}

static void
iic_watch ( int delay )
{
    int i;
    int val;

    for ( i=0; i<delay; i++ ) {
        val = iic_raw_bit();
	printf ( "SDA = %d\n", val );
	os_delay_us ( 1 );
    }
}

static void
iic_clk_d ( int clk, int delay )
{
    iic_setclk ( clk );
    iic_watch ( delay );
}

static int
iic_readbx ( void )
{
    int rv = 0;
    int i;
    int val;

    gpio_dir_in ( sda_pin );

    os_delay_us (5);

    // iic_dc ( cur_sda, 0 );
    iic_clk_d ( 0, 5 );

    for (i = 0; i < 8; i++) {
        // os_delay_us (5);
	iic_watch ( 5 );
	// iic_dc ( 1, 0 );
	iic_clk_d ( 0, 5 );
	// iic_dc ( 1, 1 );
	iic_clk_d ( 1, 5 );

        // rv |= gpio_read_bit( sda_pin ) << (7-i);
        val = iic_raw_bit();
	rv |= val << (7-i);
	printf ( "*SDA = %d\n", val );

	// iic_dc_wait ( 1, 1, i == 7 ? 8 : 5 );
	iic_clk_d ( 1, i == 7 ? 8 : 5 );
    }

    gpio_dir_out ( sda_pin );

    iic_dc ( 1, 0 );

    return rv;
}
#endif

#ifdef ARCH_ESP8266
static int ICACHE_FLASH_ATTR
iic_readb ( void )
{
    int rv = 0;
    int i;

    os_delay_us (5);

    iic_dc ( cur_sda, 0 );

    for (i = 0; i < 8; i++) {
        os_delay_us (5);
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );

        rv |= iic_get_bit () << (7-i);

	iic_dc_wait ( 1, 1, i == 7 ? 8 : 5 );
    }

    iic_dc ( 1, 0 );

    return rv;
}
#endif

static void ICACHE_FLASH_ATTR
iic_writeb ( int data )
{
    int bit;
    int i;

    os_delay_us (5);

    iic_dc ( cur_sda, 0 );

    for (i = 7; i >= 0; i--) {
        // bit = data >> i;
        bit = (data >> i) & 1;
	iic_dc ( bit, 0 );
	iic_dc_wait ( bit, 1, i == 0 ? 8 : 5 );
	iic_dc ( bit, 0 );
    }
}

static int ICACHE_FLASH_ATTR
iic_send_byte ( int byte )
{
	int ack;

	iic_writeb ( byte );
	ack = iic_getAck();
	if ( ack ) {
	    iic_stop();
	    return 1;
	}
	return 0;
}

/* XXX - useful when debugging
 *  but not what I would want when in production.
 */
static int ICACHE_FLASH_ATTR
iic_send_byte_m ( int byte, char *msg )
{
	int ack;

	iic_writeb ( byte );
	ack = iic_getAck();
	if ( ack ) {
	    printf("IIC: No ack after sending %s\n", msg);
	    iic_stop();
	    return 1;
	}
	return 0;
}

static int ICACHE_FLASH_ATTR
iic_recv_byte ( int ack )
{
	int rv;

	rv = iic_readb();
	iic_setAck ( ack );
	return rv;
}

/* ----------------------------------------------------------- */
/* Higher level iic routines */
/* ----------------------------------------------------------- */

#define IIC_WADDR(a)	(a << 1)
#define IIC_RADDR(a)	((a << 1) | 1)

/* raw write an array of bytes (8 bit objects)
 * for a device without registers (like the MCP4725)
 */
int ICACHE_FLASH_ATTR
iic_send ( int addr, unsigned char *buf, int n )
{
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	for ( i = 0; i < n; i++ ) {
		if ( iic_send_byte_m ( buf[i], "reg" ) ) return 1;
	}
	iic_stop();

	return 0;
}

/* raw read an array of bytes (8 bit objects)
 * for a device without registers (like the MCP4725)
 */
int ICACHE_FLASH_ATTR
iic_recv ( int addr, unsigned char *buf, int n )
{
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;

	for ( i=0; i < n; i++ ) {
		*buf++ = iic_recv_byte ( i == n - 1 ? 1 : 0 );
	}

	iic_stop();

	return 0;
}

#ifdef OLD_HIGH_LEVEL
/* raw read an array of shorts (16 bit objects)
 */
int ICACHE_FLASH_ATTR
iic_read_16raw ( int addr, unsigned short *buf, int n )
{
	unsigned int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;

	for ( i=0; i < n; i++ ) {
		val =  iic_recv_byte ( 0 ) << 8;
		val |= iic_recv_byte ( i == n - 1 ? 1 : 0 );
		*buf++ = val;
	}

	iic_stop();

	return 0;
}

/* 8 bit read.
 * Send the address and see if we get an ACK.
 * ripped out of iic_read();
 */
void
iic_diag ( int addr, int reg )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) {
	    printf ( " Oops (addr) !!\n" );
	    return;
	}
	if ( iic_send_byte_m ( reg, "reg" ) ) {
	    printf ( " Oops (reg) !!\n" );
	    return;
	}
	iic_stop();
	printf ( " OK !!\n" );
}

/* 8 bit read */
int ICACHE_FLASH_ATTR
iic_read ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return -1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return -1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return -1;
	rv = iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}

/* 8 bit read */
int ICACHE_FLASH_ATTR
iic_readx ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return -1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return -1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return -1;

	// rv = iic_recv_byte ( 1 );
	rv = iic_readbx();
	iic_setAck ( 1 );

	iic_stop();

	return rv;
}

/* read a 2 byte (short) object from
 * two consecutive i2c registers.
 * (or in some devices, a single 16 bit register)
 */
int ICACHE_FLASH_ATTR
iic_read_16 ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return -1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return -1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return -1;
	rv =  iic_recv_byte ( 0 ) << 8;
	rv |= iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}


/* read an array of bytes (8 bit objects) from
 * consecutive i2c registers.
 */
int ICACHE_FLASH_ATTR
iic_read_n ( int addr, int reg, unsigned char *buf, int n )
{
	unsigned int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;
	for ( i=0; i < n; i++ ) {
		*buf++ = iic_recv_byte ( i == n - 1 ? 1 : 0 );
	}
	iic_stop();

	return 0;
}

/* read an array of 2 byte (short) objects from
 * consecutive i2c registers.
 *  - note that i2c devices just read out consecutive registers
 *  until you send a nack
 */
int ICACHE_FLASH_ATTR
iic_read_16n ( int addr, int reg, unsigned short *buf, int n )
{
	int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;
	for ( i=0; i < n; i++ ) {
		val =  iic_recv_byte ( 0 ) << 8;
		val |= iic_recv_byte ( i == n - 1 ? 1 : 0 );
		*buf++ = val;
	}
	iic_stop();

	return 0;
}

/* read a 2 byte (short) object from
 * two consecutive i2c registers.
 */
int ICACHE_FLASH_ATTR
iic_read_24 ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return -1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return -1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return -1;
	rv =  iic_recv_byte ( 0 ) << 16;
	rv |=  iic_recv_byte ( 0 ) << 8;
	rv |= iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}

int ICACHE_FLASH_ATTR
iic_write ( int addr, int reg, int val )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	if ( iic_send_byte_m ( val, "val" ) ) return 1;
	iic_stop();

	return 0;
}

int ICACHE_FLASH_ATTR
iic_write16 ( int addr, int reg, int val )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	if ( iic_send_byte_m ( val >> 8, "val_h" ) ) return 1;
	if ( iic_send_byte_m ( val & 0xff, "val_l" ) ) return 1;
	iic_stop();

	return 0;
}

/* write a register address with no data
 * (this could be a call to write_raw with a
 *   count of zero)
 */
int ICACHE_FLASH_ATTR
iic_write_nada ( int addr, int reg )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	return 0;
}
#endif /* OLD_HIGH_LEVEL */

/* --------------- */
/* --------------- */
/* --------------- */

#ifdef notdef
int iic_pulses ( void )
{
	for ( ;; ) {
	    iic_dc ( 0, 0 );
	    iic_dc ( 1, 1 );
	}
}
#endif

/* THE END */
