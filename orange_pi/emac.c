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

/* line size for this flavor of ARM */
/* XXX should be in some header file */
#define	ARM_DMA_ALIGN	64

/* from linux/compiler_gcc.h in U-Boot */
#define __aligned(x)            __attribute__((aligned(x)))

struct emac {
	volatile unsigned int ctl0;		/* 00 */
	volatile unsigned int ctl1;		/* 04 */

	volatile unsigned int int_stat;		/* 08 -- not touched by U-boot */
	volatile unsigned int int_ena;		/* 0c -- not touched by U-boot */

	volatile unsigned int tx_ctl0;		/* 10 */
	volatile unsigned int tx_ctl1;		/* 14 */
	int __pad0;				/* 18 --*/
	volatile unsigned int tx_flow;		/* 1c -- never used */
	volatile void * tx_desc;		/* 20 */

	volatile unsigned int rx_ctl0;		/* 24 */
	volatile unsigned int rx_ctl1;		/* 28 */
	int __pad1;				/* 2c --*/
	int __pad2;				/* 30 --*/
	volatile void * rx_desc;		/* 34 */

	volatile unsigned int rx_filt;		/* 38 -- never used*/
	int __pad3;				/* 3C --*/

	volatile unsigned int rx_hash0;		/* 40 -- never used */
	volatile unsigned int rx_hash1;		/* 44 -- never used */

	volatile unsigned int mii_cmd;		/* 48 */
	volatile unsigned int mii_data;		/* 4c */

	struct {
	    volatile unsigned int hi;		/* 50 */
	    volatile unsigned int lo;		/* 54 */
	} mac_addr[8];
	int __pad4[8];				/* -- */

	volatile unsigned int tx_dma_stat;	/* b0 - never used */
	volatile unsigned int tx_dma_cur_desc;	/* b4 - never used */
	volatile unsigned int tx_dma_cur_buf;	/* b8 - never used */
	int __pad5;				/* bc --*/
	volatile unsigned int rx_dma_stat;	/* c0 - never used */
	volatile unsigned int rx_dma_cur_desc;	/* c4 - never used */
	volatile unsigned int rx_dma_cur_buf;	/* c8 - never used */
	int __pad6;				/* cc --*/
	volatile unsigned int rgmii_status;	/* d0 - never used */
};

#define EMAC_BASE	((struct emac *) 0x01c30000)

/* bits in the ctl0 register */

#define	CTL_FULL_DUPLEX	0x0001
#define	CTL_LOOPBACK	0x0002
#define	CTL_SPEED_1000	0x0000
#define	CTL_SPEED_100	0x000C
#define	CTL_SPEED_10	0x0008

/* bits in the int_ena and int_stat registers */
#define INT_TX			0x0001
#define INT_TX_DMA_STOP		0x0002
#define INT_TX_BUF_AVAIL	0x0004
#define INT_TX_TIMEOUT		0x0008
#define INT_TX_UNDERFLOW	0x0010
#define INT_TX_EARLY		0x0020
#define INT_TX_ALL	(INT_TX | INT_TX_BUF_AVAIL | INT_TX_DMA_STOP | INT_TX_TIMEOUT | INT_TX_UNDERFLOW )

#define INT_RX			0x0100
#define INT_RX_NOBUF		0x0200
#define INT_RX_DMA_STOP		0x0400
#define INT_RX_TIMEOUT		0x0800
#define INT_RX_OVERFLOW		0x1000
#define INT_RX_EARLY		0x2000
#define INT_RX_ALL	(INT_RX | INT_RX_NOBUF | INT_RX_DMA_STOP | INT_RX_TIMEOUT | INT_RX_OVERFLOW )

#define	INT_MII			0x10000		/* only in status */

/* bits in the Rx ctrl0 register */
#define	RX_EN			0x80000000
#define	RX_FRAME_LEN_CTL	0x40000000
#define	RX_JUMBO		0x20000000
#define	RX_STRIP_FCS		0x10000000
#define	RX_CHECK_CRC		0x08000000

#define	RX_PAUSE_MD		0x00020000
#define	RX_FLOW_CTL_ENA		0x08010000

/* bits in the Rx ctrl1 register */
#define	RX_DMA_START		0x80000000
#define	RX_DMA_ENA		0x40000000
#define	RX_MD			0x00000002
#define	RX_NO_FLUSH		0x00000001

/* bits in the Tx ctrl0 register */
#define	TX_EN			0x80000000
#define	TX_FRAME_LEN_CTL	0x40000000

/* bits in the Tx ctrl1 register */
#define	TX_DMA_START		0x80000000
#define	TX_DMA_ENA		0x40000000

