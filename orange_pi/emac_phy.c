/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * emac_phy.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/10/2023
 *
 * Ethernet phy driver for the Allwinner H3
 *
 * The Orange PI PC and the PC Plus both use the PHY internal to the
 *   H3 chip and thus support only 10/100 ethernet.
 *
 * The Orange Pi Plus (not the PC Plus) also uses the H3 chip,
 *   but has an external PHY and thus can do gigabit.
 *
 * This currently just supports the internal phy
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
#include <h3_ints.h>
#include <arch/cpu.h>

#include "emac_regs.h"

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

/* Bits in the basic mode control register. */
#define BMCR_RESET              0x8000
#define BMCR_LOOPBACK           0x4000
#define BMCR_100                0x2000		/* Set for 100 Mbit, else 10 */
#define BMCR_ANEG_ENA           0x1000		/* enable autonegotiation */
#define BMCR_POWERDOWN          0x0800		/* set to power down */
#define BMCR_ISOLATE            0x0400
#define BMCR_ANEG               0x0200		/* restart autonegotiation */
#define BMCR_FULL               0x0100		/* Set for full, else half */
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

/*
 * I get identical results from both address 0 and 1
 * U-boot sets it to 1, so I will also.
 * Code that does a phy_reset seems to set this to 0 though.
 *
 * This gives us the ethernet PHY built into the H3 chip.
 * For Orange Pi variants that support 1G, we would need to
 * address an external PHY, perhaps the Realtek RTL8211E.
 * Note that setting device 0 or 1 gives the same results
 *  other addresses (like 2 or 9) read all ones
 *
 * Why does syscon also need/use the PHY address?
 *  Whatever the case we make it match PHY_DEV
 *
 * Also, for whatever it is worth, I get the following from
 *  the U-boot console:
 *
 *  => mdio list
 * ethernet@1c30000:
 * 1 - Generic PHY <--> ethernet@1c30000
 * => mii device
 * MII devices: 'ethernet@1c30000'
 * Current device: 'ethernet@1c30000'
 * => mii info
 * PHY 0x00: OUI = 0x50000, Model = 0x00, Rev = 0x00,  10baseT, HDX
 * PHY 0x01: OUI = 0x50000, Model = 0x00, Rev = 0x00,  10baseT, HDX
 *
 */

#define PHY_DEV			1
#define SYSCON_PHY_ADDR		1

//#define PHY_DEV			0
//#define SYSCON_PHY_ADDR		0

void phy_init ( void );
void phy_update ( void );

static void emac_syscon_setup ( void );
static int phy_id ( void );
static void phy_show ( void );
static void phy_reset ( void );
static void phy_autonegotiate ( void );
static void phy_link_status ( void );
static void phy_link_setup ( void );

static int phy_read ( int );
static void phy_write ( int, int );

static int phy_debug = 0;

#define DUPLEX_HALF             0x00
#define DUPLEX_FULL             0x01

/* These values should be overwritten by autonegotiation
 */
static int phy_speed = 100;
static int phy_duplex = DUPLEX_FULL;

int
phy_get_speed ( void )
{
	return phy_speed == 100 ? 1 : 0;
}

int
phy_get_duplex ( void )
{
	return phy_duplex;
}

void
phy_init ( void )
{
	int id;

	printf ( "PHY for emac\n" );
	emac_syscon_setup ();

	phy_show ();

	//phy_write ( PHY_ADVERT, 0 );
	// phy_show ();

	//phy_debug = 1;
        // printf ( "PHY ID = %04x\n", phy_id() );

	printf ( "Start PHY reset\n" );
	phy_reset ();
        // printf ( "PHY ID = %04x\n", phy_id() );
	//phy_debug = 0;

	phy_show ();

	printf ( "Start autonegotiation\n" );
	phy_autonegotiate ();

	phy_show ();

	phy_update ();

	phy_link_setup ();
}

static void
phy_show ( void )
{
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
}

