/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* if_ne.c
 * $Id: if_ne.c,v 1.21 2005/04/01 01:17:11 tom Exp $
 *
 * Driver for national DP8390 based ethernet cards.
 *
 * Began 12/3/2002 Using an artisoft AE/2
 * card in ne2000 compatibility mode.
 *
 * I confirmed that linux probes and likes this
 * card as an NE2000 and got a hardware address from it.
 *
 * Lots of stuff learned from the NetBSD-1.4 driver,
 * as well as from linux 2.4.  The rest learned by
 * days spent dorking around with the hardware.
 *
 * First off, the hardware address must be read as
 * 6 words (not bytes) on a 16 bit card.
 * (read 6 bytes on an 8-bit card.
 *
 * The address prom gives 16 bytes.
 *	The first 6 are the hardware address.
 *	The next 8 are zeros.
 *	The last 2 are 0x57 0x57
 *	(this is the ne2000 signature).
 * Apparently bad clones lack the 0x57 0x57 thing.
 *
 * My office AE/2 gives: 00:00:6E:23:57:76
 * My home   AE/2 gives: 00:00:6E:2E:C8:61
 * Another   AE/2 gives: 00:00:6E:23:55:2B
 * My home   AE/1 gives: 00:00:6E:23:F0:6D
 *
 * My gateway 40k gives: 00:00:86:E4:77:82
 *
 * A 16 bit card with a pair of 8k ram chips
 * has 4 regions in the local memory space as follows:
 * 	0-16k	empty except for address prom at start.
 * 	16-32k	ram
 * 	32-48k	empty
 * 	48-64k	ram (alias of lower addressed ram).
 *
 * The second empty region reads back:
 *  0x8000 0x8002 0x8004 0x8006 ...
 *  this is amusing, weird, and pretty much useless.
 *
 * An 8-bit card looks like:
 * 	0-8k	empty except for address prom
 * 	8-16k	ram
 * 	....	lots of aliasing beyond this.
 *
 * I have a "gateway" clone of the 8-bit card.
 * It has 40k of ram (wow!) as follows:
 * 	0-8k	empty except for address prom
 * 	8-16k	ram
 * 	16-24k	empty
 * 	24-64k	ram (first 8k is alias of lower).
 *
 * -----------------------------------------------
 * After some success with the ne2000 jobs,
 * I turn to the HP-lan 27247 cards:
 *
 * One   shows: 08:00:09:13:E6:B3
 * other shows: 08:00:09:13:E6:89
 *
 * TODO
 *	autodetect 8 bit cards.
 */

#include "skidoo.h"
#include "intel.h"
#include "sklib.h"

#define TIMEOUT_SPIN	100000

enum ne_type { NE_2K, NE_HP, NE_WD };

struct ne_driver {
	int io_base;		/* card base */
	int nic;		/* 8390 base */
	int dataport;
	int is_8bit;
	int irq;
	enum ne_type type;

	int start;		/* first page in Rx ring */
	int stop;		/* last page +1 in Rx ring */

	int fe_count;
	int crc_count;
	int miss_count;
	int err_count;
};

static char gateway_phys[] = { 0, 0, 0x86 };
static char hp_phys[] = { 0x08, 0, 0x09 };

struct ne_driver ne_driver_info;

/* ---------------------------------------------- */
/* Board specific registers */

#define NE_DATA		0x10	/* ne2000 data port (not dp8390) */
#define NE_RESET	0x1f	/* ne2000 reset port (not dp8390) */

#define HP_ID		0x07
#define HP_CONFIG	0x08
#define HP_NIC		0x10
#define HP_DATA		0x0C

/* bits in the HP config register.
 * when the run bit is cleared, we get a board reset.
 * when the IRQ bits are zeroed, we get a software
 * triggered interrupt (with isr==0).
 * note that the config register seems to be
 * aliased over ports 0x8, 0x9, 0xA, and 0xB
 * The data port is 16 bit wide, so it covers 0xC and 0xD
 * (it seems aliased at 0xE and 0xF)
 */
#define HP_RUN		0x01	/* enable NIC registers */
#define HP_IRQ		0x0E	/* mask for IRQ setting */
#define HP_DPEN		0x10	/* enable DATAPORT register */

/* ---------------------------------------------- */

/* DP8390 definitions ...
 */

#define ED_CR		0x00	/* Command Register */

/* Page 0 registers */
#define ED_P0_PSTART	0x01	/* Page start (write) */
#define ED_P0_CLDMA0	0x01	/* Current local DMA (read) */
#define ED_P0_PSTOP	0x02	/* Page stop (write) */
#define ED_P0_CLDMA1	0x02	/* Current local DMA (read) */
#define ED_P0_BOUND	0x03	/* Boundary pointer (r/w) */
#define ED_P0_TSR	0x04	/* Xmit status (read) */
#define ED_P0_TPSR	0x04	/* Xmit page start (write) */
#define ED_P0_TBCR0	0x05	/* Xmit byte count low (write) */
#define ED_P0_NCR	0x05	/* number of collisions (read) */
#define ED_P0_TBCR1	0x06	/* Xmit byte count high (write) */
#define ED_P0_FIFO	0x06	/* FIFO (read) */
#define ED_P0_ISR	0x07	/* ISR */
#define ED_P0_RSA0	0x08	/* Remote start address low (write) */
#define ED_P0_CRDMA0	0x08	/* Current remote DMA (read) */
#define ED_P0_RSA1	0x09	/* Remote start address high (write) */
#define ED_P0_CRDMA1	0x09	/* Current remote DMA (read) */
#define ED_P0_RBCR0	0x0A	/* Remote byte count low (write) */
#define ED_P0_RBCR1	0x0B	/* Remote byte count high (write) */
	/* cannot read 0x0a or 0x0b (reserved) */
#define ED_P0_RSR	0x0C	/* Receive status (read) */
#define ED_P0_RCR	0x0C	/* Receive config (write) */
#define ED_P0_TCR	0x0D	/* Xmit config reg (write) */
#define ED_P0_COUNT_FE	0x0D	/* Frame alignment errors (read) */
#define ED_P0_DCR	0x0E	/* Data config reg (write) */
#define ED_P0_COUNT_CRC	0x0E	/* CRC errors (read) */
#define ED_P0_IMR	0x0F	/* Interrupt mask reg (write) */
#define ED_P0_COUNT_MP	0x0F	/* Missed packet errors (read) */

/* Page 1 registers */
#define ED_P1_PHYS	0x01	/* 6 byte hardware address (r/w) */
#define ED_P1_CURR	0x07	/* current page (r/w) */
#define ED_P1_MULT	0x08	/* 8 byte multicast addr (r/w) */

/* Page 2 registers */
#define ED_P2_PSTART	0x01	/* Page start (read) */
#define ED_P2_PSTOP	0x02	/* Page stop (read) */

/* ----------------------------------------------- */
/* ----------------------------------------------- */

/* bits in the command register */
#define ED_CR_STOP	0x01
#define ED_CR_STA	0x02
#define ED_CR_TXP	0x04
#define ED_CR_RD0	0x08	/* remote DMA - remote read */
#define ED_CR_RD1	0x10	/* remote DMA - remote write */
#define ED_CR_RD2	0x18	/* remote DMA - send packet */
#define ED_CR_RDA	0x20	/* remote DMA - abort */
#define ED_CR_P0	0x00	/* page 0 */
#define ED_CR_P1	0x40	/* page 1 */
#define ED_CR_P2	0x80	/* page 2 */
#define ED_CR_P3	0xC0	/* page 3 (extended chips only) */
#define ED_CR_PM	0xC0	/* mask of page bits */

/* bits in the DCR register */
#define ED_DCR_WTS	0x01	/* word transfers when set*/
#define ED_DCR_BOS	0x02	/* byte order (0 for x86) */
#define ED_DCR_LAS	0x04	/* DMA mode (1 = single 32 bit) */
#define ED_DCR_LS	0x08	/* loopback when 0 */
#define ED_DCR_ARM	0x10	/* auto remote */
#define ED_DCR_F1	0x00	/* FIFO threshold 1 word*/
#define ED_DCR_F2	0x20	/* FIFO threshold 1 word*/
#define ED_DCR_F4	0x40	/* FIFO threshold 1 word*/
#define ED_DCR_F6	0x60	/* FIFO threshold 1 word*/

/* bits in the ISR register */
#define ED_ISR_PXR	0x01	/* packet received */
#define ED_ISR_PTX	0x02	/* packet transmitted OK */
#define ED_ISR_RXE	0x04	/* Receive error */
#define ED_ISR_TXE	0x08	/* Xmit error */
#define ED_ISR_OVW	0x10	/* ring buffer overwrite */
#define ED_ISR_CNT	0x20	/* counter overflow */
#define ED_ISR_RDC	0x40	/* remote DMA complete */
#define ED_ISR_RST	0x80	/* set in reset state */

/* bits in the RCR register */
#define ED_RCR_SEP	0x01	/* save errored packets */
#define ED_RCR_RUNT	0x02	/* accept runts */
#define ED_RCR_BROAD	0x04	/* accept broadcast */
#define ED_RCR_MULT	0x08	/* accept multicast */
#define ED_RCR_PRO	0x10	/* promiscuous */
#define ED_RCR_MON	0x20	/* monitor mode */

/* bits in the TCR register */
/* note that loopback also involves 0x08 in the DCR */
#define ED_TCR_NOCRC	0x01	/* inhibit CRC */
#define ED_TCR_LB0	0x00	/* no loopback (normal) */
#define ED_TCR_LB1	0x02	/* loopback 1 */
#define ED_TCR_LB2	0x04	/* loopback 2 */
#define ED_TCR_LB3	0x06	/* loopback 3 */
#define ED_TCR_ATD	0x08	/* autoxmit disable */
#define ED_TCR_BKOFF	0x10	/* special backoff mode */

/* bits in the RSR register (and packet header) */
#define ED_RSR_PXR	0x01	/* proper packet received */
#define ED_RSR_CRC	0x02	/* CRC error */
#define ED_RSR_FE	0x04	/* Framing error */
#define ED_RSR_OV	0x08	/* FIFO overrun */

struct ring_hdr {
	unsigned char status;
	unsigned char next;
	unsigned short count;
};

/* ----------------------------------------------- */

void ne_bufr ( struct ne_driver *, int, char *, int );
void ne_bufw ( struct ne_driver *, int, char *, int );

void ne_off ( struct ne_driver * );
void ne_on ( struct ne_driver * );
void ne_prom ( struct ne_driver * );

static int
ring_inc ( struct ne_driver *dev, int val )
{
	if ( ++val >= dev->stop )
	    val = dev->start;
	return val;
}

static int
ring_dec ( struct ne_driver *dev, int val )
{
	if ( --val < dev->start )
	    val = dev->stop-1;
	return val;
}

/* ----------------------------------------------- */

/* 0 is off.
 * 1 is "reasonable"
 * 2 is a lot
 * 3 or more is massive.
 */
static int ne_debug = 2;

/* Option to stop reception after a number
 * of received packets, set to -1 to disable.
 */
static int ne_limit = -1;


#define TY_ARP	0x0806

union packet {
    char buf[1600];
    struct {
    	char src[6];
	char dst[6];
	short type;
    } eth;
};

static union packet pku;

#define pkbuf	pku.buf
#define pketh	pku.eth

static int arp_count = 0;

static void
arp_show ( union packet *pk )
{
	++arp_count;
	printf ( "ARP %d\n", arp_count );
}

/* Called at interrupt level
 * Tries to read as many packets as possible from
 * the receive buffer ring.
 * XXX - rename this - it now loops and pulls
 * all packets off the receive ring.
 */
static void
rx_show ( struct ne_driver *dev, int rx_debug )
{
	int stat;
	int cur, next, len;
	int start, stop;
	struct ring_hdr header;
	int to_end;

	stat = inb_p ( dev->nic + ED_P0_RSR );

	next = ring_inc ( dev, inb_p ( dev->nic + ED_P0_BOUND ) );

	for ( ;; ) {
	    outb_p ( ED_CR_RDA | ED_CR_P1 | ED_CR_STA, dev->nic + ED_CR );
	    cur = inb_p ( dev->nic + ED_P1_CURR );
	    outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, dev->nic + ED_CR );

	    if ( next == cur )
	    	break;

	    /* Read these, just for debug */
	    outb_p ( ED_CR_RDA | ED_CR_P2 | ED_CR_STA, dev->nic + ED_CR );
	    start = inb_p ( dev->nic + ED_P2_PSTART );
	    stop = inb_p  ( dev->nic + ED_P2_PSTOP );
	    outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, dev->nic + ED_CR );

	    if ( rx_debug > 1 ) {
		printf ( "Receive (rsr @ %03x = %02x)"
		" sta, stp, cur, bnd = %02x %02x: %02x %02x\n",
		    dev->nic + ED_P0_RSR, stat,
		    start, stop, cur, next );
	    }

	    ne_bufr ( dev, next<<8, (char *) &header, sizeof(struct ring_hdr) );

	    if ( rx_debug > 2 ) {
		printf ( "Read at %02x\n", next );
		printf ( "Header next = %02x\n", header.next );
	    }

	    len = header.count;
	    if ( len < 60 || (header.status & ED_RSR_PXR) == 0 ) {
		printf ( "Bogus packet !\n" );
	    } else {
		to_end =  ((dev->stop - next) << 8) - sizeof(header);
		if ( len < to_end )
		    ne_bufr ( dev, (next<<8) + sizeof(header), pkbuf, len );
		else {
		    ne_bufr ( dev, (next<<8) + sizeof(header), pkbuf, to_end );
		    ne_bufr ( dev, dev->start<<8, pkbuf + to_end, len - to_end );
		}

		if ( ntohs(pketh.type) == TY_ARP ) {
		    arp_show ( &pku );
		} else {
		    printf ( "Receive %d bytes, type = %04x\n", len, ntohs(pketh.type) );
		    dump_b ( pkbuf, 4 );
		    /*
		    ne_off ( dev );
		    */
		}
	    }

	    next = header.next;
	    outb_p ( ring_dec(dev, next),  dev->nic + ED_P0_BOUND );
	}
}