/* Bits in the Rx filter register */
#define	RX_FILT_DIS		0x80000000
#define	RX_DROP_BROAD		0x00020000
#define	RX_ALL_MULTI		0x00010000

/* ------------------------------------------------------------ */
/* Descriptors */
/* ------------------------------------------------------------ */

struct emac_desc {
	volatile unsigned long status;
	long size;
	char * buf;
	struct emac_desc *next;
}	__aligned(ARM_DMA_ALIGN);

/* status bits */
#define DS_ACTIVE	0x80000000	/* set when available for DMA */
#define DS_DS_FAIL	0x40000000	/* Rx DAF fail */
#define DS_CLIP		0x00004000	/* Rx clipped (buffer too small) */
#define DS_SA_FAIL	0x00002000	/* Rx SAF fail */
#define DS_OVERFLOW	0x00000800	/* Rx overflow */
#define DS_FIRST	0x00000200	/* Rx first in list */
#define DS_LAST		0x00000100	/* Rx last in list */
#define DS_HEADER_ERR	0x00000080	/* Rx header error */
#define DS_COLLISION	0x00000040	/* Rx late collision in half duplex */
#define DS_LENGTH_ERR	0x00000010	/* Rx length is wrong */
#define DS_PHY_ERR	0x00000008	/* Rx error from Phy */
#define DS_CRC_ERR	0x00000002	/* Rx error CRC wrong */
#define DS_PAYLOAD_ERR	0x00000001	/* Rx error payload checksum or length wrong */

#define DS_TX_HEADER_ERR	0x00010000	/* Tx header error */
#define DS_TX_LENGTH_ERR	0x00004000	/* Tx length error */
#define DS_TX_PAYLOAD		0x00001000	/* Tx payload checksum wrong */
#define DS_CARRIER		0x00000400	/* Tx lost carrier */
#define DS_COL1			0x00000200	/* Tx collision */
#define DS_COL2			0x00000100	/* Tx too many collisions */
/* collision count in these bits */
#define DS_DEFER_ERR		0x00000004	/* Tx defer error (too many) */
#define DS_UNDERFLOW		0x00000002	/* Tx fifo underflow */
#define DS_DEFER		0x00000001	/* Tx defer this frame (half duplex) */

#define	DSS_MAGIC		0x01000000	/* Undocumented bit for transmission */

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

/* Notes on emac phy  4-13-2017

I have driven myself crazy trying to figure out how to work with the on chip
Phy in the H3 chip.  The following summarizes the state of my understanding.

---
To read a register, I must perform two reads.  The first read gets the
previous thing I tried to read.  Consider reading two registers A and B,
what I see is this:

    read A, get x
    read A, get Aval
    read A, get Aval ....
    read B, get Aval
    read B, get Bval
    read B, get Bval ...

So performing two reads and discarding the first response gives proper values.

---
Writes just do not seem to work, and I have tried various games with no luck.
In particular I have tried to start autonegotiation, then polled the bit to
indicate completion.  This never works, nothing completes (or even seems to
start).

---
My thinking led me to a careful study of the emac code in U-boot.
It seems to work after all, but my conclusion is that it works simply
by accident.  I rebuilt U-boot sprinkling printf statements all through
the emac and phy driver to see what is going on.  I see that the phy reads
are giving the same "stale" values that I see.  Also the code in U-boot
never polls to check completion on anything, but just starts autonegotiation,
then dives in later assuming that it worked OK.

So why does it work?  Partly luck, but also I believe
that the Phy chip performs autonegotiation all by itself when it comes out
of reset and that the result registers can be read and properly analyzed.

---
TODO - I am going to press on without fully initializing the Phy
(since it seems impossible to do so).  I would like to do further
experiments to see if experimenting with the clock divisor leads to
different results.  Also I want to analyze the linux emac driver to
see if it has a clever trick to make all of this work ... someday.

 */

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

/* Make PHY_DEV match PHY_ADDR above.
 *
 * I get identical results setting this to 0 or 1.
 * U-boot sets it to 1, so I will also.
 * Code that does a phy_reset seems to set this to 0 though.
 *
 * This gives us the fast ethernet PHY built into the H3 chip.
 * For Orange Pi variants that support 1G, we would need to
 * address an external PHY, probably the Realtek RTL8211E.
 * Note that setting device 0 or 1 gives the same results
 *  other addresses (like 2 or 9) read all ones
 */

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

#ifdef notdef
/* ARM barrier stuff */
#define ISB     asm volatile ("isb sy" : : : "memory")
#define DSB     asm volatile ("dsb sy" : : : "memory")
#define DMB     asm volatile ("dmb sy" : : : "memory")

