/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * serial.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  12-22-2016
 * Tom Trebisky  1/19/2017
 * Tom Trebisky  5/28/2018
 *
 * serial.c
 * super simple "driver" for the H3 uart.
 *
 * These are supposedly 16450/16550 compatible and could use standard drivers.
 *
 * The H3 chip has 5 uarts, the first 4 are on handy IO pins on the Orange Pi.
 * then a "special" number 5 that we so far ignore.
 * The "uart_" entry points in this driver are the general use ones.
 * The "serial_" entry points at the end of this file support the Kyu console.
 *
 * Uart 0 has a special 3 pin header of its own.
 * Uart 1 is on pins 38 and 40 (Tx,Rx) of the 40 pin IO connector (PG6, PG7)
 * Uart 2 is on pins 13 and 11 (Tx,Rx) of the 40 pin IO connector (PA0, PA1)
 * Uart 3 is on pins  8 and 10 (Tx,Rx) of the 40 pin IO connector (PA13, PA14)
 *
 * All of these were tested with just 3 wires (Tx,Rx,Gnd) on 5-29-2018
 */
#include <arch/types.h>
#include "h3_ints.h"

#define NUM_UART	4

#define UART0_BASE	0x01C28000
#define UART1_BASE	0x01C28400
#define UART2_BASE	0x01C28800
#define UART3_BASE	0x01C28C00

#define UART_R_BASE	0x01F02800	/* special */

struct h3_uart {
	vu32 data;	/* 00 - Rx/Tx data */
	vu32 ier;	/* 04 - interrupt enables */
	vu32 iir;	/* 08 - interrupt ID / FIFO control */
	vu32 lcr;	/* 0c - line control */
	vu32 mcr;	/* 10 - modem control */
	vu32 lsr;	/* 14 - line status */
	vu32 msr;	/* 18 - modem status */
};

static struct h3_uart * uart_base[] = {
    (struct h3_uart *) UART0_BASE,
    (struct h3_uart *) UART1_BASE,
    (struct h3_uart *) UART2_BASE,
    (struct h3_uart *) UART3_BASE,
    (struct h3_uart *) UART_R_BASE
};

#define divisor_msb	ier
#define divisor_lsb	data

/* It looks like the basic clock is 24 Mhz
 * We need 16 clocks per character.
 */
#define BAUD_115200   13 /* 24 * 1000 * 1000 / 16 / 115200 = 13 */
#define BAUD_38400    39 /* 24 * 1000 * 1000 / 16 / 38400 = 39 */
#define BAUD_19200    78 /* 24 * 1000 * 1000 / 16 / 19200 = 78 */
#define BAUD_9600    156 /* 24 * 1000 * 1000 / 16 / 9600 = 156 */


/* bits in the lsr */
#define RX_READY	0x01
#define TX_READY	0x40
#define TX_EMPTY	0x80

/* bits in the ier */
#define IE_RDA		0x01	/* Rx data available */
#define IE_TXE		0x02	/* Tx register empty */
#define IE_LS		0x04	/* Line status */
#define IE_MS		0x08	/* Modem status */
#define IE_THRE		0x80	/* Modem status */

#define LCR_DATA_5	0x00	/* 5 data bits */
#define LCR_DATA_6	0x01	/* 6 data bits */
#define LCR_DATA_7	0x02	/* 7 data bits */
#define LCR_DATA_8	0x03	/* 8 data bits */

#define LCR_STOP	0x04	/* extra (2) stop bits, else: 1 */
#define LCR_PEN		0x08	/* parity enable */

#define LCR_EVEN	0x10	/* even parity */
#define LCR_STICK	0x20	/* stick parity */
#define LCR_BREAK	0x40

#define LCR_DLAB	0x80	/* divisor latch access bit */

/* 8 bits, no parity, 1 stop bit */
#define LCR_SETUP	LCR_DATA_8

void uart_gpio_init ( int );
// void uart_clock_init ( int );

/* XXX - Ignores baud rate argument */
void
uart_init ( int uart, int baud )
{
	struct h3_uart *up = uart_base[uart];

	uart_gpio_init ( uart );
	// uart_clock_init ( uart );

	up->ier = 0;
	up->lcr = LCR_DLAB;

	up->divisor_msb = 0;
	up->divisor_lsb = BAUD_115200;

	up->lcr = LCR_SETUP;

	// No sense if we aren't using interrupts.
	// up->ier = IE_RDA | IE_TXE;
}