static int check_ring ( struct ne_driver *dev, int nrx )
{
	int bound, cur;

    	bound = inb_p ( dev->nic + ED_P0_BOUND );

    	outb_p ( ED_CR_RDA | ED_CR_P1 | ED_CR_STA, dev->nic + ED_CR );
    	cur = inb_p ( dev->nic + ED_P1_CURR );
    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, dev->nic + ED_CR );

	if ( nrx > 1 )
	    printf ( "Nrx = %d\n", nrx );

	if ( cur != ring_inc ( dev, bound ) ) {
	    if ( (inb_p ( dev->nic + ED_P0_ISR ) & ED_ISR_PXR) != ED_ISR_PXR ) {
		printf ( "Ring oops: cur, bnd = %02x %02x\n", cur, bound );
		/*
		ne_off ( dev );
		*/
	    }
	}

	/*
	if ( nrx > 1 )
	    ne_off ( dev );
	    */
}

/* Interrupt routine.
 * Assume that the 8390 is always in page 0.
 * Anyone who switches to another page, must do so
 * inside a cpu-locked critical section.
 *
 * Notice that we may get multiple packets per RxInt,
 * so we need to loop and pull everything possible
 * off the ring each time we get an interrupt.
 */
void
ne_int ( struct ne_driver *dev )
{
	int isr;
	int stat;
    	int nic = dev->nic;
	int nrx = 0;

	/*
	printf ( "ne int!\n" );
	*/

	while ( isr = inb_p ( nic + ED_P0_ISR ) ) {
	    if ( isr & ED_ISR_PXR ) {
		/* Good packet received */
		outb_p ( ED_ISR_PXR, nic + ED_P0_ISR );
		printf ( "Rx Good %d (isr/rsr = %02x/%02x)\n",
		    nrx+1, isr, inb_p ( nic + ED_P0_RSR ) );
		rx_show ( dev, ne_debug );
		if ( ne_limit >= 0 ) {
		    ne_limit--;
		    if ( ! ne_limit )
			ne_off ( dev );	/* XXX */
		}
		nrx++;

	    } else if ( isr & ED_ISR_RXE ) {
		/* Bad packet received */
		outb_p ( ED_ISR_RXE, nic + ED_P0_ISR );
		printf ( "Rx Error (isr/rsr = %02x/%02x)\n",
		    isr, inb_p ( nic + ED_P0_RSR ) );

		dev->err_count++;

		rx_show ( dev, 9 );
		ne_off ( dev );	/* XXX */
	#ifdef notdef
	#endif

	    } else if ( isr & ED_ISR_PTX ) {
	    	/* packet transmitted OK */
	    	printf ( "packet transmitted OK\n" );
		outb_p ( ED_ISR_PTX, nic + ED_P0_ISR );

	    } else if ( isr & ED_ISR_TXE ) {
	    	/* packet transmit failed */
	    	printf ( "packet transmit failed\n" );
		outb_p ( ED_ISR_TXE, nic + ED_P0_ISR );

	    } else if ( isr & ED_ISR_CNT ) {
		/* Statistics counter overflow */
		dev->fe_count += inb_p ( nic + ED_P0_COUNT_FE);
		dev->crc_count += inb_p ( nic + ED_P0_COUNT_CRC);
		dev->miss_count += inb_p ( nic + ED_P0_COUNT_MP);
		outb_p ( ED_ISR_CNT, nic + ED_P0_ISR );

	    } else if ( isr & ED_ISR_RST ) {
		/* Device reset (device stop sorta complete) */
		printf ( "Device reset !!\n" );
		outb_p ( 0xff, nic + ED_P0_ISR );
		outb_p ( 0xff, nic + ED_P0_IMR );	/* XXX */

	    } else if ( isr & ED_ISR_OVW ) {
		/* Buffer ring overwrite (caught up to bound) */
		printf ( "Overwrite warning !\n" );
                outb ( ED_ISR_OVW, nic + ED_P0_ISR );
		stat = inb_p ( nic + ED_P0_IMR );		/* XXX */
		outb_p ( stat | ED_ISR_OVW, nic + ED_P0_IMR );	/* XXX */
		ne_off ( dev );	/* XXX */

	    } else if ( isr & ED_ISR_RDC ) {
		/* Remote DMA complete, we should never
		 * enable this to interrupt.
		 */
		printf ( "Remote DMA complete ?? !!\n" );
		outb_p ( stat | ED_ISR_RDC, nic + ED_P0_IMR );

	    } else {
		outb_p ( 0xff, nic + ED_P0_ISR );
		printf ( "Unknown (%02x)\n", isr );

	    }
	}

	if ( isr == 0 )
	    printf ( "Silly NE interrupt\n" );

	/* Done with isr */
	check_ring ( dev, nrx );
}

