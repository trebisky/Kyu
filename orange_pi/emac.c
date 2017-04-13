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
/* Syscon stuff */
/* ------------------------------------------------------------ */

#define EMAC_SYSCON	((unsigned int *) 0x01c00030)

/* This is a single 32 bit register that controlls EMAC clocks and such */
/* These are only some of the bit definitions, but more than we need */

#define PHY_ADDR		1
#define SYSCON_PHY_ADDR		(PHY_ADDR<<20);

#define SYSCON_CLK24		0x40000		/* set for 24 Mhz clock (else 25) */
#define SYSCON_LEDPOL		0x20000		/* set for LED active low polarity */
#define SYSCON_SHUTDOWN		0x10000		/* set to power down PHY */
#define SYSCON_EPHY_INTERNAL	0x08000		/* set to use internal PHY */

#define SYSCON_RXINV		0x10		/* set to invert Rx Clock */
#define SYSCON_TXINV		0x8		/* set to invert Tx Clock */
#define SYSCON_PIT		0x4		/* PHYS type, set for RGMII, else MII */

/* TCS - Transmit Clock Source */
#define SYSCON_TCS_MII		0		/* for MII (what we want) */
#define SYSCON_TCS_EXT		1		/* External for GMII or RGMII */
#define SYSCON_TCS_INT		2		/* Internal for GMII or RGMII */
#define SYSCON_TCS_INVALID	3		/* invalid */

/* We probably inherit all of this from U-Boot, but we
 * certainly aren't going to rely on that, are we?
 */
static void
syscon_setup ( void )
{
	volatile unsigned int *sc = EMAC_SYSCON;

	printf ( "Emac Syscon = %08x\n", *sc );

	*sc = SYSCON_EPHY_INTERNAL | SYSCON_CLK24 | SYSCON_PHY_ADDR;

	printf ( "Emac Syscon = %08x\n", *sc );
}

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

#define MII_DIV_MASK	0x700000	/* 3 bits for scaler */
#define MII_DIV_16	0
#define MII_DIV_32	0x100000
#define MII_DIV_64	0x200000
#define MII_DIV_128	0x300000	/* linux uses this */

#define MII_DIV		MII_DIV_128

/* Should this match PHY_ADDR above ?? */
/* U-boot phy.c uses device address 1 */
#define PHY_DEV		1

/* Here are the PHY registers we use.
 * The first PHY registers are standardized and device
 * independent
 */
#define PHY_BMCR        0
#define PHY_BMSR        1
#define PHY_ID1         2
#define PHY_ID2         3
#define PHY_ADV         4
#define PHY_PEER        5

/* Bits in the basic mode control register. */
#define BMCR_RESET              0x8000
#define BMCR_LOOPBACK           0x4000
#define BMCR_SPEED              0x2000		/* Set for 100 Mbit, else 10 */
#define BMCR_ANEG_ENA           0x1000		/* enable autonegotiation */
#define BMCR_POWERDOWN          0x0800		/* set to power down */
#define BMCR_ISOLATE            0x0400
#define BMCR_ANEG               0x0200		/* restart autonegotiation */
#define BMCR_DUPLEX             0x0100		/* Set for full, else half */
#define BMCR_CT_ENABLE          0x0080		/* enable collision test */

/* Bits in the basic mode status register. */
#define BMSR_ANEGCAPABLE        0x0008
#define BMSR_ANEGCOMPLETE       0x0020
#define BMSR_10HALF             0x0800
#define BMSR_10FULL             0x1000
#define BMSR_100HALF            0x2000
#define BMSR_100FULL            0x4000

/* Bits in the link partner ability register. */
#define LPA_10HALF              0x0020
#define LPA_10FULL              0x0040
#define LPA_100HALF             0x0080
#define LPA_100FULL             0x0100

#define LPA_ADVERT              LPA_10HALF | LPA_10FULL | LPA_100HALF | LPA_100FULL

/* At this point we always use device 0.
 * This gives us the fast ethernet PHY built into the H3 chip.
 * For Orange Pi variants that support 1G, we would need to
 * address an external PHY, probably the Realtek RTL8211E.
 * Note that setting device 0 or 1 gives the same results
 *  other addresses (like 2 or 9) read all ones
 *
 * XXX - maybe read/write need timeouts
 */

#define ISB     asm volatile ("isb sy" : : : "memory")
#define DSB     asm volatile ("dsb sy" : : : "memory")
#define DMB     asm volatile ("dmb sy" : : : "memory")

#define isb()   ISB
#define dsb()   DSB
#define dmb()   DMB

