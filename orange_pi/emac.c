/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * emac.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  3/20/2017
 *
 * emac.c
 * Ethernet driver for the Allwinner H3 emac
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
#include <h3_ints.h>

/* Public entry points */

#include "netbuf.h"
// #include "net.h"

struct emac {
	volatile unsigned int ctl0;		/* 00 */
	volatile unsigned int ctl1;		/* 04 */
	volatile unsigned int int_stat;		/* 08 */
	volatile unsigned int int_ena;		/* 0c */
	volatile unsigned int tx_ctl0;		/* 10 */
	volatile unsigned int tx_ctl1;		/* 14 */
	int __pad0;				/* 18 --*/
	volatile unsigned int tx_flow;		/* 1c */
	volatile unsigned int tx_desc;		/* 20 */
	volatile unsigned int rx_ctl0;		/* 24 */
	volatile unsigned int rx_ctl1;		/* 28 */
	int __pad1;				/* 2c --*/
	int __pad2;				/* 30 --*/
	volatile unsigned int rx_desc;		/* 34 */
	volatile unsigned int rx_filt;		/* 38 */
	int __pad3;				/* 3C --*/
	volatile unsigned int rx_hash0;		/* 40 */
	volatile unsigned int rx_hash1;		/* 44 */
	volatile unsigned int mii_cmd;		/* 48 */
	volatile unsigned int mii_data;		/* 4c */
	struct {
	    volatile unsigned int hi;		/* 50 */
	    volatile unsigned int lo;		/* 54 */
	} mac_addr[8];
	int __pad4[8];				/* -- */
	volatile unsigned int tx_dma_stat;	/* b0 */
	volatile unsigned int tx_dma_cur_desc;	/* b4 */
	volatile unsigned int tx_dma_cur_buf;	/* b8 */
	int __pad5;				/* bc --*/
	volatile unsigned int rx_dma_stat;	/* c0 */
	volatile unsigned int rx_dma_cur_desc;	/* c4 */
	volatile unsigned int rx_dma_cur_buf;	/* c8 */
	int __pad6;				/* cc --*/
	volatile unsigned int rgmii_status;	/* d0 */
};

#define EMAC_BASE	((struct emac *) 0x01c30000)

/* ------------------------------------------------------------ */
/* PHY stuff */
/* ------------------------------------------------------------ */

/* Bits in the mii_cmd register */

#define MII_BUSY	0x01
#define MII_WRITE	0x02
#define MII_REG		0x1f0		/* 5 bits for which register */
#define MII_REG_SHIFT	4
#define MII_DEV		0x1f000		/* 5 bits for which device */
#define MII_DEV_SHIFT	12
#define MII_DIV		0x700000	/* 3 bits for scaler */

#define MII_DIV_16	0
#define MII_DIV_32	0x100000
#define MII_DIV_64	0x200000
#define MII_DIV_128	0x300000

/* Here are the PHY registers we use.
 * The first PHY registers are standardized and device
 * independent
 */
#define PHY_BMCR        0
#define PHY_BMSR        1
#define PHY_ID1         2
#define PHY_ID2         3
#define PHY_ADVERT      4
#define PHY_PEER        5

/* At this point we always use device 0 */

static int
phy_read ( int reg )
{
	struct emac *ep = EMAC_BASE;

	ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY;
	while ( ep->mii_cmd & MII_BUSY )
	    ;

	return ep->mii_data;
}

static void
phy_write ( int reg, int val )
{
	struct emac *ep = EMAC_BASE;

	ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_WRITE | MII_BUSY;
	ep->mii_data = val;

	while ( ep->mii_cmd & MII_BUSY )
	    ;

	return;
}

static void
phy_init ( void )
{
	unsigned int id;

        id = phy_read ( PHY_ID1 ) << 16;
        id |= phy_read ( PHY_ID2 );

	// returns 0x0de10044
	printf ( "PHY ID = %04x\n", id );
}

int
emac_init ( void )
{
	struct emac *ep = (struct emac *) 0;

	printf ( "Hello from the Emac driver\n" );
	phy_init ();

	printf ( "Shoud be d0 == %x\n", &ep->rgmii_status );
        return 0;
}

void
emac_activate ( void )
{
	printf ( "Emac activate\n" );
}

void
emac_show ( void )
{
	printf ( "Emac show ...\n" );
}

void
get_emac_addr ( char *addr )
{
}

void
emac_send ( struct netbuf *nbp )
{
}

/* THE END */
