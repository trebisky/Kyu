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
 * Tom Trebisky  12/21/2022
 *
 * This is now shared by the H3 and the H5 code.
 * This was heavily modified 12-21-2022 to allow it to be
 *  interrupt driven.  The console needs to be polled
 *  initially, then switch to using interrupts later.
 *  This has been tested only on the H3 thus far.
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

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

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
	vu32 scr;	/* 1c - scratch register */
	int _pad0[23];
	vu32 stat;	/* 7c - uart status */
	vu32 flvl;	/* 80 - fifo level */
	vu32 rfl;	/* 84 - RFL */
	int _pad1[7];
	vu32 halt;	/* A4 - Tx halt */
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
#define IE_THRE		0x80	/* THRE */

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

#define IIR_MODEM	0x0	/* modem status, clear by reading modem status */
#define IIR_NONE	0x1	/* why isn't "none" zero ? */
#define IIR_TXE		0x2	/* THR empty, clear by reading IIR or write to THR */
#define IIR_RDA		0x4	/* rcvd data available, clear by reading it. */
#define IIR_LINE	0x6	/* line status, clear by reading line status. */
#define IIR_BUSY	0x7	/* busy detect, clear by reading modem status. */

/* 8 bits, no parity, 1 stop bit */
#define LCR_SETUP	LCR_DATA_8

static struct serial_softc {
        struct h3_uart *base;
        int irq;
	struct sem *in_sem;
	struct cqueue *in_queue;
	int use_ints;
} serial_soft[NUM_UART];

void uart_init ( int, int );
void uart_putc ( int, int );
void uart_puts ( int , char * );
void uart_gpio_init ( int );

static void serial_listen ( int devnum );
static void serial_setup ( int devnum, int irq, int baud );
static void aux_uart_init ( int devnum, int baud );

// void uart_clock_init ( int );

/* 12-20-2022 */
/* Hackish for now, just for console */
#define CONSOLE_UART	0
// static struct cqueue *in_queue;
// static struct sem *in_sem;
// static int use_ints = 0;

/* A tricky issue arises with the console and interrupts.
 * We want the console as early as
 * possible for output, but we cannot set the uart up for
 * interrupts until later when a lot of other Kyu initialization
 * has been done.
 * So we need to start up without using interrupts,
 * then switch later.
 */

/* These are the "standard" entry points used for the Kyu console */

/* Called from board.c */
void
serial_init ( int baud )
{
	// uart_init ( 0, baud );
	uart_init ( 0, BAUD_115200 );
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

/* You find out what the interrupt is all about by reading
 * the IIR register.  
 * The first time I set up this handler, the IIR_BUSY was
 * set (even with ier=0) and I had to learn to clear it by
 * reading the status register.  It is set because
 * "the master tried to write to the LCR while the Uart is busy"
 * and this is some kind of 16550 non compatibility thing.
 * It makes little or no sense to me, but there you have it.
 */
static void
uart_handler ( int devnum )
{
        struct serial_softc *sc = &serial_soft[devnum];
        struct h3_uart *up = sc->base;
	int x;
	int iir;
	int stat;

	iir = up->iir & 0xf;

	// printf ( "Uart IIR = %x\n", iir );

	// Verify my structure
	// printf ( "Uart status loc = %x\n", &up->stat );
	// printf ( "Uart halt loc = %x\n", &up->halt );

	if ( iir == IIR_BUSY ) {
	    stat = up->stat;
	    printf ( "Uart status = %x\n", stat );
	    return;
	}

	/* I see the LSR at 0x21 whenever I get this.
	 * I do get one character per interrupt when
	 * watching the console (no big surprise).
	 * The 0x20 is the TXEmpty bit
	 */
	if ( iir == IIR_RDA ) {
	    // printf ( "Uart LSR = %x\n", up->lsr );
	    while ( up->lsr & 0x1 ) {
		x = up->data;
		// printf ( "Uart data = %x\n", x );
		// putchar ( x & 0x7f );
		cq_add ( sc->in_queue, x );
	    }
	    sem_unblock ( sc->in_sem );
	}
}

void
uart_init ( int uart, int baud )
{
        struct serial_softc *sc = &serial_soft[uart];
        // struct h3_uart *up = sc->base;
	struct h3_uart *up;

	sc->base = uart_base[uart];
	up = sc->base;

	sc->use_ints = 0;

	sc->in_queue = cq_init ( 128 );
        if ( ! sc->in_queue )
            panic ( "Cannot get in-queue for uart" );

	uart_gpio_init ( uart );
	// uart_clock_init ( uart );

	up->ier = 0;
	up->lcr = LCR_DLAB;

	up->divisor_msb = 0;
	up->divisor_lsb = baud;

	up->lcr = LCR_SETUP;
}

/* XXX  12-20-2022 */
void
console_use_ints ( void )
{
        struct serial_softc *sc = &serial_soft[CONSOLE_UART];
        struct h3_uart *up = sc->base;

	sc->in_sem = sem_signal_new ( SEM_FIFO );
	// should check

        irq_hookup ( IRQ_UART0, uart_handler, CONSOLE_UART );

	// up->ier = 0;
	// up->ier = IE_RDA | IE_TXE;
	up->ier = IE_RDA;

	sc->use_ints = 1;
}

static void
serial_setup ( int devnum, int irq, int baud )
{
        struct serial_softc *sc = &serial_soft[devnum];

	printf ( "UART irq %d for device %d hookup\n", irq, devnum );
        irq_hookup ( irq, uart_handler, devnum );

	sc->base = uart_base[devnum];
	sc->irq = irq;

	aux_uart_init ( devnum, baud );
	uart_gpio_init ( devnum );
}

// Enable serial interrupts
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
        struct serial_softc *sc = &serial_soft[devnum];
	struct h3_uart *up = uart_base[devnum];

	up->ier = 0;
	up->lcr = LCR_DLAB;

	up->divisor_msb = 0;
	up->divisor_lsb = baud;

	up->lcr = LCR_SETUP;

	// up->ier = IE_RDA | IE_TXE;
	up->ier = IE_TXE;
}

/* ================================================================================ */
/* ================================================================================ */

// Polling loops like this could use a timeout maybe.
void
uart_putc ( int uart, int c )
{
        struct serial_softc *sc = &serial_soft[uart];
        struct h3_uart *up = sc->base;

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
// Or maybe not if we expect the console to sit here.
int
uart_getc ( int uart )
{
        struct serial_softc *sc = &serial_soft[uart];
        struct h3_uart *up = sc->base;
	int c;

	if ( sc->use_ints ) {
	    for ( ;; ) {
		// lock
		if ( cq_count ( sc->in_queue ) ) {
		    c = cq_remove ( sc->in_queue );
		    // unlock
		    return c;
		}
		// unlock
		sem_block ( sc->in_sem );
	    }
	}

	/* This is the tried and true method used by the Kyu console
	 * since time immemorial.  Kyu would spend pretty much all of
	 * its time polling here (rather than in the idle task)
	 */
	while ( ! (up->lsr & RX_READY) )
	    ;
	return up->data;
}

/* THE END */
