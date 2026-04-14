#include "arch/types.h"
#include "emac_regs.h"

// #define EMAC_BASE	((struct emac *) 0x01c30000)

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