static void
reg_show ( int addr, char *msg )
{
	printf ( "%s at 0x%04x: ", msg, addr );
	printf ( "%02x\n", inb_p ( addr ) );
}

/* Show all the readable registers in page 0
 * reading the fifo register can (and does!!)
 * hang the computer if the chip is not in loopback mode.
 * The 2 reserved registers just aren't interesting.
 */
void
ne_show_all ( struct ne_driver *dev )
{
    	int nic = dev->nic;

	reg_show ( nic + 0,   "NE cmd" );
	reg_show ( nic + 1,   "NE ldma0" );
	reg_show ( nic + 2,   "NE ldma1" );
	reg_show ( nic + 3,   "NE bnry" );
	reg_show ( nic + 4,   "NE tsr" );
	reg_show ( nic + 5,   "NE ncol" );
#ifdef notdef
	reg_show ( nic + 6,   "NE fifo" );	/* reads can hang */
#endif
	reg_show ( nic + 7,   "NE isr" );
	reg_show ( nic + 8,   "NE rdma0" );
	reg_show ( nic + 0x9, "NE rdma1" );
#ifdef notdef
	reg_show ( nic + 0xa, "NE -" );
	reg_show ( nic + 0xb, "NE -" );
#endif
	reg_show ( nic + 0xc, "NE rsr" );
	reg_show ( nic + 0xd, "NE aerr" );
	reg_show ( nic + 0xe, "NE cerr" );
	reg_show ( nic + 0xf, "NE miss" );
}

