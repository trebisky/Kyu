/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* el3.c
 * Driver for the 3COM etherlink 3 (XL) ethernet adapter.
 *
 * This is the third network driver written for Skidoo.
 * 
 * Tom Trebisky  tom@mmto.org  Skidoo project
 *	4-18-2005 began coding
 *
 * The first card I am working with at work is an old
 * (first generation) 3c905 with ether address: 00:60:08:C5:38:0F
 */

#include "intel.h"
#include "pci.h"
#include "net.h"
#include "netbuf.h"

typedef void (*pcifptr) ( struct pci_dev * );

#define USE_IO_BASE

#ifdef USE_IO_BASE
#define ior_8(x)	inb((int)(x))
#define ior_16(x)	inw((int)(x))
#define ior_32(x)	inl((int)(x))
#define iow_8(b,x)	outb(b,(int)(x))
#define iow_16(b,x)	outw(b,(int)(x))
#define iow_32(b,x)	outl(b,(int)(x))
#else
#define ior_8(x)	(*(volatile unsigned char __force *) (x))
#define ior_16(x)	(*(volatile unsigned short __force *) (x))
#define ior_32(x)	(*(volatile unsigned long __force *) (x))
#define iow_8(b,x)	*(volatile unsigned char __force *) (x) = b
#define iow_16(b,x)	*(volatile unsigned short __force *) (x) = b
#define iow_32(b,x)	*(volatile unsigned long __force *) (x) = b
#endif

#define EL3_VENDOR	0x10B7

struct el3_info {
	int device;
	void *memaddr;
	void *ioaddr;
	void *iobase;
	int count;
	int irq;
	unsigned char eaddr[ETH_ADDR_SIZE];
	int active;
};

static struct el3_info el3_soft;

/* the XL io register offsets */
#define	EL_EECMD	0x0A
#define EL_EEDATA	0x0C
#define EL_CSR		0x0E	/* in every window */

#define EL_TIMER	0x1A
#define EL_TXSTAT	0x1B
#define EL_CSR2		0x1E
#define EL_DMACTL	0x20
#define EL_XMTPTR	0x24	/* "Download" = xmt */
#define EL_TXTHR	0x2F
#define EL_RCVSTAT	0x30
#define EL_FTIMER	0x34
#define EL_CDOWN	0x36
#define EL_RCVPTR	0x38	/* "Upload" = rcv */

/* things that go into the command register (csr) */

#define		G_RESET		0x0000
#define		RX_RESET	0x2800
#define		TX_RESET	0x5800
#define		W_SELECT	0x0800

/* things that go into the EEPROM command register */
#define PROM_BUSY	0x8000	
#define PROM_READ	0x0080

/* Prototypes */
void el3_eeprom ( void );

void
el3_activate ( void )
{
	el3_soft.active = 1;
}

void
get_el3_addr ( unsigned char *buf )
{
    	memcpy ( buf, el3_soft.eaddr, ETH_ADDR_SIZE );
}

#define ETHER_SIZE	1518


void
el3_probe ( struct pci_dev *pci_info )
{
    int dev;

    if ( pci_info->vendor != EL3_VENDOR )
	return;

    dev = pci_info->device >> 4;
    if ( dev != 0x900 && dev != 0x905 )
	return;

    el3_soft.device = pci_info->device;

    if ( el3_soft.count ) {
	el3_soft.count++;
	return;
    }
    el3_soft.count = 1;
    el3_soft.ioaddr = (void *) ((unsigned long) pci_info->base[0] & ~3);
    /* My old 0x9050 card gives zero for memaddr */
    el3_soft.memaddr = pci_info->base[1];
    el3_soft.iobase = el3_soft.ioaddr;
    el3_soft.irq = pci_info->irq;
    el3_soft.active = 0;
} 

static void
el3_locate ( void )
{
    int i;

    el3_soft.count = 0;
    pci_find_all ( 0, el3_probe );
    if ( el3_soft.count < 1 )
	return;

    printf ( "3Com XL network at %08x (IRQ %d)\n", el3_soft.iobase, el3_soft.irq );

    el3_eeprom ();

    printf ( "ether addr = " );
    for ( i=0; i<ETH_ADDR_SIZE; i++ ) {
	printf ( "%02x", el3_soft.eaddr[i] );
	if ( i < ETH_ADDR_SIZE-1 )
	    printf ( ":" );
    }
    printf ( "\n" );
}

int
el3_init ( void )
{
    el3_locate ();
    if ( el3_soft.count < 1 ) {
	printf ( "Cannot find a 3Com network card.\n" );
	return 0;
    }

#ifdef notyet
    irq_hookup ( ee_soft.irq, ee_isr, 0 );
#endif

    return 1;
}

int
el3_eeprom_read ( int addr )
{
    void * io = el3_soft.iobase;

    while ( ior_16 ( io + EL_EECMD ) & PROM_BUSY )
	;

    iow_16 ( PROM_READ + addr, io + EL_EECMD );

    while ( ior_16 ( io + EL_EECMD ) & PROM_BUSY )
	;

    return ior_16 ( io + EL_EEDATA );
}

#define MAC	0x0a

void
el3_eeprom ( void )
{
	void * io = el3_soft.iobase;
	int i, data;

	/* select register page 0 */
    	iow_16 ( W_SELECT + 0, io+EL_CSR );

	for ( i=0; i< ETH_ADDR_SIZE; i++ ) {
	    data = el3_eeprom_read ( MAC + i / 2 );
	    if ( i % 2 )
		el3_soft.eaddr[i] = data & 0xff;
	    else
		el3_soft.eaddr[i] = data >> 8;
	}
}

/* THE END */
