/*
 * Copyright (C) 2024  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ether.c for the Zynq
 *
 * Tom Trebisky  11/17/2024
 *
 */

/* ether.c
 * Driver for the Zynq ethernet.
 *
 * The Zynq 7000 has a 1843 page TRM.
 * Chapter 16 (page 484) covers the ethernet controller.
 * Appendix B has register details (page 1267)
 *
 * The Zynq actually has two ethernet controllers!
 * I am not writing this driver to support two.
 * I don't have any boards that have interface hardware
 * for both, so I am avoiding the extra work.
 *
 * The Antminer S9 uses a B50612E (Broadcom) phy chip to 
 *  support gigabit ethernet.
 * The Ebaz supports only 10/100 ethernet vi an IP101GA phy chip.
 * 
 * Tom Trebisky  11-17-2024
 */

#include <arch/types.h>

#include "board.h"
#include "zynq_ints.h"

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

#define BIT(x)	(1<<(x))

#define ETH0_BASE	0xE000B000
#define ETH1_BASE	0xE000C000

/* Device registers - shamelessly taken from
 * the U-boot zynq_gem.c driver
 */
struct zynq_eth {
    vu32 nwctrl; /* 0x0 - Network Control reg */
    vu32 nwcfg; /* 0x4 - Network Config reg */
    vu32 nwsr; /* 0x8 - Network Status reg */
    u32 __pad1;
    vu32 dmacr; /* 0x10 - DMA Control reg */
    vu32 txsr; /* 0x14 - TX Status reg */
    vu32 rxqbase; /* 0x18 - RX Q Base address reg */
    vu32 txqbase; /* 0x1c - TX Q Base address reg */
    vu32 rxsr; /* 0x20 - RX Status reg */
    u32 __pad2[2];
    vu32 idr; /* 0x2c - Interrupt Disable reg */
    u32 __pad3;
    vu32 phymntnc; /* 0x34 - Phy Maintaince reg */
    u32 __pad4[18];
    vu32 hashl; /* 0x80 - Hash Low address reg */
    vu32 hashh; /* 0x84 - Hash High address reg */
#define LADDR_LOW   0
#define LADDR_HIGH  1
    vu32 laddr[4][LADDR_HIGH + 1]; /* 0x8c - Specific1 addr low/high reg */
    vu32 match[4]; /* 0xa8 - Type ID1 Match reg */
    u32 __pad6[18];
#define STAT_SIZE   44
    vu32 stat[STAT_SIZE]; /* 0x100 - Octects transmitted Low reg */
    u32 __pad9[20];
    vu32 pcscntrl;
    vu32 pcsstatus;
    u32 rserved12[35];
    vu32 dcfg6; /* 0x294 Design config reg6 */
    u32 __pad7[106];
    vu32 transmit_q1_ptr; /* 0x440 - Transmit priority queue 1 */
    u32 __pad8[15];
    vu32 receive_q1_ptr; /* 0x480 - Receive priority queue 1 */
    u32 __pad10[17];
    vu32 upper_txqbase; /* 0x4C8 - Upper tx_q base addr */
    u32 __pad11[2];
    vu32 upper_rxqbase; /* 0x4D4 - Upper rx_q base addr */
};

#define ETH_BASE	((struct zynq_eth *) ETH0_BASE)

/* Bits in the nwsr */
#define SR_MDIO_DONE	0x4

/* Bits in the phymnt */
#define MDIO_ALWAYS		0x40020000
#define MDIO_READ		(MDIO_ALWAYS | 0x20000000)
#define MDIO_WRITE		(MDIO_ALWAYS | 0x10000000)
#define MDIO_ADDR_SHIFT		23
#define MDIO_REG_SHIFT		18

/* return number of interfaces initialized */
int
eth_init ( void )
{
		phy_init ();
		return 1;
}

/* ===================================================================== */
/* ===================================================================== */

/* Phy stuff, eventually migrate to its own file
 */

/* Here are the registers in the PHY device that we use.
 * The first PHY registers are standardized and device
 * independent
 */
#define PHY_BMCR        0
#define PHY_BMSR        1
#define PHY_ID1         2
#define PHY_ID2         3
#define PHY_ADVERT      4
#define PHY_PEER        5
#define PHY_XSR         0xf

/* Bits in the basic mode control register. */
#define BMCR_RESET              0x8000
#define BMCR_LOOPBACK           0x4000
#define BMCR_100                0x2000      /* Set for 100 Mbit, else 10 */
#define BMCR_ANEG_ENA           0x1000      /* enable autonegotiation */
#define BMCR_POWERDOWN          0x0800      /* set to power down */
#define BMCR_ISOLATE            0x0400
#define BMCR_ANEG               0x0200      /* restart autonegotiation */
#define BMCR_FULL               0x0100      /* Set for full, else half */
#define BMCR_CT_ENABLE          0x0080      /* enable collision test */

/* Bits in the basic mode status register. */

