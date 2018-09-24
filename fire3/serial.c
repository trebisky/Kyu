/*
 * Copyright (C) 2018  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * serial.c
 * Simple driver for the s5p6818 console uart
 * Tom Trebisky 8-31-2018
 *
 * The s5p6818 uart is described in chapter 25 of the user manual
 * Also see U-boot sources: drivers/serial/serial_s5p.c
 *
 * This chip has 6 uarts
 */

// typedef volatile unsigned int vu32;
#include <arch/types.h>

struct serial {
	vu32	lcon;		/* 00 */
	vu32	con;		/* 04 */
	vu32	fcon;		/* 08 */
	vu32	mcon;		/* 0c */
	vu32	status;		/* 10 */
	vu32	re_stat;	/* 14 */
	vu32	fstat;		/* 18 */
	vu32	mstat;		/* 1c */
	vu32	txh;		/* 20 */
	vu32	rxh;		/* 24 */
	vu32	brdiv;		/* 28 */
	vu32	fract;		/* 24 */

	vu32	intp;		/* 30 */
	vu32	ints;		/* 34 */
	vu32	intm;		/* 38 */
};

struct serial *uart_base[] = {
	(struct serial *) 0xC00a1000,
	(struct serial *) 0xC00a0000,
	(struct serial *) 0xC00a2000,
	(struct serial *) 0xC00a3000,
	(struct serial *) 0xC006d000,
	(struct serial *) 0xC006f000
};

#define BAUD_115200    0

#define CONSOLE_INDEX	0

#define	TX_FIFO_FULL	0x01000000
#define	RX_FIFO_COUNT	0x0000007f

void
uart_init ( int uart, int baud )
{
 /* XXX
 * This is a start of my own driver, but it simply inherits
 *  the initialization done by bl1 and U-boot.
 */
}

void
uart_putc ( int uart, int c )
{
	struct serial *up = uart_base[uart];

	while ( up->fstat & TX_FIFO_FULL )
	    ;

	up->txh = c;

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
	struct serial *up = uart_base[uart];

	while ( ! (up->fstat & RX_FIFO_COUNT) )
                ;

	return up->rxh & 0xff;
}

/* These are the "standard" entry points used for the Kyu console */

void
serial_init ( int baud )
{
	uart_init ( CONSOLE_INDEX, baud );
}

void
serial_putc ( char c )
{
	uart_putc ( CONSOLE_INDEX, c );
}

void
serial_puts ( char *s )
{
	uart_puts ( CONSOLE_INDEX, s );
}

int
serial_getc ( void )
{
	return uart_getc ( CONSOLE_INDEX );
}

/* THE END */
