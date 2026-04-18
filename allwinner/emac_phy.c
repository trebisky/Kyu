/*
 * Copyright (C) 2016, 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * emac_phy.c
 *
 * Ethernet phy driver for the Allwinner H3 and H5
 *
 * Tom Trebisky  1/10/2023 4/14/2026
 *
 * The Orange PI PC and the PC Plus both use the PHY internal to the
 *   H3 chip and thus support only 10/100 ethernet.
 * The Orange Pi Plus (not the PC Plus) also uses the H3 chip,
 *   but has an external PHY and thus can do gigabit.
 * The Orange Pi PC2 has the H5 chip and also uses
 *   an external PHY to do gigabit (the Realtek 8211E)
 *
 * This began as a driver to only support the internal phy
 *  on the H3 chip, but has now (April, 2026) been extended
 *  to also support the H5 with the external 8211E.
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
#include <interrupts.h>
#include <arch/cpu.h>

#include "emac_regs.h"

static int phy_debug = 0;

#define PHY_DEV				1
//#define PHY_DEV			0

/* ------------------------------------------------------------------- */
/* First, a handful of emac specific MDIO routines.
 */

/* Two registers among the EMAC registers are used to access the PHY
 * MDIO, namely: mii_cmd and mii_data
 */

/* Bits in the mii_cmd register */

#define MII_BUSY		0x01
#define MII_WRITE		0x02
#define MII_REG			0x1f0		/* 5 bits for which register */
#define MII_REG_SHIFT	4
#define MII_DEV			0x1f000		/* 5 bits for which device */
#define MII_DEV_SHIFT	12

#define MII_DIV_MASK	0x700000	/* 3 bits for scaler */
#define MII_DIV_16		0
#define MII_DIV_32		0x100000
#define MII_DIV_64		0x200000
#define MII_DIV_128		0x300000	/* linux uses this */

#define MII_DIV		MII_DIV_128

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

/* ------------------------------------------------------------------- */
/* Now general PHY code that would work on any hardware
 * once MDIO access routines (like thos above) were defined.
 */

/* Here are the registers in the PHY device that we use.
 * The first 16 PHY registers are standardized and device
 * independent
 */
#define PHY_BMCR        0
#define PHY_BMSR        1
#define PHY_ID1         2
#define PHY_ID2         3
#define PHY_ADVERT      4
#define PHY_PEER        5

#define PHY_CTRL1000    9
#define PHY_STAT1000    10

#define CT1000_HD1000	0x0100
#define CT1000_FD1000	0x0200

#define ST1000_HD1000	0x0400
#define ST1000_FD1000	0x0800

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

/*
 * I get identical results from both address 0 and 1
 * U-boot sets it to 1, so I will also.
 * Code that does a phy_reset seems to set this to 0 though.
 * Also a study of NetBSD code shows it sets the syscon address to 1.
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


void phy_init ( void );
void phy_update ( void );
void phy_show ( void );
void phy_show_state ( void );

/* in emac_syscon.c */
void emac_syscon_setup ( void );

static int phy_id ( void );
static void phy_reset ( void );
static void phy_autonegotiate ( void );
static void phy_set_link_status ( int );
static void phy_link_setup ( void );

static int phy_read ( int );
static void phy_write ( int, int );

#define DUPLEX_HALF             0x00
#define DUPLEX_FULL             0x01

/* These values should be overwritten by autonegotiation
 */
// static int phy_speed = 100;
static int phy_speed = 999;
static int phy_duplex = DUPLEX_FULL;
static int phy_up = 0;

int
phy_get_speed ( void )
{
	return phy_speed;
}

int
phy_get_duplex ( void )
{
	return phy_duplex;
}

int
phy_get_link ( void )
{
	return phy_up;
}

void
phy_init ( void )
{
	int id;

	printf ( "PHY for emac\n" );
	emac_syscon_setup ();

	printf ( "Setup we inherited from U-Boot:\n" );
	phy_show ();
	phy_update ();

	//phy_write ( PHY_ADVERT, 0 );
	// phy_show ();

	// phy_debug = 1;

	printf ( "Start PHY reset\n" );
	phy_reset ();
        // printf ( "PHY ID = %04x\n", phy_id() );
	//phy_debug = 0;

	phy_show ();

	printf ( "Start autonegotiation\n" );
	phy_autonegotiate ();

	phy_show ();

	// phy_update ();
	phy_set_link_status ( 1 );
	phy_show_state ();

	/* set our control reg to reflect
	 * what happened during autonegotiation
	 */
	phy_link_setup ();

	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
}

#ifdef notdef
/* This might have been an interesting experiment once */
/* Initialize at 10 mbit.
 * XXX - does not work, autonegotiation simply fails.
 * We end up with the link down.
 */
void
phy_init_10 ( void )
{
	phy_init_int ( 1 );
}
#endif

void
phy_show ( void )
{
	printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );
	printf ( "BMCR  = %04x\n", phy_read ( PHY_BMCR ) );
	printf ( "BMSR  = %04x\n", phy_read ( PHY_BMSR ) );

	printf ( "PHY ID = %04x\n", phy_id() );

	printf ( "CTRL1000  = %04x\n", phy_read ( PHY_CTRL1000 ) );
	printf ( "STAT1000  = %04x\n", phy_read ( PHY_STAT1000 ) );
}

