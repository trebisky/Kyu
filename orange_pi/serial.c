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
 */

/* The H3 chip has 5 uarts, the first 4,
 * then a "special" number 5 that we so far ignore.
 */
#define UART0_BASE	0x01C28000
#define UART1_BASE	0x01C28400
#define UART2_BASE	0x01C28800
#define UART3_BASE	0x01C28C00

#define UART_R_BASE	0x01F02800	/* special */

#define UART_BASE	((struct h3_uart *) UART0_BASE)

struct h3_uart {
	volatile unsigned int data;	/* 00 - Rx/Tx data */
	volatile unsigned int ier;	/* 04 - interrupt enables */
	volatile unsigned int iir;	/* 08 - interrupt ID / FIFO control */
	volatile unsigned int lcr;	/* 0c - line control */
	volatile unsigned int mcr;	/* 10 - modem control */
	volatile unsigned int lsr;	/* 14 - line status */
	volatile unsigned int msr;	/* 18 - modem status */
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
#define BAUD_115200    (0xD) /* 24 * 1000 * 1000 / 16 / 115200 = 13 */

/* bits in the lsr */
#define RX_READY	0x01
#define TX_READY	0x40
#define TX_EMPTY	0x80

/* bits in the ier */
#define IE_RDA		0x01	/* Rx data available */
#define IE_TXE		0x02	/* Tx register empty */
#define IE_LS		0x04	/* Line status */
#define IE_MS		0x08	/* Modem status */


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
void uart_clock_init ( int );

/* XXX - Ignores baud rate argument */
void
uart_init ( int uart, int baud )
{
	// struct h3_uart *up = UART_BASE;
	struct h3_uart *up = uart_base[uart];

	uart_gpio_init ( uart );
	uart_clock_init ( uart );

	up->ier = 0;
	up->lcr = LCR_DLAB;

	up->divisor_msb = 0;
	up->divisor_lsb = BAUD_115200;

	up->lcr = LCR_SETUP;

	up->ier = IE_RDA | IE_TXE;
}

void
uart_putc ( int uart, char c )
{
	// struct h3_uart *up = UART_BASE;
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

int
uart_getc ( int uart )
{
	// struct h3_uart *up = UART_BASE;
	struct h3_uart *up = uart_base[uart];

	while ( ! (up->lsr & RX_READY) )
	    ;
	return up->data;
}

/* These are the "standard" entry points used for the Kyu console */

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


#ifdef notdef
int
serial_rx_status ( void )
{
	struct h3_uart *up = UART_BASE;

	return up->lsr & RX_READY;
}

int
serial_read ( void )
{
	struct h3_uart *up = UART_BASE;

	return up->data;
}

void
serial_check ( int num )
{
	struct h3_uart *up = UART_BASE;

	printf ( " Uart: lsr/ier/iir %02x %02x %02x  %d\n", up->lsr, up->ier, up->iir, num );
}
#endif


/* THE END */
