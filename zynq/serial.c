/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 * Copyright (C) 2024  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * serial.c for the Zynq
 *
 * Tom Trebisky  11/14/2024
 *
 */

/* serial.c
 * simple serial driver for the Zynq
 *
 * Currently relies entirely on U-Boot initialization
 *
 * The Zynq 7000 has a 1843 page TRM.
 * Chapter 19 (page 590) covers the uart.
 * Appendix B has register details (page 1773)
 * 
 * Tom Trebisky  1-9-2021
 * Tom Trebisky  11-14-2024
 */

#include <arch/types.h>

#include "board.h"
#include "zynq_ints.h"

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"


// typedef volatile unsigned int vu32;
// typedef unsigned int u32;

// #define IRQ_UART0	69
// #define IRQ_UART1	82

#define BIT(x)	(1<<(x))

#define NUM_UART 2

#ifdef nothere
// Orange Pi
#define BAUD_115200   13 /* 24 * 1000 * 1000 / 16 / 115200 = 13 */
#define BAUD_38400    39 /* 24 * 1000 * 1000 / 16 / 38400 = 39 */
#define BAUD_19200    78 /* 24 * 1000 * 1000 / 16 / 19200 = 78 */
#define BAUD_9600    156 /* 24 * 1000 * 1000 / 16 / 9600 = 156 */
#endif

#define BAUD_115200   0

static int early = 1;

static struct serial_softc {
        struct zynq_uart *base;
        int irq;
		struct sem *in_sem;
		struct cqueue *in_queue;
		int use_ints;
} serial_soft[NUM_UART];

void uart_handler ( int );

void uart_init ( int, int );
void uart_putc ( int, int );
void uart_puts ( int , char * );
// void uart_gpio_init ( int );

static void serial_listen ( int devnum );
static void serial_setup ( int devnum, int irq, int baud );
static void aux_uart_init ( int devnum, int baud );

#define UART0_BASE	((struct zynq_uart *) 0xe0000000)
#define UART1_BASE	((struct zynq_uart *) 0xe0001000)

#if CONSOLE_UART == 0
#define CONSOLE_UART_BASE   UART0_BASE
#else
#define CONSOLE_UART_BASE   UART1_BASE
#endif

static struct zynq_uart * uart_base[] = {
    (struct zynq_uart *) UART0_BASE,
    (struct zynq_uart *) UART1_BASE,
};

/* Called from board.c */
void
serial_init ( int baud )
{
    uart_init ( CONSOLE_UART, baud );
    early = 0;
}

void
serial_putc ( char c )
{
    uart_putc ( CONSOLE_UART, c );
}

void
serial_puts ( char *s )
{
    uart_puts ( CONSOLE_UART, s );
}

int
serial_getc ( void )
{
    return uart_getc ( CONSOLE_UART );
}

/* Called from board.c */
void
serial_aux_init ( void )
{
#if CONSOLE_UART == 0
        serial_setup ( 1, IRQ_UART1, BAUD_115200 );
		serial_listen ( 1 );
#else
        serial_setup ( 0, IRQ_UART0, BAUD_115200 );
		serial_listen ( 0 );
#endif
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
		// uart_gpio_init ( devnum );
}

// Enable serial interrupts
static void
serial_listen ( int devnum )
{
        struct serial_softc *sc = &serial_soft[devnum];
        // struct h3_uart *base = sc->base;

        // base->ier = IE_RDA;
}

/* ============================================================= */


/* The TRM keeps the register details in Appendix B
 */
struct zynq_uart {
	vu32	control;
	vu32	mode;
	vu32	ie;			/* enable ints WO */
	vu32	id;			/* disable ints WO */

	vu32	im;			/* int mask RO */
	vu32	istatus;	/* int status */
	vu32	baud;
	vu32	rto;

	vu32	rfifo;		/* Rx fifo trigger level */
	vu32	mcontrol;
	vu32	mstatus;
	vu32	cstatus;

	vu32	fifo;		/* data here */
	vu32	baud_div;
	vu32	fc_delay;
	int	__pad1[2];

	vu32	tfifo;

};

/* Both the EBAZ and the Antminer S9 use serial port 1 */
#define UART_BASE	UART1_BASE
#define IRQ_UART	IRQ_UART1

/* This is probably true, from a U-boot config file */
#define UART_CLOCK	100000000

/* Bits in the IE register */
#define IE_RXTRIG	BIT(0)
#define IE_RXE		BIT(1)
#define IE_RXFULL	BIT(2)
#define IE_TXE		BIT(3)
#define IE_TXFULL	BIT(4)
#define IE_TXTRIG	BIT(10)
#define IE_TXNFULL	BIT(11)		// nearly full

