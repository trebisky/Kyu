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
	volatile unsigned int int_stat;		/* 08 */
	volatile unsigned int int_ena;		/* 0c */
	volatile unsigned int tx_ctl0;		/* 10 */
	volatile unsigned int tx_ctl1;		/* 14 */
	int __pad0;				/* 18 --*/
	volatile unsigned int tx_flow;		/* 1c */
	volatile void * tx_desc;		/* 20 */
	volatile unsigned int rx_ctl0;		/* 24 */
	volatile unsigned int rx_ctl1;		/* 28 */
	int __pad1;				/* 2c --*/
	int __pad2;				/* 30 --*/
	volatile void * rx_desc;		/* 34 */
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
#define	RX_DMA_DIS		0x80000000
#define	RX_DMA_ENA		0x40000000
#define	RX_MD			0x00000002
#define	RX_NO_FLUSH		0x00000001

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
#define DS_CTL		0x80000000	/* set when available for DMA */
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
/* Interrupts */
/* ------------------------------------------------------------ */

static int int_count = 0;

// #define INT_LIMIT	200
#define INT_LIMIT	2

static void rx_list_show ( struct emac_desc * );

static void
emac_handler ( int junk )
{
	struct emac *ep = EMAC_BASE;

	printf ( "emac interrupt %08x\n", ep->int_stat );
	rx_list_show ( (struct emac_desc *) ep->rx_desc );

	int_count++;
	if ( int_count >= INT_LIMIT ) {
	    printf ( "emac ints killed (limit reached)\n" );
	    ep->int_ena = 0;
	}

	/* Ack the IRQ in the emac */
	/* experiment shows this to be necessary and correct */
	ep->int_stat = ep->int_stat & 0xffff;
}

/* ------------------------------------------------------------ */
/* Descriptors */
/* ------------------------------------------------------------ */

#define NUM_RX	10
#define RX_SIZE	2048

#ifdef notdef
// static char rx_buffers[NUM_RX * RX_SIZE];
static char rx_buffers[NUM_RX * RX_SIZE] __aligned(ARM_DMA_ALIGN);

static struct emac_desc rx_desc[NUM_RX];
#endif

static void
rx_list_show ( struct emac_desc *desc )
{
	int i;
	struct emac_desc *edp;
	int len;

	// invalidate_dcache_range ( (void *) desc, &desc[NUM_RX] );

	for ( i=0; i<NUM_RX; i++ ) {
	    edp = &desc[i];
	    len = (edp->status >> 16) & 0x3fff;
	    printf ( "Buf status %2d: %08x  %d/%d (%08x)\n", i, edp->status, len, edp->size, edp->buf );
	}
}

static struct emac_desc *
rx_list_init ( void )
{
	int i;
	struct emac_desc *edp;
	char *buf;
	struct emac_desc *rx_desc;
	char *mem;

	printf ( "Descriptor size: %d bytes\n", sizeof(struct emac_desc) );

	/* This gives us a full megabyte of memory with
	 * caching disabled.
	 */
	mem = (char *) ram_section_nocache ( 1 );
	// buf = rx_buffers;
	buf = mem;
	rx_desc = (struct emac_desc *) (mem + NUM_RX * RX_SIZE);

	for ( i=0; i<NUM_RX; i++ ) {
	    edp = &rx_desc[i];
	    edp->status = DS_CTL;
	    edp->size = RX_SIZE;
	    // edp->size |= 0x01000000;
	    edp->buf = buf;
	    edp->next = &rx_desc[i+1];
	    buf += RX_SIZE;
	}

	rx_desc[NUM_RX-1].next = &rx_desc[0];

	// flush_dcache_range ( (void *) rx_desc, &rx_desc[NUM_RX] );
	rx_list_show ( rx_desc );

	return rx_desc;
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

int
emac_init ( void )
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

	irq_hookup ( IRQ_EMAC, emac_handler, 0 );

	printf ( "emac rx_filt = %08x\n", ep->rx_filt );

	ep->int_ena = INT_RX_ALL;

	ep->rx_ctl1 = RX_DMA_ENA;
	ep->rx_ctl0 = RX_EN;
	ep->rx_filt = RX_FILT_DIS;

	printf ( "emac rx_desc = %08x\n", ep->rx_desc );
	printf ( "emac tx_desc = %08x\n", ep->tx_desc );


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