#define BMSR_100FULL            0x4000
#define BMSR_100HALF            0x2000
#define BMSR_10FULL             0x1000
#define BMSR_10HALF             0x0800
#define BMSR_ANEGCOMPLETE       0x0020
#define BMSR_ANEGCAPABLE        0x0008
#define BMSR_LINK_UP            0x0004

/* Bits in the link partner ability register. */
#define LPA_10HALF              0x0020
#define LPA_10FULL              0x0040
#define LPA_100HALF             0x0080
#define LPA_100FULL             0x0100

#define LPA_ADVERT              LPA_10HALF | LPA_10FULL | LPA_100HALF | LPA_100FULL

void phy_show ( void );
void phy_ztest ( void );

static int
phy_read ( int reg )
{
		struct zynq_eth *ep = ETH_BASE;
		int phy_addr = 1;	/* this works */
		int tmo;
		int rv;
		//int stat;
		int cmd;

		cmd = MDIO_READ | (phy_addr<<MDIO_ADDR_SHIFT) | (reg<<MDIO_REG_SHIFT);
		ep->phymntnc = cmd;

		// stat = ep->nwsr;
		// See: Eth stat = 00000002
		// printf ( "Eth stat = %08x\n", stat );

		tmo = 100;
		while ( tmo-- && !(ep->nwsr & SR_MDIO_DONE) )
			;

		if ( tmo <= 0 ) {
			printf ( "Phy read timeout\n" );
			return 0xffff;
		}

		// printf ( "Phy tmo = %d\n", tmo);
		// printf ( "Eth stat = %08x\n", ep->nwsr );
		// printf ( "Eth phy = %08x\n", ep->phymntnc );

		rv = ep->phymntnc;

		return rv & 0xffff;
}

static void
phy_write ( int reg, int value )
{
		struct zynq_eth *ep = ETH_BASE;
		int phy_addr = 1;	/* this works */
		int tmo;
		int cmd;

		cmd = MDIO_WRITE | (phy_addr<<MDIO_ADDR_SHIFT) | (reg<<MDIO_REG_SHIFT) | (value&0xffff);
		ep->phymntnc = cmd;

		// stat = ep->nwsr;
		// printf ( "Eth stat = %08x\n", stat );

		/* These timeouts are very short, maybe 2 iterations
		 */
		tmo = 100;
		while ( tmo-- && !(ep->nwsr & SR_MDIO_DONE) )
			;

		if ( tmo <= 0 ) {
			printf ( "Phy write timeout\n" );
		}

		// printf ( "Phy tmo = %d\n", tmo);
		// printf ( "Eth stat = %08x\n", ep->nwsr );
		// printf ( "Eth phy = %08x\n", ep->phymntnc );
}

int
phy_init ( void )
{
		struct zynq_eth *ep = ETH_BASE;
		int aa, bb;

		printf ( "             *********************\n" );
		printf ( "In phy_init()\n" );

		phy_show ();
		// phy_ztest ();
		// phy_show ();

		ep->phymntnc = 0;

		aa = phy_read ( PHY_ID1 );
		bb = phy_read ( PHY_ID2 );

		/* This gets: Phy ID = 362 5e62
		 * exactly the value in the Broadcom B50612D datasheet
		 */
		printf ( "Phy ID = %x %x\n", aa, bb );
		printf ( "             *********************\n" );
}

/* This was an experiment to see which registers I could clear:
 *  The SR are thus read only.
 *  Note that the PEER register cleared itself.
 *  Due to my writes to the BMCR or ADVERT register.
 *  PEER  = cde1
 *  BMSR  : 7949 --> 7949
 *  XSR   : 3000 --> 3000
 *  BMCR  : 1140 -->    0
 *  ADVERT:  1e1 -->    0
 *  PEER  :    0 -->    0
 */

static void
ztest ( char *msg, int reg )
{
		int ini, aft;

		ini = phy_read ( reg );
		phy_write ( reg, 0 );
		aft = phy_read ( reg );
		printf ( "%s: %4x --> %4x\n", msg, ini, aft );
}

void
phy_ztest ( void )
{
    ztest ( "BMCR  ", PHY_BMCR );
    ztest ( "BMSR  ", PHY_BMSR );
    ztest ( "XSR   ", PHY_XSR );
    ztest ( "ADVERT", PHY_ADVERT );
    ztest ( "PEER  ", PHY_PEER );
}

/* Initiallly I see this (left from U-Boot)
 *  ADV   = 01e1
 *  PEER  = cde1
 *  BMCR  = 1140
 *  BMSR  = 796d
 *  XSR   = 3000
 * My network switch shows that it is in 1000 Mbit mode.
 * (during autonegotiation the status lights on my switch
 *  go out -- indicating no connection)
/* 
 */

void
phy_show ( void )
{
    printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
    printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
    printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
    printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
    printf ( "XSR   = %04x\n", phy_read ( PHY_XSR ) );
}

/* THE END */