/* Needs a timeout */
/* XXX - could be inline */
static void
phy_spin ( void )
{
	struct emac *ep = EMAC_BASE;
	int tmo = 0;

	/* polls consistently 41 times */
	while ( ep->mii_cmd & MII_BUSY )
	    tmo++;

	// printf ( "phy_spin count = %d\n", tmo );
}

#ifdef notdef
static int
phy_readx ( int reg )
{
	struct emac *ep = EMAC_BASE;
	int rv;
	int i;

	phy_spin ();
	rv = ep->mii_data;	/* XXX - discard */

	// ep->mii_cmd = (2 << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;
	// ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY;
	dmb ();
	ep->mii_cmd = (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;

	phy_spin ();

	for ( i=0; i<5; i++ ) {
	    thr_delay ( 200 );	/* XXX */
	    printf ( "phy_readx %2d of %02x gets = %04x\n", i+1, reg, ep->mii_data );
	}

#ifdef notdef
	ep->mii_cmd = (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;

	phy_spin ();

	for ( i=0; i<5; i++ ) {
	    thr_delay ( 200 );	/* XXX */
	    printf ( "phy_readx %2d of %02x gets = %04x\n", i+1, reg, ep->mii_data );
	}
#endif


	// return ep->mii_data;
	rv = ep->mii_data;
	dmb ();
	return rv;
}

static int
phy_readxx ( int reg )
{
	struct emac *ep = EMAC_BASE;
	int rv;
	int i;

	phy_spin ();

	// ep->mii_cmd = (2 << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;
	// ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY;
	dmb ();
	ep->mii_cmd = (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;

	phy_spin ();

	rv = ep->mii_data;
	printf ( "phy_readxx of %02x gets = %04x\n", reg, rv );

	/*
	for ( i=0; i<5; i++ ) {
	    thr_delay ( 200 );
	    printf ( "phy_readx %2d of %02x gets = %04x\n", i+1, reg, ep->mii_data );
	}
	*/

	// return ep->mii_data;
	dmb ();
	return rv;
}
#endif

/* This has the odd problem, fixed below */
static int
phy_read1 ( int reg )
{
	struct emac *ep = EMAC_BASE;

	phy_spin();
	ep->mii_cmd = MII_DIV | (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;
	phy_spin();

	return ep->mii_data;
}

/* This works, but why do we have to do it twice ??? XXX */
static int
phy_read ( int reg )
{
	struct emac *ep = EMAC_BASE;
	unsigned int val;
	unsigned int cmd;

	// ep->mii_cmd = (2 << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;
	// ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY;
	cmd = MII_DIV | (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;

	/* Some weird thing is going on and whenever I switch registers
	 * I get the data for the address before the one I set.
	 * I just gave up and do this.  tjt  3-26-2017
	 */
	phy_spin();
	ep->mii_cmd = cmd;
	phy_spin();
	val = ep->mii_data;

	printf ( "phy_read A called %d, %04x, %04x\n", reg, discard, cmd );

	phy_spin();
	ep->mii_cmd = cmd;
	phy_spin();
	val = ep->mii_data;

	printf ( "phy_read B called %d, %04x, %04x\n", reg, val, cmd );

	// return ep->mii_data;
	return val;
}


static void
phy_write ( int reg, int val )
{
	struct emac *ep = EMAC_BASE;
	unsigned int cmd;

	// ep->mii_cmd = (2 << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;
	// ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;

	cmd = MII_DIV | (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;

	printf ( "phy_write called %d, %04x, %04x\n", reg, val, cmd );

	phy_spin ();
	ep->mii_cmd = cmd;
	ep->mii_data = val;
	phy_spin ();

	return;
}

static void
phy_show ( void )
{
        printf ( "PHY status BMCR: %04x, BMSR: %04x\n",
	    phy_read ( PHY_BMCR ), phy_read ( PHY_BMSR ) );
}

#define DUPLEX_HALF             0x00
#define DUPLEX_FULL             0x01

int link_good;
int link_duplex;
int link_speed;

static void
phy_set_link ( void )
{
	unsigned int val;

	printf ( "ADV   = %04x\n", phy_read ( PHY_ADV ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

        link_speed = 10;
        link_duplex = DUPLEX_HALF;

        val = phy_read ( PHY_ADV ) & phy_read ( PHY_PEER );

        if ( val & (LPA_100FULL | LPA_100HALF) )
            link_speed = 100;

        if ( val & (LPA_100FULL | LPA_10FULL) )
            link_duplex = DUPLEX_FULL;

	/*
	if ( ! link_good ) {
            printf("link down (emac)\n");
	    return;
	}
	*/

	printf ( "link up (emac), speed %d, %s duplex\n",
                link_speed, (link_duplex == DUPLEX_FULL) ? "full" : "half" );

}

static void
phy_uboot ( void )
{
	printf ( "Values from U-Boot:\n" );
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADV ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
}

#define AN_TIMEOUT	5000

/* perform autonegotiation.
 * This takes from 2 to 3 seconds,
 *  typically 2040 milliseconds.
 */
static void
phy_aneg ( void )
{
        int reg;
        int tmo = AN_TIMEOUT;

	printf ( "Autonegotiation ...\n" );
	phy_uboot ();

	phy_write ( PHY_ADV, 0 );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADV ) );
	phy_write ( PHY_ADV, 0 );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADV ) );
	phy_write ( PHY_ADV, LPA_ADVERT );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADV ) );
	phy_write ( PHY_ADV, LPA_ADVERT );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADV ) );

	/* start autonegotiation, this bit self clears */
	printf ( "Starting Autonegotiation:\n" );
	printf ( "BMSR   = %04x\n", phy_read ( PHY_BMSR ) );
        reg = phy_read ( PHY_BMCR );
        phy_write ( PHY_BMCR, reg | BMCR_ANEG );
        phy_write ( PHY_BMCR, reg | BMCR_ANEG );
	printf ( "BMSR   = %04x\n", phy_read ( PHY_BMSR ) );

        while ( tmo ) {
            if ( phy_read ( PHY_BMSR ) & BMSR_ANEGCOMPLETE )
                break;
            thr_delay ( 1 );
	    tmo--;
        }

	if ( ! tmo ) {
	    printf ( " *** *** Autonegotiation timeout\n" );
	    phy_show ();
	    link_good = 0;
	    phy_set_link ();	/* XXX */
	    return;
	}

	printf ( "Autonegotiation finished\n" );
	printf ( "Aneg done in %d milliseconds\n", AN_TIMEOUT - tmo );
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADV ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

	phy_show ();
	link_good = 1;
	phy_set_link ();
}