void
ne_show ( struct ne_driver *dev )
{
	int nic = dev->nic;

	reg_show ( nic + 0,   "NE cmd" );
	reg_show ( nic + 7,   "NE isr" );
}

#define N_HWPROM	16

/* get hardware address prom contents
 * (16 bytes)
 */
void
ne_hget ( char *cbuf )
{
	short buf[N_HWPROM];
	int i;
	struct ne_driver *dev = &ne_driver_info;

	if ( dev->type == NE_HP ) {
	    for ( i=0; i<N_HWPROM; i++ )
		cbuf[i] = inb_p ( dev->io_base + i );
	    return;
	}

	/* NE-2000 */

	if ( dev->is_8bit ) {
	    ne_bufr ( dev, 0, (char *) cbuf, N_HWPROM );
	    return;
	}

	ne_bufr ( dev, 0, (char *) buf, N_HWPROM*2 );
	for ( i=0; i<N_HWPROM; i++ )
	    cbuf[i] = buf[i];
}

void
ne_hshow ( void )
{
	char cbuf[N_HWPROM];
	int i;

	ne_hget ( cbuf );

	printf ( "hardware address:  " );
	for ( i=0; i<N_HWPROM; i++ ) {
	    printf ( "%02x", cbuf[i] );
	    printf ( i < N_HWPROM-1 ? ":" : "\n" );
	}
}