static void
phy_link_setup ( void )
{
	int reg;

        reg = phy_read ( PHY_BMCR );
	reg &= (BMCR_100|BMCR_FULL);

	if ( phy_speed ==100 )
	    reg |= BMCR_100;
	if ( phy_duplex == DUPLEX_FULL )
	    reg |= BMCR_FULL;
        phy_write ( PHY_BMCR, reg );
}

void
phy_update ( void )
{
	phy_link_status ();

	printf ( "Link detected at " );
	if ( phy_speed ==100 )
	    printf ( "100" );
	else
	    printf ( "10" );
	if ( phy_duplex == DUPLEX_FULL )
	    printf ( " Full duplex\n" );
	else
	    printf ( " Half duplex\n" );

}

static void
phy_link_status ( void )
{
        int match;

        phy_speed = 10;
        phy_duplex = DUPLEX_HALF;

        match = phy_read ( PHY_ADVERT ) &
		phy_read ( PHY_PEER );

        if ( match & (LPA_100FULL | LPA_100HALF) )
            phy_speed = 100;

        if ( match & (LPA_100FULL | LPA_10FULL) )
            phy_duplex = DUPLEX_FULL;
}


#define ANEG_TIMEOUT	5000	/* 5 seconds */

static void
phy_autonegotiate ( void )
{
        int reg;
        int tmo = ANEG_TIMEOUT;

        reg = phy_read ( PHY_BMCR );

	reg |= BMCR_ANEG_ENA;
        phy_write ( PHY_BMCR, reg );

	/* this will self clear */
	reg |= BMCR_ANEG;
        phy_write ( PHY_BMCR, reg );

	printf ( "autonegotion started: BMSR  = %04x\n", phy_read ( PHY_BMSR ) );

        while ( tmo-- && ! (phy_read(PHY_BMSR) & BMSR_ANEGCOMPLETE ) )
	    thr_delay ( 2 );

        if ( ! (phy_read(PHY_BMSR) & BMSR_ANEGCOMPLETE ) )
	    printf ( "Autonegotiation fails\n" );
	else {
	    printf ( "autonegotion done: BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
	    printf ( "PHY autonegotion done in %d milliseconds\n", 2*(ANEG_TIMEOUT-tmo) );
	    // I see 2.9 seconds, 3.0 seconds 
	}

	reg &= ~BMCR_ANEG_ENA;
        phy_write ( PHY_BMCR, reg );
}

/* For some reason I thought this would take a long time,
 * the bit clears almost immediately.
 */
#define RESET_TIMEOUT	1000

static void
phy_reset ( void )
{
        int reg;
        int tmo = RESET_TIMEOUT;

	/* start reset, this bit self clears */
        reg = phy_read ( PHY_BMCR );
        phy_write ( PHY_BMCR, reg | BMCR_RESET );

        while ( tmo-- && (phy_read(PHY_BMCR) & BMCR_RESET ) )
	    ;

        // printf ( "PHY reset cleared in %d counts\n", (RESET_TIMEOUT-tmo) );
	if ( phy_read(PHY_BMCR) & BMCR_RESET )
	    printf ( "PHY reset fails\n" );
}

/* With the cpu clock at 1008 Mhz, this polls 90 times
 */
#define SPIN_TIMEOUT	1000

static void
phy_spin ( void )
{
	struct emac *ep = EMAC_BASE;
	int tmo = SPIN_TIMEOUT;

	/* polls consistently 41 times */
	while ( ep->mii_cmd & MII_BUSY && tmo-- )
	    ;

	// printf ( "phy_spin count = %d\n", tmo );
	if ( ep->mii_cmd & MII_BUSY  )
	    printf ( "PHY spin timeout\n" );
}

static int
phy_read ( int reg )
{
	struct emac *ep = EMAC_BASE;
	unsigned int val1, val2;
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
	// val1 = ep->mii_data;

#ifdef DOUBLE_READ
	// long long ago, I was confused and it seemed that I had to read each
	// register twice and toss the first read, but that is not the case.
	// printf ( "phy_read A called: reg, cmd, val = %d, %04x, %04x\n", reg, cmd, val1 );

	phy_spin();
	ep->mii_cmd = cmd;
	phy_spin();
	val2 = ep->mii_data;

	// printf ( "phy_read B called: reg, cmd, val = %d, %04x, %04x\n", reg, cmd, val1 );

	// this check can false trigger, for example during autonegotiation
	// the link transitions from down to up at the end and tricks this check.
	if ( val1 != val2 ) {
	    printf ( "Mismatch in phy_read()\n" );
	    printf ( "phy_read A: reg, cmd, val = %d, %04x, %04x\n", reg, cmd, val1 );
	    printf ( "phy_read B: reg, cmd, val = %d, %04x, %04x\n", reg, cmd, val2 );
	}

	return val1;
#endif

	return ep->mii_data;
}

/* no double stuff here like read.  Should there be?
 */
static void
phy_write ( int reg, int val )
{
	struct emac *ep = EMAC_BASE;
	unsigned int cmd;

	// ep->mii_cmd = (2 << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;
	// ep->mii_cmd = (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;

	cmd = MII_DIV | (PHY_DEV << MII_DEV_SHIFT) | (reg << MII_REG_SHIFT) | MII_BUSY | MII_WRITE;

	if ( phy_debug )
	    printf ( "phy_write called %d, %04x, %04x\n", reg, val, cmd );

	phy_spin ();
	/* Post the data, then the command to write it */
	ep->mii_data = val;
	ep->mii_cmd = cmd;
	phy_spin ();

	return;
}

/* Works fine */
/* Returns 0x441400 for ADDR 0 or 1 */
static int
phy_id ( void )
{
        unsigned int id;

        id = phy_read ( PHY_ID1 ) << 16;
        id |= phy_read ( PHY_ID2 );
	return id;
}


/* ------------------------------------------------------------ */
/* Syscon stuff */
/* ------------------------------------------------------------ */

#define EMAC_SYSCON	((unsigned int *) 0x01c00030)

/* Syscon is peculiar in that it only has this one register for
 * controlling EMAC stuff, along with a chip version register.
 * This single 32 bit register controlls EMAC phy mux and such.
 */
/* These are only some of the bit definitions, but more than we need */

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
 * I see: Emac Syscon =     00148000 before and after
 * the datasheet default is 00058000
 * The "1" is the PHY ADDR
 * The "5" going to "4" is clearing the shutdown bit.
 *
 * So even calling or having this is pointless, given what U-Boot
 * passes to us, but it is just "clean living" to perform all
 * of our own initialization.
 */
static void
emac_syscon_setup ( void )
{
	volatile unsigned int *sc = EMAC_SYSCON;

	// printf ( "Emac Syscon = %08x\n", *sc );

	*sc = SYSCON_EPHY_INTERNAL | SYSCON_CLK24 | (SYSCON_PHY_ADDR<<20);

	// printf ( "Emac Syscon = %08x\n", *sc );
}

/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */

/* OLD below here */

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
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
	*/
}
#endif

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

	// printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	// printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

	/* XXX - usually this would be set if autonegotiation
	 * worked or not.
	 */
	link_good = 1;

        link_speed = 10;
        link_duplex = DUPLEX_HALF;

        val = phy_read ( PHY_ADVERT ) & phy_read ( PHY_PEER );

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
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
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

	phy_write ( PHY_ADVERT, 0 );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	phy_write ( PHY_ADVERT, 0 );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	phy_write ( PHY_ADVERT, LPA_ADVERT );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	phy_write ( PHY_ADVERT, LPA_ADVERT );
	printf ( "I set: ADV   = %04x\n", phy_read ( PHY_ADVERT ) );

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
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

	phy_show ();
	link_good = 1;
	phy_set_link ();
}
#endif

static void
phy_init_OLD ( void )
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


/* THE END */