#define RESET_TIMEOUT	50

static void
phy_reset ( void )
{
        int reg;
        int tmo = RESET_TIMEOUT;

	/* start reset, this bit self clears */
        reg = phy_read ( PHY_BMCR );
        phy_write ( PHY_BMCR, reg | BMCR_RESET );

        /* Should autoclear in under 0.5 seconds */
        /* (I am seeing 2 milliseconds) */

        while ( tmo-- && (phy_read(PHY_BMCR) & BMCR_RESET ) )
            thr_delay ( 1 );

        printf ( "PHY reset cleared in %d milliseconds\n", (RESET_TIMEOUT-tmo) );

	phy_show ();
}

#ifdef notdef
static void sun8i_adjust_link(struct emac_eth_dev *priv,
                              struct phy_device *phydev)
{
        u32 v;

        v = readl(priv->mac_reg + EMAC_CTL0);

        if (phydev->duplex)
                v |= BIT(0);
        else
                v &= ~BIT(0);

        v &= ~0x0C;

        switch (phydev->speed) {
        case 1000:
                break;
        case 100:
                v |= BIT(2);
                v |= BIT(3);
                break;
        case 10:
                v |= BIT(3);
                break;
        }
        writel(v, priv->mac_reg + EMAC_CTL0);
}
#endif

static void 
phy_id ( void )
{
	unsigned int id;

        id = phy_read ( PHY_ID1 ) << 16;
        id |= phy_read ( PHY_ID2 );

	// returns 0x00441400
	printf ( "PHY ID = %04x\n", id );
}

#ifdef notdef
static void
phy_test1 ( void )
{
	phy_id ();
	phy_id ();
	phy_id ();
	phy_reset ();
	phy_id ();
	phy_id ();
	phy_id ();
	phy_aneg ();
	phy_id ();
	phy_id ();
	phy_id ();
}

static void
phy_test2 ( void )
{
	phy_readxx ( PHY_BMCR );
	phy_readxx ( PHY_BMSR );
	phy_readxx ( PHY_ID1 );
	phy_readxx ( PHY_ID2 );

	phy_readxx ( PHY_BMCR );
	phy_readxx ( PHY_BMCR );
	phy_readxx ( PHY_BMCR );
	phy_readxx ( PHY_BMCR );

	phy_readxx ( PHY_BMSR );
	phy_readxx ( PHY_BMSR );
	phy_readxx ( PHY_BMSR );
	phy_readxx ( PHY_BMSR );

	phy_readxx ( PHY_ID1 );
	phy_readxx ( PHY_ID1 );
	phy_readxx ( PHY_ID1 );
	phy_readxx ( PHY_ID1 );

	phy_readxx ( PHY_ID2 );
	phy_readxx ( PHY_ID2 );
	phy_readxx ( PHY_ID2 );
	phy_readxx ( PHY_ID2 );
}
#endif