// Polling loops like this could use a timeout maybe.
void
uart_putc ( int uart, char c )
{
	struct h3_uart *up = uart_base[uart];

	while ( !(up->lsr & TX_READY) )
	    ;
	up->data = c;

	if ( c == '\n' )
	    uart_putc ( uart, '\r' );
}

void
uart_puts ( int uart, char *s )
{
	while ( *s ) {
	    if (*s == '\n')
		uart_putc ( uart, '\r' );
	    uart_putc ( uart, *s++ );
	}
}

// Polling loops like this could use a timeout maybe.
int
uart_getc ( int uart )
{
	struct h3_uart *up = uart_base[uart];

	while ( ! (up->lsr & RX_READY) )
	    ;
	return up->data;
}

/* These are the "standard" entry points used for the Kyu console */

/* Called from board.c */
void
serial_init ( int baud )
{
	uart_init ( 0, baud );
}

void
serial_putc ( char c )
{
	uart_putc ( 0, c );
}

void
serial_puts ( char *s )
{
	uart_puts ( 0, s );
}

int
serial_getc ( void )
{
	return uart_getc ( 0 );
}

/* Code below added for interrupt experiment, 7-29-2020 */

static struct serial_softc {
        struct h3_uart *base;
        int irq;
} serial_soft[NUM_UART];

static void
serial_handler ( int devnum )
{
        struct serial_softc *sc = &serial_soft[devnum];
        struct h3_uart *base = sc->base;
	int x;

	/* poll data ready */
	while ( base->lsr & 0x1 ) {
	    x = base->data;
	    putchar ( x & 0x7f );
	}

	/* Disable any more interrupts.
	 * OK for some testing,
	 * but needs to be more fancy in the future
	 */
	// base->ier &= ~(IE_TXE | IE_THRE);
	base->ier &= IE_RDA;
	// printf ( " ** Uart interrupt, lsr = %08x\n", base->lsr );
}

/* This series of routines are for
 * some quick testing to see if we can get UART interrupts,
 * we can simply enable TXE and we do get interrupts.
 * We can also nicely listen via interrupts to a port (works great)
 */

static void
serial_listen ( int devnum )
{
        struct serial_softc *sc = &serial_soft[devnum];
        struct h3_uart *base = sc->base;

	base->ier = IE_RDA;
}

static void
aux_uart_init ( int devnum, int baud )
{
	struct h3_uart *up = uart_base[devnum];

	up->ier = 0;
	up->lcr = LCR_DLAB;

	up->divisor_msb = 0;
	up->divisor_lsb = baud;

	up->lcr = LCR_SETUP;

	// up->ier = IE_RDA | IE_TXE;
	up->ier = IE_TXE;
}


static void
serial_setup ( int devnum, int irq, int baud )
{
        struct serial_softc *sc = &serial_soft[devnum];

	printf ( "UART irq %d for device %d hookup\n", irq, devnum );
        irq_hookup ( irq, serial_handler, devnum );

	sc->base = uart_base[devnum];
	sc->irq = irq;

	aux_uart_init ( devnum, baud );
	uart_gpio_init ( devnum );
}

/* Called from board.c */
void
serial_aux_init ( void )
{
        // serial_setup ( 0, IRQ_UART0, BAUD_115200 );
        serial_setup ( 1, IRQ_UART1, BAUD_9600 );
        serial_setup ( 2, IRQ_UART2, BAUD_9600 );
        serial_setup ( 3, IRQ_UART3, BAUD_9600 );

	serial_listen ( 1 );
}

#ifdef notdef
 ## 
 ## /* OLD */
 ## #define UART_BASE	((struct h3_uart *) UART0_BASE)
 ## 
 ## int
 ## serial_rx_status ( void )
 ## {
 ## 	struct h3_uart *up = UART_BASE;
 ## 
 ## 	return up->lsr & RX_READY;
 ## }
 ## 
 ## int
 ## serial_read ( void )
 ## {
 ## 	struct h3_uart *up = UART_BASE;
 ## 
 ## 	return up->data;
 ## }
 ## 
 ## void
 ## serial_check ( int num )
 ## {
 ## 	struct h3_uart *up = UART_BASE;
 ## 
 ## 	printf ( " Uart: lsr/ier/iir %02x %02x %02x  %d\n", up->lsr, up->ier, up->iir, num );
 ## }
#endif

/* THE END */