/* write to buffer memory
 */
void
ne_bufw ( struct ne_driver *dev, int addr, char *buf, int count )
{
	int timeout = TIMEOUT_SPIN;
	int nic, data;
	int save;

	nic = dev->nic;
	data = dev->dataport;

	if ( count & 1 )
	    ++count;

	if ( dev->type == NE_HP ) {
	    save = inb_p ( dev->io_base + HP_CONFIG );
	    outb_p ( save | HP_DPEN, dev->io_base + HP_CONFIG );
	}

    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, nic + ED_CR );
    	outb_p ( ED_ISR_RDC, nic + ED_P0_ISR );

    	outb_p ( addr, nic + ED_P0_RSA0 );
    	outb_p ( addr>>8, nic + ED_P0_RSA1 );
    	outb_p ( count, nic + ED_P0_RBCR0 );
    	outb_p ( count>>8, nic + ED_P0_RBCR1 );

    	outb_p ( ED_CR_RD1 | ED_CR_P0 | ED_CR_STA, nic + ED_CR );

	outsw ( data, buf, count / 2 );

#ifdef notdef
	/* This was OK with NE2000 clones, but hangs the HPlan driver
	 */
	while ( timeout-- && (inb_p ( nic + ED_P0_ISR ) & ED_ISR_RDC) == 0 )
	    ;
	/*
	printf ( "ne_bufw done (%02x) after %d\n",
	    inb_p ( nic + ED_P0_ISR ), TIMEOUT_SPIN - timeout );
	*/
#endif

    	outb_p ( ED_ISR_RDC, nic + ED_P0_ISR );

	if ( dev->type == NE_HP ) {
	    outb_p ( save, dev->io_base + HP_CONFIG );
	}
}

/* read from buffer memory
 */
void
ne_bufr ( struct ne_driver *dev, int addr, char *buf, int count )
{
	int timeout = TIMEOUT_SPIN;
	int nic, data;
	int save;

	nic = dev->nic;
	data = dev->dataport;

	if ( count & 1 )
	    ++count;

	if ( dev->type == NE_HP ) {
	    save = inb_p ( dev->io_base + HP_CONFIG );
	    outb_p ( save | HP_DPEN, dev->io_base + HP_CONFIG );
	}

    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, nic + ED_CR );
	/*
    	outb_p ( ED_ISR_RDC, nic + ED_P0_ISR );
	*/

    	outb_p ( addr, nic + ED_P0_RSA0 );
    	outb_p ( addr>>8, nic + ED_P0_RSA1 );
    	outb_p ( count, nic + ED_P0_RBCR0 );
    	outb_p ( count>>8, nic + ED_P0_RBCR1 );

    	outb_p ( ED_CR_RD0 | ED_CR_P0 | ED_CR_STA, nic + ED_CR );

	insw ( data, buf, count / 2 );

#ifdef notdef
	/* This was OK with NE2000 clones, but hangs the HPlan driver
	 */
	while ( timeout-- && (inb_p ( nic + ED_P0_ISR ) & ED_ISR_RDC) == 0 )
	    ;
	/*
	printf ( "ne_bufr done (%02x) after %d\n",
	    inb_p ( nic + ED_P0_ISR ), TIMEOUT_SPIN - timeout );
	*/
#endif

    	outb_p ( ED_ISR_RDC, nic + ED_P0_ISR );

	if ( dev->type == NE_HP ) {
	    outb_p ( save, dev->io_base + HP_CONFIG );
	}
}

#ifdef notdef
/* fill buffer memory
 */
ne_fill ( int addr, int val )
{
	short buf[128];
	int i;

	for ( i=0; i<128; i++ )
	    buf[i] = val;
	ne_bufw ( dev, addr, (char *) buf, 128 );
}
#endif

void
ne_reset ( void )
{
	int val;
	struct ne_driver *dev = &ne_driver_info;

	printf (" Resetting the ne2000\n" );

	val = inb_p ( dev->io_base + NE_RESET );
	__udelay ( 5000 );
	outb_p ( dev->io_base + NE_RESET, val );
	__udelay ( 5000 );
}

