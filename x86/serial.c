/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* serial.c
 * $Id: serial.c,v 1.8 2005/04/13 21:09:26 tom Exp $
 * skidoo serial port driver.
 * T. Trebisky 3/24/2002
 */

#include "skidoo.h"
#include "intel.h"
#include "thread.h"
#include "sklib.h"

void sio_init ( void );
void sio_baud ( int, int );
void sio_putc ( int, int );
int  sio_getc ( int );

static void	sio_initialize ( int );
static void	sio_probe ( int );

#define SIO_0_BASE	0x3f8
#define SIO_1_BASE	0x2f8

#define SIO_0_IRQ	4
#define SIO_1_IRQ	3

#define NUM_SIO		2

struct sio_stuff {
	int base;
	int run;
	int crmod;
	int echo;
	int in_int;
	int out_int;
	struct cqueue *in_queue;
	struct cqueue *out_queue;
	struct sem *in_sem;
	struct sem *out_sem;
};

static struct sio_stuff sio_info[NUM_SIO];

/* ------------------------------------- */

/* Here are the registers in the NS16450/NS16550 */

#define DATA	0	/* xf8 - data */
#define IER	1	/* xf9 - interrupt enable */
#define ISR	2	/* xfa - interrupt status r/o */
#define LCR	3	/* xfb - line control */
#define MCR	4	/* xfc - modem control */
#define LSR	5	/* xfd - line status */
#define MSR	6	/* xfe - modem status */
#define SCRATCH	7	/* xff - scratch */

#define DLL	0	/* xf8 - divisor latch low */
#define DLH	1	/* xf8 - divisor latch high */
#define FIFO	2	/* xf9 - fifo control (extended features) */

/* ------------------------------------- */

/* bits in the IER */
#define IE_MS		0x08	/* Modem status change */
#define IE_LS		0x04	/* Line status change */
#define IE_TX		0x02	/* Xmit buffer empty */
#define IE_RX		0x01	/* Rcv buffer full */

/* bits in the ISR */
#define IS_IP		0x01	/* NO interrupt pending */
#define IS_MS		0x00	/* Modem status change */
#define IS_TX		0x02	/* Xmit buffer empty */
#define IS_RX		0x04	/* Rcv buffer full */
#define IS_LS		0x06	/* Line status change */
#define IS_MASK		0x06	/* only 2 bits */

/* bits in the LCR */
#define LC_DLAB		0x80	/* Divisor latch access */
#define LC_BREAK	0x40	/* set break */
#define LC_PARST	0x20	/* stick parity */
#define LC_PAREV	0x10	/* even parity */
#define LC_PARENA	0x08	/* parity enable */
#define LC_STOP		0x04	/* stop bits */
#define LC_8BIT		0x03	/* 8 bits of data */
#define LC_7BIT		0x02	/* 7 bits of data */
#define LC_6BIT		0x01	/* 6 bits of data */
#define LC_5BIT		0x00	/* 5 bits of data */

/* bits in the MCR */
#define MC_LOOP		0x10	/* loopback */
#define MC_OUT2		0x08	/* OUT 2 */
#define MC_OUT1		0x04	/* OUT 1 */
#define MC_RTS		0x02	/* RTS */
#define MC_DTR		0x01	/* DTR */

/* bits in the MSR */
#define MS_DCD		0x80
#define MS_RI		0x40
#define MS_DSR		0x20
#define MS_CTS		0x10
#define MS_DDCD		0x08	/* delta DCD */
#define MS_TERI		0x04	/* Trailing edge RI */
#define MS_DCTS		0x02	/* delta CTS */

/* bits in the LSR */
#define LS_RX		0x01	/* receive data ready */
#define LS_OE		0x02	/* overrun error */
#define LS_PE		0x04	/* parity error */
#define LS_FE		0x08	/* framing error */
#define LS_BRK		0x10	/* break ! */
#define LS_TX		0x20	/* Xmit buffer empty */
#define LS_TXE		0x40	/* Xmit buffer and shift both empty */
#define LS_RFE		0x80	/* receive fifo error (550 only) */

/* bits in the FIFO control register  */
#define FC_ENA		0x01	/* FIFOs enabled */
#define FC_RRX		0x02	/* Rx FIFO reset */
#define FC_RTX		0x04	/* Tx FIFO reset */
#define FC_DMA		0x08
/* last 2 bits are char threshold to flag read data */

void
sio_init ( void )
{
	/* For some weird reason, the printf statements
	 * in here lock up keyboard interrupts.  this is
	 * somehow involved with sending more than one newline.
	 * This only happens if printf is called before setting
	 * up the interrupt system.  I don't really need to
	 * probe the exact UART type, since my driver doesn't
	 * use any fancy modes, so heck with it.
	 */

#ifdef notyet
	sio_probe ( 0 );
	sio_probe ( 1 );
#endif

	sio_initialize ( 0 );
	sio_initialize ( 1 );
}

/* This sure as heck better be reentrant
 * at interrupt level !
 */
