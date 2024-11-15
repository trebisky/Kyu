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

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#define BIT(x)	(1<<(x))

/* The TRM keeps the register details in Appendix B
 */
struct zynq_uart {
	vu32	control;
	vu32	mode;
	vu32	ie;
	vu32	id;

	vu32	im;
	vu32	istatus;
	vu32	baud;
	vu32	rto;

	vu32	rfifo;
	vu32	mcontrol;
	vu32	mstatus;
	vu32	cstatus;

	vu32	fifo;
	vu32	baud_div;
	vu32	fc_delay;
	int	__pad1[2];

	vu32	tfifo;

};

#define IRQ_UART0	69
#define IRQ_UART1	82

#define UART0_BASE	((struct zynq_uart *) 0xe0000000)
#define UART1_BASE	((struct zynq_uart *) 0xe0001000)

/* Both the EBAZ and the Antminer S9 use serial port 1 */
#define UART_BASE	UART1_BASE
#define IRQ_UART	IRQ_UART1

/* This is probably true, from a U-boot config file */
#define UART_CLOCK	100000000

/* Bits in the IE register */
#define IE_RXTRIG	BIT(0)
#define IE_RXE		BIT(1)
#define IE_TXE		BIT(3)

/* Bits in the cstatus register */
#define CS_TXFULL	BIT(4)
#define CS_TXEMPTY	BIT(3)
#define CS_RXFULL	BIT(2)
#define CS_RXEMPTY	BIT(1)
#define CS_RXTRIG	BIT(0)

#ifdef notyet
void serial_handler ( int );
#endif

/* XXX - the argument is ignored */
void
serial_init ( int baud )
{
	struct zynq_uart *up = UART_BASE;

	/* We rely on U-Boot to have done initialization,
	 * at least so far, we just make some tweaks here
	 * to let us fool around with interrupts.
	 */

	up->rfifo = 1;

	/* Interrupts */
	// up->ie = IE_RXTRIG;
	// up->ie = IE_RXE | IE_TXE;
	// irq_hookup ( IRQ_UART, serial_handler, 0 );
}

void
serial_putc ( char c )
{
	struct zynq_uart *up = UART_BASE;

	while ( up->cstatus & CS_TXFULL )
	    ;
	up->fifo = c;

	if ( c == '\n' )
		serial_putc ( '\r' );
}

void
serial_puts ( char *s )
{
	while ( *s ) {
	    if (*s == '\n')
		serial_putc('\r');
	    serial_putc(*s++);
	}
}

int
serial_getc ( void )
{
	struct zynq_uart *up = UART_BASE;

	while ( up->cstatus & CS_RXEMPTY )
	    ;
	return up->fifo;
} 

/* XXX */
void
console_use_ints ( void )
{
}

void
uart_early_putc ( int c )
{
}

#ifdef notyet
// Orange Pi
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
#endif


#ifdef notyet
void
serial_handler ( int xxx )
{
	struct zynq_uart *up = UART_BASE;
	int c;

	// printf ( "Uart interrupt %d\n", xxx );

	// ACK the interrupt 
	up->istatus = IE_RXTRIG;

	// disable the interrupt
	// up->id = IE_RXE | IE_TXE;

	c = up->fifo;
	// show_reg ( "Uart interrupt, istatus: ", &up->istatus );
	// printf ( "Received: %x %d\n", c, c );
	c &= 0x7f;
	if ( c < ' ' )
	    c = '.';
	printf ( "Uart interrupt, istatus: %h char = %x %c\n", &up->istatus, c, c );
}
#endif

/* THE END */