ne_stop ( int nic )
{
	int timeout = TIMEOUT_SPIN;

	printf (" Stopping the 8390 (%02x)", inb_p ( nic + ED_CR ));
    	outb_p ( ED_CR_RDA | ED_CR_STOP, nic + ED_CR );
	printf (" (%02x)\n", inb_p ( nic + ED_CR ));
	/* the above yields a 0x23 status, how does STA get set ?? */

	while ( timeout-- && (inb_p ( nic + ED_P0_ISR ) & ED_ISR_RST) == 0 )
	    ;

	printf ( "ne_stop done (%02x) after %d\n",
	    inb_p ( nic + ED_P0_ISR ), TIMEOUT_SPIN - timeout );

	/* clear the interrupt bit */
	outb_p ( ED_ISR_RST, nic + ED_P0_ISR );
}

/* initialize the 8390
 * Do it by the book (e.g. p 1-27 of my datasheet)
 */
void
ne_init ( struct ne_driver *dev )
{
	int start, stop;
	int txpages = 6;
	int i;
	char phys[N_HWPROM];
	int gateway = 1;	/* XXX XXX */

	/* 1 - set CR to page 0
	 */
    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STOP, dev->nic + ED_CR );
	__udelay ( 5000 );

	/* 2 - initialize DCR
	 */
	if ( dev->is_8bit )
	    outb_p ( ED_DCR_F2 | ED_DCR_LS, dev->nic + ED_P0_DCR );
	else
	    outb_p ( ED_DCR_F2 | ED_DCR_WTS | ED_DCR_LS, dev->nic + ED_P0_DCR );

	/* 3 - clear remote byte counters
	 */
    	outb_p ( 0, dev->nic + ED_P0_RBCR0 );
    	outb_p ( 0, dev->nic + ED_P0_RBCR1 );

	/* 4 - initialize RCR.
	 */
    	outb_p ( 0, dev->nic + ED_P0_RCR );

	/* 5 - use TCR to put device into loopback mode 1 or 2.
	 */
    	outb_p ( ED_TCR_LB2, dev->nic + ED_P0_TCR );

	/* 6 - initialize buffer ring pointers.
	 */
	if ( dev->is_8bit ) {
	    start = 0x20;
	    stop = start + 0x20;
	} else {
	    start = 0x40;
	    stop = start + 0x40;
	}

	if ( gateway ) {
	    start = 0x60;
	    stop = 0xff;
	}
	start += txpages;

	dev->start = start;
	dev->stop = stop;

	outb_p ( start, dev->nic + ED_P0_PSTART );
	outb_p ( stop, dev->nic + ED_P0_PSTOP );

	/* The NS datasheet says to initialize
	 * BOUND=start.  This works fine for the chip,
	 * and it can do local DMA into all the buffers,
	 * but as soon as BOUND gets moved up to start,
	 * you get an Overflow shutdown, so now I am
	 * doing what the other drivers do, and keep
	 * BOUND at start-1, which gives up one page,
	 * but makes things work.
	 */
	outb_p ( ring_dec(dev,start), dev->nic + ED_P0_BOUND );

	/* 7 - clear any interrupts.
	 * 8 - initialize IMR
	 */
    	outb_p ( 0xff, dev->nic + ED_P0_ISR );
    	outb_p ( 0x00, dev->nic + ED_P0_IMR );

	/* 9 - switch to page 1 and load
	 * hardware and multicast registers.
	 * also CURR page.
	 * Do the switch in a cpu locked section.
	 */
	ne_hget ( phys );
	if ( memcmp ( phys, gateway_phys, 3 ) == 0 )
	    printf("Gateway card\n");

	cpu_enter ();
    	outb_p ( ED_CR_RDA | ED_CR_P1 | ED_CR_STOP, dev->nic + ED_CR );
	for ( i=0; i<6; i++ )
	    outb_p ( phys[i], dev->nic + ED_P1_PHYS + i );
	for ( i=0; i<8; i++ )
	    outb_p ( 0xff, dev->nic + ED_P1_MULT + i );
    	outb_p ( start, dev->nic + ED_P1_CURR );
    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STOP, dev->nic + ED_CR );
	cpu_leave ();

	/* 10 - write to CR and start the NIC
	 * (also switches back to page 0),
	 * device is still in loopback, so no
	 * local receive DMA happens.
	 */
    	outb_p ( ED_CR_RDA | ED_CR_P0 | ED_CR_STA, dev->nic + ED_CR );

	/* 11 - initialize TCR.
	 *  0 does put it into normal operation.
	 * (takes it out of loopback).
	 */
    	outb_p ( 0, dev->nic + ED_P0_TCR );

	/*
	 * device is now active.
	 */
}

/* Diagnostic -
 * Scan onboard ram address space to see
 * what areas respond as ram.
 */