void
sio_interrupt ( int who )
{
	struct sio_stuff *sp = &sio_info[who];
	int base = sp->base;
	int s;
	int c;

	s = inb_p ( base+ISR );
	switch ( s & IS_MASK ) {
	    case IS_RX:
		sp->in_int++;
		c = inb_p ( base+DATA );
		/*
		printf ( "SIO Rx interrupt on %d, %02x: %02x %c\n",
		    who, s, c, c );
		*/
		cq_add ( sp->in_queue, c & 0x7f );
		cpu_signal ( sp->in_sem );
		break;

	    case IS_TX:
		sp->out_int++;
		/*
		printf ( "SIO Tx interrupt on %d, %02x: ", who, s );
		*/
		if ( cq_count ( sp->out_queue ) ) {
		    c = cq_remove ( sp->out_queue );
		    cpu_signal ( sp->out_sem );
		    outb_p ( c, base+DATA );
		    /*
		    printf ( "%02x %c\n", c, c );
		    */
		} else {
		    outb_p ( inb_p ( base+IER ) & ~IE_TX, base+IER );
		    sp->run = 0;
		    /*
		    printf ( "DONE\n" );
		    */
		}
		break;

	    case IS_LS:
	    case IS_MS:
		printf ( "SIO interrupt on %d, %02x\n", who, s );
		break;
	}
}

void
sio_show ( int who )
{
	struct sio_stuff *sp;

	sp = &sio_info[who];

	printf ( "Sio channel %d at %04x\n", who, sp->base );
	printf ( " tX interrupts: %5d\n", sp->out_int );
	printf ( " rX interrupts: %5d\n", sp->in_int );
	printf ( " rX drops:      %5d\n", cq_toss(sp->in_queue) );
}

static void
sio_initialize ( int who )
{
	struct sio_stuff *sp;
	int base;

	sp = &sio_info[who];
	sp->base = base = who ? SIO_1_BASE : SIO_0_BASE;

	sp->in_sem = cpu_new ();
	if ( ! sp->in_sem )
	    panic ( "Cannot get sio in semaphore" );

	sp->out_sem = cpu_new ();
	if ( ! sp->out_sem )
	    panic ( "Cannot get sio out semaphore" );

	sp->in_queue = cq_init ( 128 );
	if ( ! sp->in_queue )
	    panic ( "Cannot get sio in queue" );

	sp->out_queue = cq_init ( 128 );
	if ( ! sp->out_queue )
	    panic ( "Cannot get sio out queue" );
	sp->crmod = 1;
	sp->echo = 0;
	sp->in_int = 0;
	sp->out_int = 0;

	/*
	sio_baud ( who, CONSOLE_BAUD );
	*/
	sio_baud ( who, 38400 );

	outb_p ( LC_8BIT, base+LCR );
        outb_p ( IE_RX, base+IER );    /* Rx interrupts */

	/* Setting OUT2 activates intermal DCD bit */
        outb_p ( MC_OUT2|MC_RTS|MC_DTR, base+MCR );    /* DTR, RTS */

#ifdef notdef
	irq_hookup ( SIO_0_IRQ, sio0_int );
	irq_hookup ( SIO_1_IRQ, sio1_int );
	pic_enable ( SIO_0_IRQ );
	pic_enable ( SIO_1_IRQ );
#endif

	irq_hookup ( SIO_0_IRQ, sio_interrupt, 0 );
	irq_hookup ( SIO_1_IRQ, sio_interrupt, 1 );

	outb_p ( FC_RRX|FC_RTX, base+FIFO );

	/* read and flush a character */
        (void) inb_p ( base+DATA );

	outb_p ( 0, base+FIFO );

	/*
	outb_p ( inb_p ( base+IER ) | IE_TX, base+IER );
	sp->run = 1;
	*/
	sp->run = 0;
}

/* This seems to be the UART clock on your
 * garden variety PC.  It gives dead accurate rates
 * from 300 to 38400 baud,
 * and you can get 56000 with a 2.96 percent error
 * (which seems to work and is "conventional")
 */
#define SIO_CRYSTAL	1843200

void
sio_baud ( int who, int rate )
{
	int val = SIO_CRYSTAL / ( 16 * rate);
	int base = who ? SIO_1_BASE : SIO_0_BASE;

	outb_p ( inb_p ( base+LCR ) | LC_DLAB, base+LCR );

	outb_p ( val & 0xff, base+DLL );
	outb_p ( val >> 8,   base+DLH );

	outb_p ( inb_p ( base+LCR ) & ~LC_DLAB, base+LCR );
}

/* XXX goofy interface.
 * When this is set (and it is by default),
 * we convert every output \n to \n\r
 * No effect on input.
 */
void
sio_crmod ( int who, int mode )
{
	struct sio_stuff *sp = &sio_info[who];

	sp->crmod = mode;
}

/* XXX goofy interface */
void
sio_echo ( int who, int mode )
{
	struct sio_stuff *sp = &sio_info[who];

	sp->echo = mode;
}