/* Needs work XXX */
static void
phy_link_setup ( void )
{
	int reg;

	reg = phy_read ( PHY_BMCR );
	reg &= ~(BMCR_100|BMCR_FULL);

	if ( phy_speed == 100 )
	    reg |= BMCR_100;

	if ( phy_duplex == DUPLEX_FULL )
	    reg |= BMCR_FULL;
        phy_write ( PHY_BMCR, reg );
}

void
phy_show_state ( void )
{
	if ( phy_up )
	    printf ( "PHY link detected UP at %d", phy_speed );
	else
	    printf ( "PHY link detected DOWN at %d", phy_speed );

	if ( phy_duplex == DUPLEX_FULL )
	    printf ( " Full duplex\n" );
	else
	    printf ( " Half duplex\n" );
}

void
phy_update ( void )
{
	phy_set_link_status ( 0 );
	phy_show_state ();
}

/* Examine the PHY registers and set local variables to
 * reflect what has been negotiated.
 */
static void
phy_set_link_status ( int autoneg )
{
	int match;
	int gmatch;

	// printf ( "ADV   = %04x\n", phy_read ( PHY_ADVERT ) );
	// printf ( "PEER  = %04x\n", phy_read ( PHY_PEER ) );

	phy_up = 0;
	if ( phy_read( PHY_BMSR ) & BMSR_LINK_UP )
	    phy_up = 1;

	if ( autoneg )
	    match = phy_read ( PHY_ADVERT ) & phy_read ( PHY_PEER );
	else
	    match = phy_read ( PHY_ADVERT );

/* Only check for gigabit on the H5 board with the Realtek 8211 PHY
 * We ignore the idea of 1000 Mbit half duplex.
 */
#ifdef BOARD_H5
	gmatch = phy_read ( PHY_CTRL1000 );
	gmatch &= phy_read ( PHY_STAT1000 ) >> 2;
	if ( gmatch & CT1000_FD1000 ) {
		phy_speed = 1000;
		phy_duplex = DUPLEX_FULL;
		return;
	}
#endif

	if ( match & LPA_100FULL ) {
		phy_speed = 100;
		phy_duplex = DUPLEX_FULL;
		return;
	}

	if ( match & LPA_100HALF ) {
		phy_speed = 100;
		phy_duplex = DUPLEX_HALF;
		return;
	}

	/* Unlikely in this day and age */
	phy_speed = 10;
	phy_duplex = DUPLEX_HALF;
	if ( match & LPA_10FULL )
		phy_duplex = DUPLEX_FULL;
}

#define ANEG_TIMEOUT	5000	/* 5 seconds */

static void
phy_autonegotiate ( void )
{
	int reg;
	int tmo = ANEG_TIMEOUT;

	reg = phy_read ( PHY_BMCR );

	// reg = 0;	// bad
	printf ( "autonegotiation starting, BMCR = %04x\n", reg );
	/* I see 0x3000 at this point, which is what we get after
	 * the PHY reset. However, if I clear these bits,
	 * the autonegotiation doesn't go right and I get all
	 * zeros in the PEER register when it is done.
	 * 0x2000 is the 100 Mbit speed selection
	 * 0x1000 is ANEG enable
	 */

#ifdef notdef
	/* limit speed to 10 Mbit */
	if ( speed_10 ) {
	    printf ( "PHY - limit autoneg speed to 10\n" );
	    reg = phy_read ( PHY_ADVERT );
	    printf ( "ADV   = %04x\n", reg );
	    reg &= ~( LPA_100HALF | LPA_100FULL);
	    printf ( "ADV   = %04x\n", reg );
	    phy_write ( PHY_ADVERT, reg );
	}
#endif

	reg |= BMCR_ANEG_ENA;
        phy_write ( PHY_BMCR, reg );

	/* this will self clear */
	reg |= BMCR_ANEG;
        phy_write ( PHY_BMCR, reg );

	printf ( "autonegotiation started: BMSR  = %04x\n", phy_read ( PHY_BMSR ) );

        while ( tmo-- && ! (phy_read(PHY_BMSR) & BMSR_ANEGCOMPLETE ) )
	    thr_delay ( 2 );

        if ( ! (phy_read(PHY_BMSR) & BMSR_ANEGCOMPLETE ) )
	    printf ( "Autonegotiation fails\n" );
	else {
	    printf ( "autonegotion done: BMSR  = %04x\n", phy_read ( PHY_BMSR ) );
	    printf ( "PHY autonegotion done in %d milliseconds\n", 2*(ANEG_TIMEOUT-tmo) );
	    // I see 2.9 seconds, 3.0 seconds 
	}

	// If we do this (disable ANEG) we run for a little while,
	// but then the link goes down.
	// leave this up and we are fine.
	// 11-13-2023
	// reg &= ~BMCR_ANEG_ENA;
        // phy_write ( PHY_BMCR, reg );
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

/* Works fine */
/* Returns 0x441400 for ADDR 0 or 1 for the h3 (internal PHY) */
/* Returns 0x1cc915 for the RTL8211 external PHY (on my h5 boards)
 *  (this is correct as per RTL8211 datasheet).
 */
static int
phy_id ( void )
{
        unsigned int id;

        id = phy_read ( PHY_ID1 ) << 16;
        id |= phy_read ( PHY_ID2 );

		return id;
}

/* THE END */
