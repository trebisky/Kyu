/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* serial.c
 * simple polled serial port driver
 * see section 19 of the am3359 TRM
 * register description pg 3992 ...
 *
 * Reworked 1-8-2023 to use interrupts, as well as to tidy
 *  up register declarations.
 * The original version was heavily influenced by
 *   the U-boot ns16550 driver.
 */

#include <arch/types.h>
#include "omap_ints.h"

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_DTR    0x01            /* DTR   */
#define UART_MCR_RTS    0x02            /* RTS   */
#define UART_MCR_OUT1   0x04            /* Out 1 */
#define UART_MCR_OUT2   0x08            /* Out 2 */
#define UART_MCR_LOOP   0x10            /* Enable loopback test mode */
#define UART_MCR_AFE    0x20            /* Enable auto-RTS/CTS */

#define UART_MCR_DMA_EN 0x04
#define UART_MCR_TX_DFR 0x08

/*
 * These are the definitions for the Line Control Register
 *
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_WLS_MSK 0x03           /* character length select mask */
#define UART_LCR_WLS_5  0x00            /* 5 bit character length */
#define UART_LCR_WLS_6  0x01            /* 6 bit character length */
#define UART_LCR_WLS_7  0x02            /* 7 bit character length */
#define UART_LCR_WLS_8  0x03            /* 8 bit character length */
#define UART_LCR_STB    0x04            /* # stop Bits, off=1, on=1.5 or 2) */
#define UART_LCR_PEN    0x08            /* Parity eneble */
#define UART_LCR_EPS    0x10            /* Even Parity Select */
#define UART_LCR_STKP   0x20            /* Stick Parity */
#define UART_LCR_SBRK   0x40            /* Set Break */
#define UART_LCR_DLE    0x80            /* Divisor latch enable */

/*
 * These are the definitions for the Line Status Register
 */
#define UART_LSR_DR     0x01            /* Data ready */
#define UART_LSR_OE     0x02            /* Overrun */
#define UART_LSR_PE     0x04            /* Parity error */
#define UART_LSR_FE     0x08            /* Framing error */
#define UART_LSR_BI     0x10            /* Break */
#define UART_LSR_THRE   0x20            /* Xmit holding register empty */
#define UART_LSR_TEMT   0x40            /* Xmitter empty */
#define UART_LSR_ERR    0x80            /* Error */

/*
 * These are the definitions for the FIFO Control Register
 */
#define UART_FCR_FIFO_EN        0x01 /* Fifo enable */
#define UART_FCR_CLEAR_RCVR     0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT     0x04 /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT     0x08 /* For DMA applications */
#define UART_FCR_TRIGGER_MASK   0xC0 /* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1      0x00 /* Mask for trigger set at 1 */
#define UART_FCR_TRIGGER_4      0x40 /* Mask for trigger set at 4 */
#define UART_FCR_TRIGGER_8      0x80 /* Mask for trigger set at 8 */
#define UART_FCR_TRIGGER_14     0xC0 /* Mask for trigger set at 14 */

#define UART_FCR_RXSR           0x02 /* Receiver soft reset */
#define UART_FCR_TXSR           0x04 /* Transmitter soft reset */

/* These are actually 8 bit registers,
 *  followed by 3 bytes of "air".
 * but because we are little endian
 *  accessing them as 32 bit items like this works.
 */
struct uart {
	volatile unsigned int	rhr;	/* 00 - data - thr/tbr */
	volatile unsigned int	ier;	/* 04 - interrupt enable */
	volatile unsigned int	fcr;	/* 08 - fifo control */
	volatile unsigned int	lcr;	/* 0C - line control */
	volatile unsigned int	mcr;	/* 10 - modem control */
	volatile unsigned int	lsr;	/* 14 - line status */
	volatile unsigned int	msr;	/* 18 - modem status */
	volatile unsigned int	spr;	/* 1c - scratch */

	volatile unsigned int	mdr1;	/* 20 - mode 1 */
	volatile unsigned int	mdr2;	/* 24 - mode 2 */
	volatile unsigned int	tfll;	/* 28 - transmit frame length low */
	volatile unsigned int	tflh;	/* 2c - transmit frame length high */
	volatile unsigned int	rfll;	/* 30 - receive frame length low */
	volatile unsigned int	rflh;	/* 34 - receive frame length high */
	volatile unsigned int	bof;	/* 38 - autobaud status */
	volatile unsigned int	uasr;	/* 3c - aux status */
	volatile unsigned int	scr;	/* 40 - suppl control */
	volatile unsigned int	ssr;	/* 44 - suppl status */
	volatile unsigned int	bofl;	/* 48 - bof length */
	/* More registers, see page 4030-4031 */
};

#define thr rhr
#define iir fcr
#define dll rhr
#define dlm ier

#define IER_CTS		0x80
#define IER_RTS		0x40
#define IER_XOFF	0x20
#define IER_SLEEP	0x10
#define IER_MODEM	0x08
#define IER_LINE	0x04
#define IER_THR		0x02
#define IER_RHR		0x01

#define UART_LCR_8N1    0x03
#define UART_LCRVAL UART_LCR_8N1                /* 8 data, 1 stop, no parity */

#define UART_MCRVAL (UART_MCR_DTR | UART_MCR_RTS)

/* Clear & enable FIFOs */
#define UART_FCRVAL (UART_FCR_FIFO_EN | UART_FCR_RXSR | UART_FCR_TXSR)

/* The Uart gets a 48 Mhz clock */
#define UART_CLOCK	48000000