static void
phy_init ( void )
{
	struct emac *ep = EMAC_BASE;

	printf ( "Starting Phy Init\n" );

	ep->mii_cmd = 0x10;	/* perform reset */
	/* delay ?? */
	ep->mii_cmd = MII_DIV;  /* Set clock divisor */

	phy_id ();
	// phy_reset ();
	// phy_id ();
	phy_aneg ();

	printf ( "Finished with Phy Init\n" );
}

/* ------------------------------------------------------------ */
/* Real Emac stuff */
/* ------------------------------------------------------------ */

#define SID_BASE	((unsigned int *) 0x01c14200)

static void
show_sid ( void )
{
	volatile unsigned int *p = SID_BASE;
	int i;

	for ( i=0; i<8; i++ ) {
	    printf ( "SID %d = %08x: %08x\n", i, p, *p );
	    p++;
	}
}

/* Interestingly the emac can accomodate 8 MAC addresses.
 * All but the first must have a bit set to indicate they
 *  are active.
 */
static void
set_mac ( char *mac_id )
{
	struct emac *ep = EMAC_BASE;

	ep->mac_addr[0].hi = mac_id[4] + (mac_id[5] << 8);
	ep->mac_addr[0].lo = mac_id[0] + (mac_id[1] << 8) +
	    (mac_id[2] << 16) + (mac_id[3] << 24);
}

#ifdef notdef
                        mac_addr[0] = (i << 4) | 0x02;
                        mac_addr[1] = (sid[0] >>  0) & 0xff;
                        mac_addr[2] = (sid[3] >> 24) & 0xff;
                        mac_addr[3] = (sid[3] >> 16) & 0xff;
                        mac_addr[4] = (sid[3] >>  8) & 0xff;
                        mac_addr[5] = (sid[3] >>  0) & 0xff;
#endif


#define CTL1_SOFT_RESET		0x01

#define SOFT_RESET_TIMEOUT	500

static void
emac_reset ( void )
{
	struct emac *ep = EMAC_BASE;
        int tmo = SOFT_RESET_TIMEOUT;

	ep->ctl1 |= CTL1_SOFT_RESET;

        while ( tmo-- && ep->ctl1 & CTL1_SOFT_RESET )
            thr_delay ( 1 );

        printf ( "Emac Reset cleared in %d milliseconds\n", (SOFT_RESET_TIMEOUT-tmo) );
}


/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */

/* Try to treat a register like ram */
/* Seems to work just fine */
static void
bogus ( void )
{
	struct emac *ep = EMAC_BASE;

	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0xff;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );

	ep = (struct emac *) 0;
	printf ( "MII data = %08x\n", &ep->mii_data );
	printf ( "MII cmd  = %08x\n", &ep->mii_cmd );
}

/* Call early in Kyu and see what is up */
void
emac_probe ( void )
{
	// struct emac *ep = EMAC_BASE;
	printf ( "READ1 Values from U-Boot:\n" );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "ID1  = %04x\n", phy_read1 ( PHY_ID1 ) );
	printf ( "ID2  = %04x\n", phy_read1 ( PHY_ID2 ) );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "ID1  = %04x\n", phy_read1 ( PHY_ID1 ) );
	printf ( "ID2  = %04x\n", phy_read1 ( PHY_ID2 ) );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "ID1  = %04x\n", phy_read1 ( PHY_ID1 ) );
	printf ( "ID2  = %04x\n", phy_read1 ( PHY_ID2 ) );

	/*
	printf ( "EARLY Values from U-Boot:\n" );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADV ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
	*/
}

int
emac_init ( void )
{
	// struct emac *ep = (struct emac *) 0;

	// validate my structure layout
	// printf ( "Shoud be 0xd0 == 0x%x\n", &ep->rgmii_status );

	printf ( " *************************** Hello from the Emac driver\n" );

	/* We inherit clocks and such from U-boot, thus far anyway ... */
	// ccu_emac ();

	bogus ();
	phy_uboot ();
	// bogus ();
	//syscon_setup ();
	//emac_reset ();
	// bogus ();
	phy_init ();
	// show_sid ();
	bogus ();

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