void
ne_scan ( struct ne_driver *dev )
{
	int i, j, k;
	int page, bad;
	short buf0[128];
	short buf1[128];
	short buf2[128];

	for ( i=0; i<128; i++ ) {
	    buf0[i] = 0xaa55;
	    buf1[i] = 0x55aa;
	}

	for ( i=0; i<16; i++ ) {
	    printf ("Page %02x: ", i<<4 );
	    for ( j=0; j<16; j++ ) {
		page = i<<4 | j;
		bad = 0;
		ne_bufw ( dev, page<<8, (char *) buf1, 256 );
		ne_bufr ( dev, page<<8, (char *) buf2, 256 );
		for ( k=0; k<128; k++ )
		    if ( buf1[k] != buf2[k] )
			bad = 1;
		ne_bufw ( dev, page<<8, (char *) buf0, 256 );
		ne_bufr ( dev, page<<8, (char *) buf2, 256 );
		for ( k=0; k<128; k++ )
		    if ( buf0[k] != buf2[k] )
			bad = 1;
		if ( bad )
		    printf ( ".", i );
		else
		    printf ( "+", i );
	    }
	    printf ("\n");
	}
}

/* Diagnostic - modified scan from above.
 * Scan onboard ram address space to see
 * what areas are mapped twice.
 */
void
ne_scan2 ( struct ne_driver *dev )
{
	int i, j, k;
	int page, bad;
	short pat;
	short buf0[128];
	short buf1[128];
	short buf2[128];
	short is_ram[256];

	for ( i=0; i<128; i++ ) {
	    buf0[i] = 0xaa55;
	    buf1[i] = 0x55aa;
	}

	/* first scan simply locates ram
	 * and deposits a recognizable page pattern.
	 */
	for ( page=0; page<256; page++ ) {
	    bad = 0;
	    ne_bufw ( dev, page<<8, (char *) buf1, 256 );
	    ne_bufr ( dev, page<<8, (char *) buf2, 256 );
	    for ( k=0; k<128; k++ )
		if ( buf1[k] != buf2[k] )
		    bad = 1;
	    ne_bufw ( dev, page<<8, (char *) buf0, 256 );
	    ne_bufr ( dev, page<<8, (char *) buf2, 256 );
	    for ( k=0; k<128; k++ )
		if ( buf0[k] != buf2[k] )
		    bad = 1;

	    if ( bad ) {
		is_ram[page] = 0;
	    } else {
		is_ram[page] = 1;
	    }

	    if ( ! bad ) {
		/* deposit pattern */
		pat = page << 8 | page;
		for ( i=0; i<128; i++ )
		    buf2[i] = pat;
		ne_bufw ( dev, page<<8, (char *) buf2, 256 );
	    }
	}

	for ( i=0; i<16; i++ ) {
	    printf ("Page %02x: ", i<<4 );
	    for ( j=0; j<16; j++ ) {
		page = i<<4 | j;
		if ( ! is_ram[page] ) {
		    printf ( ".", i );
		} else {
		    bad = 0;
		    ne_bufr ( dev, page<<8, (char *) buf2, 256 );
		    pat = page << 8 | page;
		    for ( k=0; k<128; k++ )
			if ( buf2[k] != pat )
			    bad = 1;
		    if ( bad )
			printf ( "O", i );
		    else
			printf ( "+", i );
		}
	    }
	    printf ("\n");
	}
}

/* stolen right from the linux driver,
 * these values go into the HP-lan config
 * register to select an IRQ.
 *                      0  1  2  3  4  5  6  7  8  9 10 11		*/
static int hp_irq[] = { 0, 0, 4, 6, 8,10, 0,14, 0, 4, 2,12,0,0,0,0};

/* after reset, the NIC seems to come up with
 * 0x29 in the command register.
 * 0x80 in the ISR
 */
void
ne_open ( int base, int irq, int is_8bit )
{
    	int i;
	struct ne_driver *dev = &ne_driver_info;
	enum ne_type type = NE_HP;

	dev->type = type;

	if ( type == NE_2K ) {
	    dev->io_base = base;
	    dev->dataport = base + NE_DATA;
	    dev->nic = base;
	    dev->irq = irq;
	} else {
	    dev->io_base = base;
	    dev->dataport = base + NE_DATA;
	    dev->nic = base + HP_NIC;
	    dev->irq = irq;
	    /* NIC registers only appear when ya do this.
	     */
	    outb_p ( hp_irq[irq] | HP_RUN, base + HP_CONFIG );
	    is_8bit = 0;
	}

	if ( is_8bit )
		printf ("Testing 8 bit card at 0x%03x, irq %d\n", base, irq );
	else
		printf ("Testing 16 bit card at 0x%03x, irq %d\n", base, irq );


	/* XXX - Can detect this at runtime */
	dev->is_8bit = is_8bit;

	dev->fe_count = 0;
	dev->crc_count = 0;
	dev->miss_count = 0;
	dev->err_count = 0;

	irq_hookup ( irq, ne_int, dev );

	ne_init ( dev );

	ne_hshow ();
}

void
ne_on ( struct ne_driver *dev )
{
	if ( ne_debug )
	    printf ("Start receiver\n" );

	/* accept broadcast packets */
    	outb_p ( ED_RCR_BROAD, dev->nic + ED_P0_RCR );

	/* allow all interrupts except
	 * remote DMA complete.
	 */
    	outb_p ( ~ED_ISR_RDC, dev->nic + ED_P0_IMR );
}