/* Testing this flat enables putc to work from the very start.
 * We rely upon U-Boot initialization for that,
 *  which does work.
 */
static int early = 1;

/* Yes Martha, the chip really has 6 serial ports.
 * This driver only supports the console at this time.
 */
#define NUM_UART        6

static struct serial_softc {
        struct uart *base;
        int irq;
        struct sem *in_sem;
        struct cqueue *in_queue;
        int use_ints;
} serial_soft[NUM_UART];

#define UART0_BASE      ((struct uart *) 0x44E09000)

#define UART1_BASE      ((struct uart *) 0x48022000)
#define UART2_BASE      ((struct uart *) 0x48024000)
#define UART3_BASE      ((struct uart *) 0x481a6000)
#define UART4_BASE      ((struct uart *) 0x481a8000)
#define UART5_BASE      ((struct uart *) 0x481aa000)

#define CONSOLE_UART	0
#define CONSOLE_UART_BASE	UART0_BASE

static void serial_setup ( int, int );

/* You can bet on it that we will be asked for 115200 for the console */
void
serial_init ( int baud )
{
	struct serial_softc *sc = &serial_soft[CONSOLE_UART];

	setup_uart0_mux ();

	sc->use_ints = 0;
	sc->base = (struct uart *) UART0_BASE;
	sc->irq = IRQ_UART0;

	serial_setup ( CONSOLE_UART, baud );

	sc->in_queue = cq_init ( 128 );
        if ( ! sc->in_queue )
            panic ( "Cannot get in-queue for uart" );

	early = 0;
	printf ( "Serial port initialized\n" );
}

/* set the baud rate generator */
/* for 115200 this sets 26 */
static void
set_baud ( struct uart *up, int baud )
{
	int bdiv, div;

	bdiv = 16 * baud;
	div = (UART_CLOCK + bdiv/2) / bdiv;

	up->lcr = UART_LCR_DLE | UART_LCRVAL;
	up->dll = div & 0xff;
	up->dlm = (div >> 8) & 0xff;
	up->lcr = UART_LCRVAL;
}

static void
serial_setup ( int devnum, int baud )
{
	struct serial_softc *sc = &serial_soft[devnum];
	struct uart *up = sc->base;

	/* Wait for transmitter to empty
	 */
	while ( ! (up->lsr & UART_LSR_TEMT) )
		;

	up->ier = 0;

	/* This disables the uart, but the following
	 * odd comments come from U-Boot ---
	 *   Extra -- needed for Omap 335x devices.
	 *   mode select reset TL16C750
	 */
	up->mdr1 = 0x7;

	up->mcr = UART_MCRVAL;
	up->fcr = UART_FCRVAL;

	if ( baud < 0 || baud > 1000000 )
	    panic ( "Crazy baud rate" );

	set_baud ( up, baud );

	/* Uart 16x mode.
	 * This lets us get 115200 with our 48 Mhz clock.
	 */
	up->mdr1 = 0;
}

void
serial_early_putc ( int c )
{
	struct uart *up = CONSOLE_UART_BASE;

	while ( ! (up->lsr & UART_LSR_THRE) )
	    ;

	up->thr = c;

	if ( c == '\n' )
	    serial_early_putc ( '\r' );
}

/* Good old polled output */
void
serial_putc ( int c )
{
	struct serial_softc *sc = &serial_soft[CONSOLE_UART];
	struct uart *up;

	/* Be able to generate output before serial_init is called */
	if ( early )
	    up = CONSOLE_UART_BASE;
	else
	    up = sc->base;

	while ( ! (up->lsr & UART_LSR_THRE) )
	    ;

	up->thr = c;

	if ( c == '\n' )
	    serial_putc ( '\r' );
}

/* Good old polled input.
 * -- never gets called until we are fully initialized.
 */
int
serial_getc ( void )
{
	struct serial_softc *sc = &serial_soft[CONSOLE_UART];
	struct uart *up = sc->base;
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

	/* Simple polled read */
	while ( ! (up->lsr & UART_LSR_DR) )
	    ;
	return up->rhr;
}

void
serial_puts ( char *s )
{
    while ( *s )
	serial_putc ( *s++ );
}

/* We are using printf to debug ourself.
 * It works given that output is polled.
 */
static void
uart_handler ( int devnum )
{
	struct serial_softc *sc = &serial_soft[devnum];
	struct uart *up = sc->base;
	int x;

	// printf ( "Console serial interrupt %d\n", devnum );
	while ( up->lsr & UART_LSR_DR ) {
	    x = up->rhr;
	    cq_add ( sc->in_queue, x );
	}
	/* Would this work right if the above loop
	 * harvested more than 1 character?
	 */
	sem_unblock ( sc->in_sem );
}

/* This is new as of 1-2023
 * The idea is that the console serial port transitions to use
 * interrupts (for input only) after most of Kyu initialization
 * is done.  Truth be known, serial input could just as well be
 * the game from the start, since we never do any input, but we
 * need a variety of things to be initialized before we can
 * switch to using interrupts.
 */
void
console_use_ints ( void )
{
	struct serial_softc *sc = &serial_soft[CONSOLE_UART];
	struct uart *up = sc->base;

	// Must do this here, doing it in "init" is too early
        sc->in_sem = sem_signal_new ( SEM_FIFO );
        // should check

        irq_hookup ( IRQ_UART0, uart_handler, CONSOLE_UART );

	up->ier = IER_RHR;

        sc->use_ints = 1;
}

/* THE END */