#define isb()   ISB
#define dsb()   DSB
#define dmb()   DMB
#endif

/* XXX - Needs a timeout.  */
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

#ifdef PHY_JUNK
/* This has the odd problem,
 * We use this for experiments, but the version below
 * that does two reads gives correct behavior
 */
static int
phy_read1 ( int reg )
{
	struct emac *ep = EMAC_BASE;

	phy_spin();
	ep->mii_cmd = MII_DIV | (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY;
	phy_spin();

	return ep->mii_data;
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
#endif

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

	// printf ( "phy_read A called %d, %04x, %04x\n", reg, val, cmd );

	phy_spin();
	ep->mii_cmd = cmd;
	phy_spin();
	val = ep->mii_data;

	// printf ( "phy_read B called %d, %04x, %04x\n", reg, val, cmd );

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

#define DUPLEX_HALF             0x00
#define DUPLEX_FULL             0x01

int link_good;
int link_duplex;
int link_speed;

static void
phy_set_link ( void )
{
	struct emac *ep = EMAC_BASE;
	unsigned int val;

	// printf ( "ADV   = %04x\n", phy_read ( PHY_ADV ) );
	// printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

	/* XXX - usually this would be set if autonegotiation
	 * worked or not.
	 */
	link_good = 1;

        link_speed = 10;
        link_duplex = DUPLEX_HALF;

        val = phy_read ( PHY_ADV ) & phy_read ( PHY_PEER );

        if ( val & (LPA_100FULL | LPA_100HALF) )
            link_speed = 100;

        if ( val & (LPA_100FULL | LPA_10FULL) )
            link_duplex = DUPLEX_FULL;

	if ( ! link_good ) {
            printf("link down (emac)\n");
	    return;
	}

	printf ( "link up (emac), speed %d, %s duplex\n",
                link_speed, (link_duplex == DUPLEX_FULL) ? "full" : "half" );

	val = 0;

	if ( link_duplex == DUPLEX_FULL )
	    val |= CTL_FULL_DUPLEX;

	if ( link_speed == 1000 )
	    val |= CTL_SPEED_1000;
	else if ( link_speed == 10 )
	    val |= CTL_SPEED_10;
	else /* 100 */
	    val |= CTL_SPEED_100;

	ep->ctl0 = val;
}

#ifdef PHY_JUNK

static void
phy_show ( void )
{
        printf ( "PHY status BMCR: %04x, BMSR: %04x\n",
	    phy_read ( PHY_BMCR ), phy_read ( PHY_BMSR ) );
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
#endif

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

	/* Clears in 1 millesecond (it is not clear that it ever starts) */
        printf ( "PHY reset cleared in %d milliseconds\n", (RESET_TIMEOUT-tmo) );

	// phy_show ();
}

/* Works fine */
static void 
phy_id ( void )
{
	unsigned int id;

        id = phy_read ( PHY_ID1 ) << 16;
        id |= phy_read ( PHY_ID2 );

	// returns 0x00441400
	// printf ( "PHY ID = %04x\n", id );
}

static void
phy_init ( void )
{
	struct emac *ep = EMAC_BASE;

	// printf ( "Starting Phy Init\n" );

#ifdef notyet
	ep->mii_cmd = 0x10;	/* perform reset */
	/* delay ?? */
	ep->mii_cmd = MII_DIV;  /* Set clock divisor */
#endif

	/* We won't reset it and leave well enough alone for now.
	 * We won't even try to start autonegotiation.
	 */
	// phy_reset ();
	// phy_id ();
	// phy_aneg ();

	phy_set_link ();

	// printf ( "Finished with Phy Init\n" );
}

/* ------------------------------------------------------------ */
/* Descriptors */
/* ------------------------------------------------------------ */

#define NUM_RX	10
#define NUM_TX	32

/* There are notes in the U-Boot driver that setting the value 2048
 * causes weird behavior and something less like 2044 should be used
 * On the other hand, we need a DMA aligned buffer, so both values
 * have relevance.
 * The behavior when you do set 2048 in the size field is that when
 * a packet arrives, the DMA runs through every available buffer
 * putting nothing in any of them.
 */
#define RX_SIZE		2048
#define TX_SIZE		2048
#define RX_ETH_SIZE	2044

#ifdef notdef
// static char rx_buffers[NUM_RX * RX_SIZE];
static char rx_buffers[NUM_RX * RX_SIZE] __aligned(ARM_DMA_ALIGN);

static struct emac_desc rx_desc[NUM_RX];
#endif

static void
rx_list_show ( struct emac_desc *desc, int num )
{
	int i;
	struct emac_desc *edp;
	int len;

	invalidate_dcache_range ( (void *) desc, &desc[num] );

	for ( i=0; i<num; i++ ) {
	    edp = &desc[i];
	    len = (edp->status >> 16) & 0x3fff;
	    printf ( "Buf %d (%08x) status: %08x  %d/%d %08x %08x\n", i, edp, edp->status, len, edp->size, edp->buf, edp->next );
	}
}

static void
tx_list_show ( struct emac_desc *desc, int num )
{
	struct emac_desc *edp;
	int len;
	int i;

	invalidate_dcache_range ( (void *) desc, &desc[num] );

	for ( i=0; i<num; i++ ) {
	    edp = &desc[i];
	    printf ( "Buf %d (%08x) status: %08x %08x  %08x %08x\n", i, edp, edp->status, edp->size, edp->buf, edp->next );
	}
}

static struct emac_desc *
rx_list_init ( void )
{
	int i;
	struct emac_desc *edp;
	char *buf;
	struct emac_desc *desc;
	char *mem;

	// printf ( "Descriptor size: %d bytes\n", sizeof(struct emac_desc) );

	/* This gives us a full megabyte of memory with
	 * caching disabled.
	 */
	mem = (char *) ram_section_nocache ( 1 );
	// buf = rx_buffers;
	buf = mem;
	desc = (struct emac_desc *) (mem + NUM_RX * RX_SIZE);

	for ( i=0; i<NUM_RX; i++ ) {
	    edp = &desc[i];
	    edp->status = DS_ACTIVE;
	    edp->size = RX_ETH_SIZE;
	    edp->buf = buf;
	    edp->next = &desc[i+1];
	    buf += RX_SIZE;
	}

	desc[NUM_RX-1].next = &desc[0];

	// flush_dcache_range ( (void *) desc, &desc[NUM_RX] );
	// rx_list_show ( desc, NUM_RX );

	return desc;
}

static struct emac_desc *
tx_list_init ( void )
{
	int i;
	struct emac_desc *edp;
	struct emac_desc *desc;
	char *buf;
	char *mem;

	mem = (char *) ram_alloc ( NUM_TX * (TX_SIZE + sizeof(struct emac_desc)) );
	buf = mem;
	desc = (struct emac_desc *) (mem + NUM_TX * TX_SIZE);

	for ( i=0; i<NUM_TX; i++ ) {
	    edp = &desc[i];
	    edp->next = &desc[i+1];
	    // edp->status = DS_ACTIVE;
	    edp->status = 0;
	    edp->size = 0;
	    edp->buf = buf;
	    buf += TX_SIZE;
	}

	desc[NUM_TX-1].next = &desc[0];

	flush_dcache_range ( (void *) desc, &desc[NUM_TX] );

	return desc;
}

/* ------------------------------------------------------------ */
/* Interrupts */
/* ------------------------------------------------------------ */

static struct emac_desc *rx_list;
static struct emac_desc *tx_list;
static struct emac_desc *cur_rx_dma;
static struct emac_desc *cur_tx_dma;
static int rx_count = 0;
static int tx_count = 0;

#include <net/net.h> 

#define ONE_LINE

static void
emac_show_packet ( struct netbuf *nbp )
{
#ifdef ONE_LINE
        printf ( "packet %d, %d bytes", rx_count, nbp->elen );
        printf ( " ether src: %s ## dst: %s", ether2str(nbp->eptr->src),
	    ether2str(nbp->eptr->dst) );
        if ( nbp->eptr->type == ETH_ARP_SWAP )
            printf ( " (ARP)" );
        else if ( nbp->eptr->type == ETH_IP_SWAP ) {
	    if ( nbp->iptr->proto == IPPROTO_ICMP )
		printf ( " (IP: ICMP)" );
	    else if ( nbp->iptr->proto == IPPROTO_UDP )
		printf ( " (IP: UDP)" );
	    else if ( nbp->iptr->proto == IPPROTO_TCP )
		printf ( " (IP: TCP)" );
	    else
		printf ( " (IP: %d)", nbp->iptr->proto );
	} else
	    printf ( " ether type: %04x", nbp->eptr->type );
	printf ( "\n" );
#else
        printf ( "emac packet %d, %d bytes, ether type: %04x\n", rx_count, nbp->elen, nbp->eptr->type );

        printf ( "ether src: %s ## dst: %s\n", ether2str(nbp->eptr->src),
	    ether2str(nbp->eptr->dst) );

        if ( nbp->eptr->type == ETH_ARP_SWAP ) {
            printf ( "ARP packet\n" );
        } else if ( nbp->eptr->type == ETH_IP_SWAP ) {
            printf ( "ip src: %s (%08x)\n", ip2strl(nbp->iptr->src), nbp->iptr->src );
            printf ( "ip dst: %s (%08x)\n", ip2strl(nbp->iptr->dst), nbp->iptr->dst );
            printf ( "ip proto: %d\n", nbp->iptr->proto );
        } else
            printf ( "unknown packet type\n" );

        // dump_buf ( nbp->eptr, nbp->elen );
#endif
}

static struct netbuf dump_netbuf;

#define NUM_RX_UBOOT	32
#define UBOOT_PKTSIZE	2048

static void
kyu_receive ( char *buf, int len )
{
	struct netbuf *nbp;

	invalidate_dcache_range ( (unsigned long) buf, (unsigned long) &buf[UBOOT_PKTSIZE] );

        // nbp = netbuf_alloc ();
        // nbp = netbuf_alloc_i ();
        // if ( ! nbp )
        //    return;     /* drop packet */

	nbp = &dump_netbuf;
	nbp->bptr = nbp->data;
        nbp->eptr = (struct eth_hdr *) (nbp->bptr + NETBUF_PREPAD * sizeof(struct netbuf *));
        nbp->iptr = (struct ip_hdr *) ((char *) nbp->eptr + sizeof ( struct eth_hdr ));

        // nbp->elen = len - 4;
        // memcpy ( (char *) nbp->eptr, buf, len - 4 );

        nbp->elen = len;
        memcpy ( (char *) nbp->eptr, buf, len );

        // net_rcv ( nbp );

	emac_show_packet ( nbp );
	// netbuf_free ( nbp );
}

static void
rx_handler ( int stat )
{
	int len;
	// int i_dma;

	// don't invalidate the whole thing ...
	// invalidate_dcache_range ( (void *) rx_list, &rx_list[NUM_RX_UBOOT] );

	/*
	if ( first_int ) {
	    rx_list_show ( (struct emac_desc *) ep->rx_desc, NUM_RX_UBOOT );
	    first_int = 0;
	}
	*/

	invalidate_dcache_range ( (void *) cur_rx_dma, &cur_rx_dma[1] );
	while ( ! (cur_rx_dma->status & DS_ACTIVE) ) {
	    rx_count++;
	    len = (cur_rx_dma->status >> 16) & 0x3fff;
	    /*
	    i_dma = (cur_rx_dma - rx_list);
	    printf ( " ---- buf %d (%08x) status: %08x  %d/%d %08x %08x\n",
		i_dma, cur_rx_dma, cur_rx_dma->status, len, cur_rx_dma->size, cur_rx_dma->buf, cur_rx_dma->next );
		*/
	    // kyu_receive ( cur_rx_dma->buf, len );
	    cur_rx_dma->status = DS_ACTIVE;
	    flush_dcache_range ( (void *) cur_rx_dma, &cur_rx_dma[1] );

	    cur_rx_dma = cur_rx_dma->next;
	    invalidate_dcache_range ( (void *) cur_rx_dma, &cur_rx_dma[1] );
	}

	// don't flush the whole thing ...
	// flush_dcache_range ( (void *) rx_list, &rx_list[NUM_RX_UBOOT] );
}

static void
tx_handler ( int stat )
{
	printf ( "emac tx interrupt %08x\n", stat );
}

static int int_count = 0;	/* not currently used */
static int first_int = 1;

/* Interrupt handler.
 * When just receiving packets we see 0x40000100
 *  no telling what the 0x40000000 bit is.
 *  we expect the 0x100 (RX_INT).
 *  eventually we also expect 0x001 (TX_INT).
 */
static void
emac_handler ( int junk )
{
	struct emac *ep = EMAC_BASE;
	int stat;

	int_count++;

	stat = ep->int_stat;

	// printf ( "emac interrupt %08x\n", stat );

	if ( stat & INT_RX_ALL )
	    rx_handler ( stat );

	if ( stat & INT_TX_ALL )
	    tx_handler ( stat );

	/* Ack the IRQ in the emac */
	/* experiment shows this to be necessary and correct */
	ep->int_stat = ep->int_stat & 0xffff;
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

#ifdef notdef
/* This doesn't give the same as we see Wireshark pick up */
static void
get_mac ( char *mac_addr )
{
	mac_addr[0] = 0x02;
	mac_addr[1] = (sid[0] >>  0) & 0xff;
	mac_addr[2] = (sid[3] >> 24) & 0xff;
	mac_addr[3] = (sid[3] >> 16) & 0xff;
	mac_addr[4] = (sid[3] >>  8) & 0xff;
	mac_addr[5] = (sid[3] >>  0) & 0xff;
}
#endif

/* Wireshark shows that my development board has the MAC address:
 * 02:20:7f:9B:26:8c
 * Just jamming these values will do for now ...
 */
static void
get_mac ( char *x )
{
	x[0] = 0x02;
	x[1] = 0x20;
	x[2] = 0x7f;
	x[3] = 0x9b;
	x[4] = 0x26;
	x[5] = 0x8c;
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
verify_regs ( void )
{
	struct emac *ep = EMAC_BASE;

	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0xff;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );
	ep->mac_addr[0].hi = 0;
	printf ( "MAC test = %08x\n", ep->mac_addr[0].hi );

	/* Verify structure */
	ep = (struct emac *) 0;
	printf ( "MII data = %08x\n", &ep->mii_data );
	printf ( "MII cmd  = %08x\n", &ep->mii_cmd );
}

static int
emac_init_raw ( void )
{
	struct emac *ep = EMAC_BASE;
	void *list;
	char mac_buf[6];

	// validate my structure layout
	// printf ( "Shoud be 0xd0 == 0x%x\n", &ep->rgmii_status );
	// verify_regs ();

	/* We inherit clocks and such from U-boot, thus far anyway ... */
	// ccu_emac ();

	// phy_uboot ();
	// verify_regs ();
	//syscon_setup ();
	// show_sid ();

	printf ( "Emac init\n" );
	// printf ( " *************************** Hello from the Emac driver\n" );

	emac_reset ();
	get_mac ( mac_buf );
	set_mac ( mac_buf );

	phy_init ();			/* sets ctl0 */

	ep->ctl1 = 0x08000000;		/* Burst len = 8 */

	ep->rx_ctl1 |= RX_MD;

	ep->rx_desc = rx_list_init ();
	// ep->tx_desc = tx_list_init ();

	irq_hookup ( IRQ_EMAC, emac_handler, 0 );

	printf ( "emac rx_filt = %08x\n", ep->rx_filt );

	// ep->int_ena = INT_RX_ALL;

	ep->rx_ctl1 |= RX_DMA_ENA;
	ep->rx_ctl0 |= RX_EN;

	// ep->tx_ctl1 |= TX_DMA_ENA;
	// ep->tx_ctl0 = TX_EN;

	ep->rx_filt = RX_FILT_DIS;

	printf ( "emac rx_desc = %08x\n", ep->rx_desc );
	printf ( "emac tx_desc = %08x\n", ep->tx_desc );

	printf ( "emac CTL0 = %08x\n", ep->ctl0 );
	printf ( "emac CTL1 = %08x\n", ep->ctl1 );
	printf ( "emac RX CTL0 = %08x\n", ep->rx_ctl0 );
	printf ( "emac RX CTL1 = %08x\n", ep->rx_ctl1 );
	printf ( "emac TX CTL0 = %08x\n", ep->tx_ctl0 );
	printf ( "emac TX CTL1 = %08x\n", ep->tx_ctl1 );

        return 1;
}

static void
reset_rx_list ( struct emac_desc *list, int num )
{
	struct emac_desc *edp;

	list->status = DS_ACTIVE;
	for ( edp = list->next; edp != list; edp = edp->next )
	    edp->status = DS_ACTIVE;

/* Doing this will trigger a bug in the emac silicon.
 * with the size field set to 2048, it will run through all
 * of the available buffers putting nothing in any of them.
 */
#ifdef notdef
	/* XXX */
	list->size = 2048;
	for ( edp = list->next; edp != list; edp = edp->next )
	    edp->size = 2048;
#endif

	flush_dcache_range ( (void *) list, &list[num] );
}

static char emac_mac[ETH_ADDR_SIZE];

static void
fetch_uboot_mac ( void )
{
	struct emac *ep = EMAC_BASE;
	char *addr = emac_mac;

	unsigned long mac_hi = ep->mac_addr[0].hi;
	unsigned long mac_lo = ep->mac_addr[0].lo;

	/* This is just from good old U-Boot
	printf ( "MAC addr %d: %08x %08x\n", ep->mac_addr[0].hi, ep->mac_addr[0].lo );
	MAC addr 0: 00008c26 9b7f2002
	*/
	*addr++ = mac_lo & 0xff;
	mac_lo >>= 8;
	*addr++ = mac_lo & 0xff;
	mac_lo >>= 8;
	*addr++ = mac_lo & 0xff;
	mac_lo >>= 8;
	*addr++ = mac_lo & 0xff;

	*addr++ = mac_hi & 0xff;
	mac_lo >>= 8;
	*addr++ = mac_hi & 0xff;
}

/* We have been pulling our hair out getting the emac properly
 * initialized from scratch, so as an "end run" approach, we want
 * to just try using it as we inherit the initialization from U-Boot
 */
static int
emac_init_uboot ( void )
{
	struct emac *ep = EMAC_BASE;
	int i;
	void *desc;

	printf ( "emac rx_desc = %08x\n", ep->rx_desc );
	printf ( "emac tx_desc = %08x\n", ep->tx_desc );

	// rx_list_show ( (struct emac_desc *) ep->rx_desc, NUM_RX_UBOOT );

	printf ( "emac CTL0 = %08x\n", ep->ctl0 );
	printf ( "emac CTL1 = %08x\n", ep->ctl1 );
	printf ( "emac RX CTL0 = %08x\n", ep->rx_ctl0 );
	printf ( "emac RX CTL1 = %08x\n", ep->rx_ctl1 );
	printf ( "emac TX CTL0 = %08x\n", ep->tx_ctl0 );
	printf ( "emac TX CTL1 = %08x\n", ep->tx_ctl1 );

	for ( i=0; i<8; i++ ) {
	    printf ( "MAC addr %d: %08x %08x\n", 
		i, ep->mac_addr[i].hi, ep->mac_addr[i].lo );
	}
	/* The mac address registers read as all ones, except the first.
	 * MAC addr 0: 00008c26 9b7f2002
	 * MAC addr 0: 00 00 8c 26 9b 7f 20 02
	 * our MAC address on the wire is: 02:20:7f:9b:26:8c
	 */
	fetch_uboot_mac ();

	/* Shut down receiver and receive DMA */
	ep->rx_ctl0 &= ~RX_EN;
	ep->rx_ctl1 &= ~RX_DMA_ENA;

	/* Shut down transmitter and transmit DMA */
	ep->tx_ctl0 &= ~TX_EN;
	ep->tx_ctl1 &= ~TX_DMA_ENA;

	/* Set up interrupts */
	irq_hookup ( IRQ_EMAC, emac_handler, 0 );

	/* Find the list that U-Boot left for us */
	desc = (void *) ep->rx_desc;
	rx_list = desc;

	reset_rx_list ( desc, NUM_RX_UBOOT );

	/* Reload the dma pointer register.
	 * This causes the dma list pointer to get reset. 
	 */
	ep->rx_desc = desc;
	cur_rx_dma = desc;

	// rx_list_show ( (struct emac_desc *) ep->rx_desc, NUM_RX_UBOOT );

	/* Now set up the Tx list */
	desc = tx_list_init ();
	tx_list = desc;

	ep->tx_desc = desc;
	cur_tx_dma = desc;

	return 1;
}

/* Get things going.
 * Called by emac_activate()
 */
static void
emac_enable ( void )
{
	struct emac *ep = EMAC_BASE;

	/* Enable interrupts */
	ep->int_ena = INT_RX_ALL | INT_TX_ALL;

	/* Restart Rx DMA */
	ep->rx_ctl1 |= RX_DMA_ENA;

	/* Enable the receiver */
	ep->rx_ctl0 |= RX_EN;
}

static void
tx_restart ( void )
{
	struct emac *ep = EMAC_BASE;

	/* Restart Tx DMA */
	ep->tx_ctl1 |= TX_DMA_START | TX_DMA_ENA;

	/* Enable the transmitter */
	ep->tx_ctl0 |= TX_EN;
}

#ifdef notdef
/* Stuff pertaining to U-Boot harvesting */

/* Figure out where list activity starts, if there is any.
 * Note that DMA is active while this is running,
 * so things can change out from under us.
 * This is not an issue unless the list fills since
 * DMA fiddles with the end while we look for the start.
 */
static int
scan_rcv_list ( struct emac_desc *list )
{
	struct emac_desc *edp;
	int index = -1;
	int ready0 = 0;
	int wait = 0;
	int i;
	int num;

	if ( ! (list->status & DS_ACTIVE) ) {
	    ready0 = 1;
	    wait = 1;
	}

	i = 0;
	for ( edp = list;; edp = edp->next ) {
	    if ( edp->next == list )
		break;
	    if ( wait && ! (edp->status & DS_ACTIVE) )
		continue;
	    wait = 0;
	    if ( ! (edp->status & DS_ACTIVE) ) {
		index = i;
		break;
	    }
	    i++;
	}

	/* detect idle list */
	if ( index < 0 && ready0 == 0 ) {
	    printf ( "Scan finds idle list\n" );
	    return -1;
	}

	/* detect full list */
	if ( index < 0 && ready0 == 1 && wait == 1 ) {
	    num = i;
	    printf ( "Rcv list found full with %d entries\n", num );
	    for ( i=0; i<num; i++ )
		list[i].status = DS_ACTIVE;
	    return -1;
	}

	// printf ( "scan ends with %d %d\n", index, ready0 );
	if ( index < 0 )
	    return 0;
	return index;
}

static int first_poll = 1;
static struct emac_desc *cur_rx_dma;
static struct emac_desc *cur_tx_dma;

static void
emac_rcv_poll ( void )
{
	struct emac *ep = EMAC_BASE;
	struct emac_desc *list;
	struct emac_desc *edp;
	int index;
	int len;
	int i_dma;

	list = (struct emac_desc *) ep->rx_desc;

	invalidate_dcache_range ( (void *) list, &list[NUM_RX_UBOOT] );

	// rx_list_show ( (struct emac_desc *) ep->rx_desc, NUM_RX_UBOOT );

	if ( first_poll ) {
	    index = scan_rcv_list ( list );
	    if ( index < 0 ) {
		printf ( "Rx list empty at startup\n" );
		return;
	    }
	    cur_rx_dma = &list[index];
	    first_poll = 0;
	}

	if ( cur_rx_dma->status & DS_ACTIVE ) {
	    // rx_list_show ( (struct emac_desc *) ep->rx_desc, NUM_RX_UBOOT );
	    // printf ( "Rx list empty on poll\n" );
	    return;
	}

	while ( ! (cur_rx_dma->status & DS_ACTIVE) ) {
	    i_dma = (cur_rx_dma - list);
	    len = (cur_rx_dma->status >> 16) & 0x3fff;
	    printf ( " ---- buf %d (%08x) status: %08x  %d/%d %08x %08x\n",
		i_dma, cur_rx_dma, cur_rx_dma->status, len, cur_rx_dma->size, cur_rx_dma->buf, cur_rx_dma->next );
	    kyu_receive ( cur_rx_dma->buf, len );
	    cur_rx_dma->status = DS_ACTIVE;
	    cur_rx_dma = cur_rx_dma->next;
	}
}
#endif

/* Not truly official -- called at 1 Hz from "net_slow"
 */
void
emac_poll ( void )
{
	// Don't need this is we are using interrupts
	// emac_rcv_poll ();
}

/* My orange Pi seems to be 196.168.0.61
 * with MAC address 02:20:7f:9b:26:8c
 */

/* These are the "official" production entry points to this driver.
 */

int
emac_init ( void )
{
	// return emac_init_raw ();
	return emac_init_uboot ();
}

void
emac_activate ( void )
{
	printf ( "Emac activate\n" );

	emac_enable ();
}

void
emac_show ( void )
{
	printf ( "Emac int count: %d\n", int_count );
	printf ( "Emac rx_count: %d\n", rx_count );
	printf ( "Emac tx_count: %d\n", tx_count );

	// rx_list_show ( rx_list, NUM_RX_UBOOT );
	tx_list_show ( tx_list, NUM_TX );
}

void
get_emac_addr ( char *addr )
{
	memcpy ( addr, emac_mac, ETH_ADDR_SIZE );
}

#ifdef notdef
struct emac_desc {
	unsigned long status;	/* status */
	long size;		/* st */
	char * buf;		/* buf_addr */
	struct emac_desc *next;	/* next */
}	__aligned(ARM_DMA_ALIGN);
#endif

void
emac_send ( struct netbuf *nbp )
{
        int len;

        ++tx_count;

        /* Put our ethernet address in the packet */
        memcpy ( nbp->eptr->src, emac_mac, ETH_ADDR_SIZE );

        len = nbp->ilen + sizeof(struct eth_hdr);

        // if ( len < ETH_MIN_SIZE ) len = ETH_MIN_SIZE;

	memcpy ( cur_tx_dma->buf, (char *) nbp->eptr, len );
	flush_dcache_range ( (unsigned long ) cur_tx_dma->buf, (unsigned long) cur_tx_dma->buf + len );

	cur_tx_dma->size = len | DSS_MAGIC;
	cur_tx_dma->status = DS_ACTIVE;
	flush_dcache_range ( (void *) cur_rx_dma, &cur_rx_dma[1] );

	cur_tx_dma = cur_tx_dma->next;

	tx_restart ();
}

/* THE END */
