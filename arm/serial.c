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
 * U-boot ns16550 driver is at:
 *  drivers/serial/ns16550.c
 *  include/ns16550.h
 */

#define UART0_BASE      0x44E09000

#define UART1_BASE      0x48022000
#define UART2_BASE      0x48024000
#define UART3_BASE      0x481a6000
#define UART4_BASE      0x481a8000
#define UART5_BASE      0x481aa000

#define UART_BASE	UART0_BASE

/* registers are 4 bytes apart, but only a byte wide.
 * they are read at the address specified
 */

#define getb(a)          (*(volatile unsigned char *)(a))
#define getw(a)          (*(volatile unsigned short *)(a))
#define getl(a)          (*(volatile unsigned int *)(a))

#define putb(v,a)        (*(volatile unsigned char *)(a) = (v))
#define putw(v,a)        (*(volatile unsigned short *)(a) = (v))
#define putl(v,a)        (*(volatile unsigned int *)(a) = (v))

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
#define UART_LCR_BKSE   0x80            /* Bank select enable */
#define UART_LCR_DLAB   0x80            /* Divisor latch access bit */

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

#define UART_MSR_DCD    0x80            /* Data Carrier Detect */
#define UART_MSR_RI     0x40            /* Ring Indicator */
#define UART_MSR_DSR    0x20            /* Data Set Ready */
#define UART_MSR_CTS    0x10            /* Clear to Send */
#define UART_MSR_DDCD   0x08            /* Delta DCD */
#define UART_MSR_TERI   0x04            /* Trailing edge ring indicator */
#define UART_MSR_DDSR   0x02            /* Delta DSR */
#define UART_MSR_DCTS   0x01            /* Delta CTS */

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

/* From include/ns16550.h */

#define UART_REG(x)                                                     \
        unsigned char x;                                                \
        unsigned char postpad_##x[3];

struct NS16550 {
        UART_REG(rbr);          /* 0 */
        UART_REG(ier);          /* 1 */
        UART_REG(fcr);          /* 2 */
        UART_REG(lcr);          /* 3 */
        UART_REG(mcr);          /* 4 */
        UART_REG(lsr);          /* 5 */
        UART_REG(msr);          /* 6 */
        UART_REG(spr);          /* 7 */

        UART_REG(mdr1);         /* 8 */
        UART_REG(reg9);         /* 9 */
        UART_REG(regA);         /* A */
        UART_REG(regB);         /* B */
        UART_REG(regC);         /* C */
        UART_REG(regD);         /* D */
        UART_REG(regE);         /* E */
        UART_REG(uasr);         /* F */
        UART_REG(scr);          /* 10*/
        UART_REG(ssr);          /* 11*/
};

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier

#define UART_LCR_8N1    0x03
#define UART_LCRVAL UART_LCR_8N1                /* 8 data, 1 stop, no parity */

#define UART_MCRVAL (UART_MCR_DTR | UART_MCR_RTS)

/* Clear & enable FIFOs */
#define UART_FCRVAL (UART_FCR_FIFO_EN | UART_FCR_RXSR | UART_FCR_TXSR)

/* From linux/kernel.h
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(                  \
{                                                       \
        typeof(x) __x = x;                              \
        typeof(divisor) __d = divisor;                  \
        (((typeof(x))-1) > 0 ||                         \
         ((typeof(divisor))-1) > 0 || (__x) > 0) ?      \
                (((__x) + ((__d) / 2)) / (__d)) :       \
                (((__x) - ((__d) / 2)) / (__d));        \
}                                                       \
)

#define UART_CLOCK	48000000

static void serial_init_int ( int );

static int
calc_divisor ( int clock, int baudrate)
{
        const unsigned int mode_x_div = 16;

        return DIV_ROUND_CLOSEST(clock, mode_x_div * baudrate);
}

/* set the baud rate generator */
static void
setbrg ( int baud_divisor)
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    putb ( UART_LCR_BKSE | UART_LCRVAL, &com_port->lcr );
    putb ( baud_divisor & 0xff, &com_port->dll );
    putb ( (baud_divisor >> 8) & 0xff, &com_port->dlm );
    putb ( UART_LCRVAL, &com_port->lcr );
}

void
serial_init ( int baud )
{
    int divisor = calc_divisor ( UART_CLOCK, baud );

    setup_uart0_mux ();

    serial_init_int ( divisor );
}

static void
serial_init_int ( int baud_divisor )
{
	struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

#ifdef notdef
	/*
	 * On some OMAP3/OMAP4 devices when UART3 is configured for boot mode
	 * before SPL starts only THRE bit is set. We have to empty the
	 * transmitter before initialization starts.
	 */
	if ( (getb(&com_port->lsr) & (UART_LSR_TEMT | UART_LSR_THRE)) == UART_LSR_THRE) {
	    if (baud_divisor != -1)
		setbrg ( baud_divisor );
	    putb(0, &com_port->mdr1);
	}
#endif

	while ( !(getb(&com_port->lsr) & UART_LSR_TEMT))
		;

	putb ( 0, &com_port->ier );

	/* Extra -- needed for Omap 335x devices */
	putb(0x7, &com_port->mdr1);	/* mode select reset TL16C750*/

	setbrg ( 0 );

	putb (UART_MCRVAL, &com_port->mcr);
	putb (UART_FCRVAL, &com_port->fcr);

	if (baud_divisor != -1)
	    setbrg ( baud_divisor );

	/* Special for Omap3 335x devices */
	/* /16 is proper to hit 115200 with 48MHz */
	putb(0, &com_port->mdr1);
}

void
serial_putc ( int c )
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    while ( (getb(&com_port->lsr) & UART_LSR_THRE) == 0)
	;

    putb(c, &com_port->thr);

    if ( c == '\n' )
	serial_putc ( '\r' );
}

int
serial_getc ( void )
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    while ( (getb(&com_port->lsr) & UART_LSR_DR) == 0)
	;
    return getb(&com_port->rbr);

}

int 
serial_tstc ( void )
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    return getb(&com_port->lsr) & UART_LSR_DR != 0;
}


void
serial_puts ( char *s )
{
    while ( *s )
	serial_putc ( *s++ );
}

/* Interrupt stuff below here, above is polling driver */

char msg[] = "Hello World\r\n";

#define IE_TX	2
#define IE_RX	1

char *next;

/* Enable TH interrupts */
void serial_int_setup ()
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    next = (char *) 0;
}

/* Simple test, queue up the above for output */
void serial_int_test ( void )
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;

    next = msg;
    putb(IE_TX, &com_port->ier);
}

/* Do whatever it takes to Ack an interrupt */
void serial_irqack ( void )
{
}

/* Process whatever should be processed */
void serial_int ( int xxx )
{
    struct NS16550 *com_port = (struct NS16550 *) UART_BASE;
    int c;

    serial_irqack ();

    if ( ! next ) {
	putb(0, &com_port->ier);
	return;
    }
    if ( ! *next ) {
	putb(0, &com_port->ier);
	next = (char *) 0;
	return;
    }

    putb(*next++, &com_port->thr);

#ifdef notdef
    /* Try to output as many chars as possible per interrupt.
     * When I tested this, nothing happened till I typed
     * a character ??!!  Then it worked fine.
     */
    for ( ;; ) {
	if ( (getb(&com_port->lsr) & UART_LSR_DR) == 0)
	    break;
	putb(*next++, &com_port->thr);
	if ( ! *next ) {
	    next = (char *) 0;
	    break;
	}
    }
#endif
}

/* THE END */