void
ne_prom ( struct ne_driver *dev )
{
	if ( ne_debug )
	    printf ("Start receiver (promiscuous)\n" );

	/* promiscuous mode */
    	outb_p ( ED_RCR_PRO, dev->nic + ED_P0_RCR );

    	outb_p ( ~ED_ISR_RDC, dev->nic + ED_P0_IMR );
}

void
ne_off ( struct ne_driver *dev )
{
	if ( ne_debug )
	    printf ("Stop receiver\n" );

	/* turn off all interrupts */
    	outb_p ( 0, dev->nic + ED_P0_IMR );

	/* accept no packets */
    	outb_p ( 0, dev->nic + ED_P0_RCR );
}

/* network test "shell"
 */
#define MAXB	64
#define MAXW	4

static void dump_bb ( char *, int, unsigned long  );

void
ne_test ( void )
{
	char buf[MAXB];
	char *wp[MAXW];
	char pbuf[256];
	int nw, page, i;
	struct ne_driver *dev = &ne_driver_info;

	int iobase = 0x300;
	int irq = 5;
	int is8bit = 1;

	for ( ;; ) {

	    printf ( "net-test> " );
	    getline ( buf, MAXB );

	    if ( ! buf[0] )
	    	continue;

	    nw = split ( buf, wp, MAXW );

	    if ( **wp == 'q' )
	    	break;

	    if ( **wp == '?' ) {
		printf ( "Help\n" );
	    }

	    if ( **wp == 'a' ) {
	    	ne_show_all ( dev );
	    }

	    if ( wp[0][0] == 'm' && wp[0][1] == 'p' && nw == 3 ) {
		int what = atoi(wp[2]);
		int where = atoi(wp[1]);
		printf ( "%02x --> port %03x\n", what, where );
		outb_p ( what, where );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'p' && nw == 2 ) {
		show_port ( atoi(wp[1]) );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'p' &&
		wp[0][2] == 'n' && nw == 3 ) {

		for ( i=atoi(wp[2]); i; i-- )
		    show_port ( atoi(wp[1]) );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'p' && nw == 3 ) {
		show_ports ( atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'b' && nw == 3 ) {
	    	dump_b ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( wp[0][0] == 'd' && wp[0][1] == 'w' && nw == 3 ) {
	    	dump_w ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( **wp == 'd' && nw == 3 ) {
	    	dump_l ( (void *) atoi(wp[1]), atoi(wp[2]) );
		continue;
	    }

	    if ( **wp == 'b' && nw == 2 ) {
		page = atoi(wp[1]);
		ne_bufr ( dev, page<<8, pbuf, 256 );
		dump_bb ( pbuf, 256, page<<8 );
	    }

	    if ( **wp == 'f' && nw == 3 ) {
		page = atoi(wp[1]);
		memset ( pbuf, atoi(wp[2]), 256 );
		ne_bufw ( dev, page<<8, pbuf, 256 );
	    }

	    if ( **wp == 'o' && nw == 2 ) {
	    	iobase = atoi ( wp[1] );
	    }
	    if ( **wp == 'r' && nw == 2 ) {
	    	irq = atoi ( wp[1] );
	    }
	    if ( **wp == '8' && nw == 2 ) {
	    	is8bit = atoi ( wp[1] );
	    }

	    if ( **wp == 'l' && nw == 2 ) {
	    	ne_debug = atoi ( wp[1] );
	    }

	    if ( **wp == 'i' ) {
		ne_open ( iobase, irq, is8bit );
	    }

	    if ( **wp == 's' ) {
	    	/* Show statistics */
		dev->fe_count += inb_p ( dev->nic + ED_P0_COUNT_FE);
		dev->crc_count += inb_p ( dev->nic + ED_P0_COUNT_CRC);
		dev->miss_count += inb_p ( dev->nic + ED_P0_COUNT_MP);

		printf ( "Framing errors: %d\n", dev->fe_count );
		printf ( "CRC errors: %d\n",     dev->crc_count );
		printf ( "Missed packets: %d\n", dev->miss_count );
		printf ( "Receive errors: %d\n", dev->err_count );
	    }

	    if ( **wp == 'r' ) {
	    	/* scan on-board ram */
		ne_scan ( dev );
	    }
	    if ( **wp == 'S' ) {
	    	/* special scan on-board ram */
		ne_scan2 ( dev );
	    }

	    if ( **wp == 'p' ) {
		/* Go in promiscuous mode! */
		ne_prom ( dev );
	    }

	    if ( **wp == 'g' ) {
		/* Go ! */
	    	ne_on ( dev );
	    }
	    if ( **wp == 'h' ) {
		/* Stop ! */
	    	ne_off ( dev );
	    }

	}
}

/* Dump a buffer, labelling with address.
 * buffer is not at that address.
 */
static void
dump_bb ( char *buf, int count, unsigned long addr )
{
	int i, k;

	for ( k=0; k < count; k += 16, addr += 16 ) {
	    printf ( "%08x  ", addr );

	    for ( i=0; i<16; i++ )
		printf ( "%02x ", buf[k+i] );

	    printf ( "\n" );
	}
}

/* THE END */