/* Bits in the cstatus register */
#define CS_TXFULL	BIT(4)
#define CS_TXEMPTY	BIT(3)
#define CS_RXFULL	BIT(2)
#define CS_RXEMPTY	BIT(1)
#define CS_RXTRIG	BIT(0)


/* XXX - the baud argument is ignored */
void
uart_init ( int uart, int baud )
{
	struct serial_softc *sc = &serial_soft[uart];
	struct zynq_uart *up;

	sc->base = uart_base[uart];
	up = sc->base;

	sc->use_ints = 0;

    sc->in_queue = cq_init ( 128 );
        if ( ! sc->in_queue )
            panic ( "Cannot get in-queue for uart" );


	/* We rely on U-Boot to have done initialization,
	 * at least so far, we just make some tweaks here
	 * to let us fool around with interrupts.
	 */

	/* Trigger on one character received */
	up->rfifo = 1;

	/* Interrupts */
	// up->ie = IE_RXTRIG;
	// up->ie = IE_RXE | IE_TXE;
	// irq_hookup ( IRQ_UART, uart_handler, 0 );
}

void
console_use_ints ( void )
{
	struct serial_softc *sc = &serial_soft[CONSOLE_UART];
	struct zynq_uart *up = sc->base;

    sc->in_sem = sem_signal_new ( SEM_FIFO );
    // should check

	/* Trigger on one character received */
	up->rfifo = 1;

	irq_hookup ( IRQ_UART, uart_handler, CONSOLE_UART );

	/* Clear everything in int status register */
	up->istatus = 0xffff;

	/* Enable Rx trigger only */
	up->ie = IE_RXTRIG;

    sc->use_ints = 1;
	// printf ( "Console use ints -- BYPASSED\n" );
}

static void
aux_uart_init ( int devnum, int baud )
{
	struct serial_softc *sc = &serial_soft[devnum];
    struct zynq_uart *up = uart_base[devnum];

	// Orange Pi
    // up->ier = 0;
    // up->lcr = LCR_DLAB;

    // up->divisor_msb = 0;
    // up->divisor_lsb = baud;

    // up->lcr = LCR_SETUP;

    // up->ier = IE_TXE;
}

void
uart_putc ( int uart, int c )
{
	struct serial_softc *sc = &serial_soft[uart];
	struct zynq_uart *up;

    /* Be able to generate output before serial_init is called */
	if ( early )
		up = (struct zynq_uart *) CONSOLE_UART_BASE;
	else
		up = sc->base;

	while ( up->cstatus & CS_TXFULL )
	    ;
	up->fifo = c;

	if ( c == '\n' )
		serial_putc ( '\r' );
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
    struct serial_softc *sc = &serial_soft[uart];
    struct zynq_uart *up = sc->base;
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

	while ( up->cstatus & CS_RXEMPTY )
	    ;
	return up->fifo;
} 

/* Plain old polled output.
 * Allows output during early board initialization
 */
void
uart_early_putc ( int c )
{
	struct zynq_uart *up = UART_BASE;

	while ( up->cstatus & CS_TXFULL )
	    ;
	up->fifo = c;

	if ( c == '\n' )
		serial_putc ( '\r' );
}

/* The argument "uart" is whatever we gave to the hookup */
void
uart_handler ( int uart )
{
    struct serial_softc *sc = &serial_soft[uart];
    struct zynq_uart *up = sc->base;
	int c;

	// I see: Uart interrupt 1
	// printf ( "Uart interrupt %d\n", uart );

	// ACK (clear) the interrupt 
	up->istatus = IE_RXTRIG;

	// disable the interrupt
	// up->id = IE_RXTRIG;

	c = up->fifo;

	// I see:
	//  Uart interrupt, istatus:  e0001014 --> 00000c1a
	//  Uart interrupt, mask:  e0001010 --> 00000001
	// Received: d 13

	// show_reg ( "Uart interrupt, istatus: ", &up->istatus );
	// show_reg ( "Uart interrupt, mask: ", &up->im );
	// printf ( "Received: %x %d\n", c, c );

	cq_add ( sc->in_queue, c );
	sem_unblock ( sc->in_sem );

#ifdef notdef
	c &= 0x7f;
	if ( c < ' ' )
	    c = '.';
	printf ( "Uart interrupt, istatus: %x char = %x %c\n", &up->istatus, c, c );
#endif

}


#ifdef notyet
void
serial_handler ( int xxx )
{
	struct zynq_uart *up = UART_BASE;
	int c;

	// printf ( "Uart interrupt %d\n", xxx );

}
#endif

/* THE END */