#ifdef notyet
static void
sio_probe ( int who )
{
	int s0, s1, s2;
	int lcr;
	int base = who ? SIO_1_BASE : SIO_0_BASE;

        outb_p ( IE_RX, base+IER );    /* Rx interrupts */
	lcr = inb_p ( base+LCR );

/* 0xbf is: 1011 1111 = all but break */
#define LC_NORM		0xbf

	/*
	outb_p ( LC_NORM, base+LCR );
	outb_p ( 0, base+FIFO );
	*/
	outb_p ( 0, base+LCR );

	outb_p ( FC_ENA, base+FIFO );
	/* should delay here a while */
	/* OK */

	switch ( inb_p ( base+ISR ) >> 6 ) {
	    case 1:
	    	printf ("Unknown UART chip for sio %x\n", base );
	    case 0:
		s0 = inb_p ( base+SCRATCH );
	    	outb_p ( 0xa5, base+SCRATCH );
		s1 = inb_p ( base+SCRATCH );
	    	outb_p ( 0x5a, base+SCRATCH );
		s2 = inb_p ( base+SCRATCH );
	    	outb_p ( s0, base+SCRATCH );
		if ( s1 != 0xa5 || s2 != 0x5a )
		    printf ("8250 for sio %x\n", base );
		else
		    printf ("NS16450 for sio %x\n", base );
	    case 2:
		printf ("NS16550 for sio %x\n", base );
	    case 3:
		/*
	    	outb_p ( lcr | LC_DLAB, base+LCR );
		*/

		if ( inb_p ( base+ISR ) != 0 ) {
		    printf ("zoot\n" );
		    /*
		    printf ("zoot\n" );
		    */
		}

#ifdef notdef
		if ( inb_p ( base+ISR ) == 0 )
		    printf ("ST16550 for sio %x\n", base );
		else {
		    /*
		    outb_p ( LC_NORM, base+LCR );
		    */
		    if ( inb_p ( base+FIFO ) == 0 )
			printf ("ST16550 for sio %x\n", base );
		    else
			printf ("NS16550A for sio %x\n", base );
		}
#endif

	}

	outb_p ( lcr, base+LCR );
}
#endif

void
sio_puts ( int who, char *s )
{
	int c;

	while ( c = *s++ )
	    sio_putc ( who, c );
}

void
sio_gets ( int who, char *s )
{
	char *p;
	int c;

	for ( p = s;; ) {
	    c = sio_getc ( who );
	    *p++ = c;
	    if ( c == '\n' )
	    	break;
	}
	*p = '\0';
}

/*
#define SIO_CV_READ
#define SIO_POLL_WRITE
*/
#define SIO_CV_READ
#define SIO_CV_WRITE

#ifdef SIO_POLL_READ

int sio_check ( int who ) { return 0; }

int
sio_getc ( int who )
{
	int base = who ? SIO_1_BASE : SIO_0_BASE;

	while ( ! (inb_p ( base+LSR ) & LS_RX) )
	    ;
	return inb_p ( base+DATA );
}
#endif

#ifdef SIO_POLL_WRITE

void
sio_putc ( int who, int ch )
{
	/* XXX - should reference sio_stuff for base
	 */
	int base = who ? SIO_1_BASE : SIO_0_BASE;

	while ( ! (inb_p ( base+LSR ) & LS_TX) )
	    ;
	outb_p ( ch, base+DATA );

	if ( ch == '\n' && sio_info[who].crmod )
	    sio_putc ( who, '\r' );
}

#endif

/* XXX
 * We don't have foolproof I/O handshake here.
 * If someone sends us so many characters that
 * our puny little receive buffer gets overrun,
 * there is not much we can do without some kind
 * of flow control on the serial port.  We can
 * ameliorate this by running whatever task needs
 * to process the characters at a high enough
 * priority, or by building some handshake limiting
 * into higher level protocols (usually we get
 * this by accident and for free).
 *
 * XXX XXX XXX
 * Outgoing, we have full control and should
 * block the writer, but at this point we don't.
 *
 * In either case, characters are just dropped.
 */

#ifdef SIO_CV_READ
int
sio_check ( int who )
{
        return cq_count ( sio_info[who].in_queue );
}

int
sio_getc ( int who )
{
	struct sio_stuff *sp = &sio_info[who];
        int c;

        cpu_enter ();

        for ( ;; ) {
            if ( cq_count ( sp->in_queue ) ) {
                c = cq_remove ( sp->in_queue );
                cpu_leave ();
		if ( sp->echo )
		    sio_putc ( who, c );
                return c;
            }
            cpu_wait ( sp->in_sem );
        }
}
#endif

#ifdef SIO_CV_WRITE
/* This is the interrupt driven code that is normally
 * in use.
 */
void
sio_putc ( int who, int ch )
{
	struct sio_stuff *sp = &sio_info[who];
	int base = sp->base;

	/* XXX - needs to block if queue is full
	 */
	 if ( cq_space ( sp->out_queue ) < 1 )
            cpu_wait ( sp->out_sem );

        cpu_enter_s ();
	cq_add ( sp->out_queue, ch );
	if ( ! sp->run ) {
	    outb_p ( inb_p ( base+IER ) | IE_TX, base+IER );
	    sp->run = 1;
	}
	cpu_leave_s ();

	if ( sp->crmod && ch == '\n' )
	    sio_putc ( who, '\r' );
}

#endif

/* THE END */
