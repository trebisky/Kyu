/*
 * CPSW Ethernet Switch Driver
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 * from U-boot sources (2015.01)
 *      drivers/net/cpsw.c
 *      include/cpsw.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* Kyu project
 *
 * Driver for the cpsw ethernet interface in the BBB
 *
 * Tom Trebisky 5-26-2015
 * Derived from the U-boot (2015.01) sources (from cpsw.c)
 *  many other files merged in to make it stand alone, then
 *   reworked heavily and lots of cruft removed.
 *
 * The cpsw is a 3 port ethernet switch capable of gigabit
 *  port 0 is CPPI -- (Communications Port Programming Interface)
 *  port 1 is ethernet (we use this one)
 *  port 2 is ethernet (not connected to anything on the BBB)
 *  on the BBB the port 1 PHY supports only a 10/100 media interface.
 * The media interface is a SMSC "LAN8710" chip.
 *
 * Port 0 is not entirely ignored, so be on your toes.
 */

#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
#include <omap_ints.h>

/* Hardware base addresses */
/* from the TRM, chapter 2, pages 175-176 */

#define CPSW_BASE	0x4a100000	/* Ethernet Switch Subsystem */
#define PORT0_BASE	0x4a100200	/* Port 0 control */
#define PORT1_BASE	0x4a100200	/* Port 1 control */
#define PORT2_BASE	0x4a100300	/* Port 2 control */
#define CPDMA_BASE	0x4a100800	/* DMA */
#define STATS_BASE	0x4a100900	/* Statistics - never used */
#define STATERAM_BASE	0x4a100A00	/* State RAM */
#define CPTS_BASE	0x4a100C00	/* Time sync - never used*/
#define ALE_BASE	0x4a100D00	/* Address Lookup Engine */
#define SLIVER1_BASE	0x4a100D80	/* "sliver" for Port 1 */
#define SLIVER2_BASE	0x4a100DC0	/* "sliver" for Port 2 */
#define MDIO_BASE	0x4a101000	/* MDIO */
#define WR_BASE		0x4a101200	/* RMII/RGMII wrapper - never used */

#define BDRAM_BASE	0x4a102000	/* Communications Port Programming Interface RAM */

#define NUM_CPDMA_CHANNELS	8

/* XXX TRM says 1152 entries */
#define NUM_ALE_ENTRIES		1024

/* This is a 3 port switch,
 * The first port is the mysterious "HOST" or cppi port.
 * The second port is the ethernet port we use.
 * The third port is another ethernet port, not connected on the BBB.
 *
 * This driver shows embryonic signs of support for the second ethernet port,
 *  but noone should be deceived, this is neither complete nor tested.
 */
#define HOST_PORT		0
#define ETH_PORT		1
#define ETH_PORT_EXTRA		2

struct cpdma_desc {
	/* hardware fields */
	struct cpdma_desc *	hw_next;
	void *			hw_buffer;
	u32			hw_len;
	u32			hw_mode;
	/* software fields */
	void *			sw_buffer;
	u32			sw_len;
};

/* Near as I can tell this is entirely software defined */
struct cpdma_chan {
	struct cpdma_desc	*head;
	struct cpdma_desc	*tail;
	volatile unsigned long	*hdp;		/* points into stateram */
	volatile unsigned long    *cp;		/* points into stateram */
	volatile unsigned long    *rxfree;	/* points to cpdma hardware */
};

struct dma_regs {
	volatile unsigned long tx_ver;				/* 00 */
	volatile unsigned long tx_control;
	volatile unsigned long tx_teardown;
	long	_pad;

	volatile unsigned long rx_ver;				/* 10 */
	volatile unsigned long rx_control;
	volatile unsigned long rx_teardown;

	volatile unsigned long soft_reset;
	volatile unsigned long control;
	volatile unsigned long status;				/* 24 */

	long _pad2[46];
	volatile unsigned long rxfree[NUM_CPDMA_CHANNELS];	/* e0 */
};

/* Descriptor mode bits */
#define CPDMA_DESC_SOP		BIT(31)
#define CPDMA_DESC_EOP		BIT(30)
#define CPDMA_DESC_OWNER	BIT(29)
#define CPDMA_DESC_EOQ		BIT(28)

#ifdef notdef
/* Not for us - these are offsets in some older version of the chip */
#define CPDMA_TXHDP_VER1	0x100
#define CPDMA_RXHDP_VER1	0x120
#define CPDMA_TXCP_VER1		0x140
#define CPDMA_RXCP_VER1		0x160

/* These are the offsets we want.
 * These offsets are from the cpdma base, but ...
 * They are actually in the State RAM
 */
#define CPDMA_TXHDP_VER2	0x200
#define CPDMA_RXHDP_VER2	0x220
#define CPDMA_TXCP_VER2		0x240
#define CPDMA_RXCP_VER2		0x260
#endif

#define NUM_ALE_PORTS		6

// #define ALE_ENTRY_BITS		68
// #define ALE_ENTRY_WORDS		DIV_ROUND_UP(ALE_ENTRY_BITS, 32)
#define ALE_ENTRY_SIZE		3

struct ale_regs {
	vu32	version;
	u32	  __pad0;
	vu32	control;
	u32	  __pad1;
	vu32	prescale;	/* 0x10 */
	u32	  __pad2;
	vu32	unk_vlan;
	u32	  __pad3;
	vu32	table_control;	/* 0x20 */
	u32	  __pad4[4];
	/*
	vu32	tbl_w2;
	vu32	tbl_w1;
	vu32	tbl_w0;
	*/
	vu32	table[ALE_ENTRY_SIZE];		/* 0x34 */
	vu32	port_control[NUM_ALE_PORTS];	/* 0x40 */
};

/* Bits in the ALE control register */
#define ALE_CTL_ENABLE		0x80000000
#define ALE_CTL_CLEAR		0x40000000
#define ALE_CTL_VLAN_AWARE	0x4
#define ALE_CTL_BYPASS		0x10

struct stateram_regs {
	volatile unsigned long txhdp[NUM_CPDMA_CHANNELS];
	volatile unsigned long rxhdp[NUM_CPDMA_CHANNELS];
	volatile unsigned long txcp[NUM_CPDMA_CHANNELS];
	volatile unsigned long rxcp[NUM_CPDMA_CHANNELS];
};

struct mdio_regs {
	volatile unsigned long version;		/* 0 */
	volatile unsigned long control;
	volatile unsigned long phy_alive;
	volatile unsigned long phy_link;
	volatile unsigned long link_stat_raw;	/* 0x10 */
	volatile unsigned long link_stat;
	long _pad1[2];
	volatile unsigned long cmd_raw;		/* 0x20 */
	volatile unsigned long cmd;
	volatile unsigned long maskset;
	volatile unsigned long maskclr;
	long _pad2[20];
	volatile unsigned long access0;		/* 0x80 */
	volatile unsigned long physel0;
	/* -- */
	volatile unsigned long access1;
	volatile unsigned long physel1;
};

struct cpsw_regs {
	vu32	id_ver;
	vu32	control;
	vu32	soft_reset;
	vu32	stat_port_en;
	vu32	ptype;
};

/* Port 0 */
struct host_regs {
	vu32	control;
	u32	_pad;
	vu32	max_blks;
	vu32	blk_cnt;
	vu32	flow_thresh;
	vu32	port_vlan;
	vu32	tx_pri_map;

	vu32	cpdma_tx_pri_map;
	vu32	cpdma_rx_chan_map;
};

/* Port 1 and 2 (ethernet) */
struct port_regs {
	u32	control;
	u32	_pad;
	u32	max_blks;
	u32	blk_cnt;
	u32	flow_thresh;
	u32	port_vlan;
	u32	tx_pri_map;

	u32	ts_seq_mtype;
	u32	sa_lo;
	u32	sa_hi;
};

struct sliver_regs {
	vu32	id_ver;
	vu32	mac_control;
	vu32	mac_status;
	vu32	soft_reset;
	vu32	rx_maxlen;
	vu32	__reserved_0;
	vu32	rx_pause;
	vu32	tx_pause;
	vu32	__reserved_1;
	vu32	rx_pri_map;
};


#define ACCESS_GO	0x80000000
#define ACCESS_WRITE	0x40000000
#define ACCESS_ACK	0x20000000
#define ACCESS_DATA	0xFFFF

#define CTL_IDLE	0x80000000
#define CTL_ENABLE	0x40000000

#define MDIO_DIV	255

/* ------------------------------------------------------------------------- */
/* Everything above here is defined by the hardware */
/* ------------------------------------------------------------------------- */

/* Only have one port on the BBB, but ...
 *  the general driver should support two ethernet ports
 *  These get called various things: channels, slaves, ports, ...
 *  I prefer to call them "ports"
 */

struct cpsw_port {
	struct port_regs		*regs;
	struct sliver_regs		*sliver;
	int				port_num;
	u32				mac_control;
};

struct cpsw_priv {
        unsigned char			enetaddr[6];	/* XXX should be per port */

	struct cpdma_desc		*desc_free;

	struct cpdma_chan		rx_chan;
	struct cpdma_chan		tx_chan;

	struct cpsw_port		ports[2];
};

static struct cpsw_priv cpsw_private;

/* --------------------------------------------------- */
/* PHY and mdio stuff follows */
/* --------------------------------------------------- */

#define MDIO_TIMEOUT        100

/* if we had multiple PHY chips, we would have to work
 * out something different here, but this gets us the
 * SMSC chip on the BBB
 */
static int smsc_phy_id = 0;

static void 
mdio_init ( void )
{
	struct mdio_regs *mp = (struct mdio_regs *) MDIO_BASE;

	mp->control = CTL_ENABLE | MDIO_DIV;
	udelay(1000);
}

/* Neither of these spin wait routines does anything clever
 * if they timeout (but at least they don't spin forever)
 */
static int
mdio_access_wait ( void )
{
	int tmo = MDIO_TIMEOUT;
	int rv;
	struct mdio_regs *mp = (struct mdio_regs *) MDIO_BASE;

	while ( tmo-- ) {
	    rv = mp->access0;
	    if ( ! (rv & ACCESS_GO) )
		break;
	    udelay ( 10 );
	}
	return rv;	/* XXX */
}

static void
mdio_idle_wait ( void )
{
	int tmo = MDIO_TIMEOUT;
	struct mdio_regs *mp = (struct mdio_regs *) MDIO_BASE;

	while ( tmo-- ) {
	    if ( mp->control & CTL_IDLE )
		return;
	    udelay ( 10 );
	}
}

static int
mdio_read ( int reg )
{
	struct mdio_regs *mp = (struct mdio_regs *) MDIO_BASE;
	unsigned int val;

	mdio_access_wait ();

	mp->access0 = ACCESS_GO | (reg << 21) | (smsc_phy_id << 16);

	val = mdio_access_wait ();

	/* XXX only valid if ACK bit is set.
	 * We don't check (and what if the access timed out?)
	 */
	return val & ACCESS_DATA;
}

static void
mdio_write ( int reg, int val )
{
	struct mdio_regs *mp = (struct mdio_regs *) MDIO_BASE;

	mdio_access_wait ();
	mp->access0 = 
	    ACCESS_GO | ACCESS_WRITE | (reg << 21) | (smsc_phy_id << 16) | (val & ACCESS_DATA);
	mdio_access_wait ();
}

/* Here are the PHY registers we use */
#define PHY_BMCR	0
#define PHY_BMSR	1
#define PHY_ID1		2
#define PHY_ID2		3
#define PHY_ADVERT	4
#define PHY_PEER	5

#define PHY_SMSC_8710_UID	0x7c0f0
#define PHY_SMSC_8710_MASK	0xffff0

/* Bits in the basic mode control register. */
#define BMCR_RESET		0x8000

/* Bits in the basic mode status register. */
#define BMSR_ANEGCAPABLE	0x0008
#define BMSR_ANEGCOMPLETE	0x0020
#define BMSR_10HALF		0x0800
#define BMSR_10FULL		0x1000
#define BMSR_100HALF		0x2000
#define BMSR_100FULL		0x4000

/* Bits in the link partner ability register. */
#define LPA_10HALF		0x0020
#define LPA_10FULL		0x0040
#define LPA_100HALF		0x0080
#define LPA_100FULL		0x0100

static void
cpsw_phy_verify ( void )
{
	unsigned int id;

	id = mdio_read ( PHY_ID1 ) << 16;
	id |= mdio_read ( PHY_ID2 );

	if ( (id & PHY_SMSC_8710_MASK) == PHY_SMSC_8710_UID )
	    printf ( "Verified smsc 8710 PHY chip\n" );
	else
	    printf ( "Unknown PHY chip, id = %08x\n", id );
}

/* Reset the PHY, this seems to trigger autonegotiation.
 * When we start, BMCR is 0x3100
 * After we trigger reset, it is 0x3000
 * Oddly, it is the duplex mode bit that is resetting here.
 * Also note that there is a bit to restart autonegotiation,
 * which it seems we could use instead of a full reset, but no
 * harm seems to be done by a full reset like this.
 */
static void
cpsw_phy_reset ( void )
{
	int reg;
	int tmo = 500;

	reg = mdio_read ( PHY_BMCR );
	mdio_write ( PHY_BMCR, reg | BMCR_RESET );

	/* Should autoclear in under 0.5 seconds */
	/* (I am seeing 2 milliseconds) */
	while ( tmo-- && (mdio_read(PHY_BMCR) & BMCR_RESET ) )
	    thr_delay ( 1 );

	printf ( "Reset cleared in %d milliseconds\n", (500-tmo) );
	printf ( "Reset status CR: %04x\n", mdio_read ( PHY_BMCR ) );
}

/* we keep track with this */
/* XXX should migrate to "private" or put info directly into mac_control format */
#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01

/* XXX This stuff ought to go into the priv structure */
/* XXX - also we could just set the values that mac_control wants */
int phy_link;
int phy_speed;
int phy_duplex;

/* Give this 5 seconds.
 * I am seeting this complete in about 1.6 seconds.
 *
 * In the case of Kyu, which at present always boots over the network,
 *  we never expect to see the link fail to be present on startup.
 *  (unless somebody is really working fast to pull the cable).
 * But if we get Kyu to boot from flash someday, we may need to
 *  rethink this, even then if the autonegotiation times out, we
 *  can check the link status and set it properly.
 */
static void
cpsw_phy_wait_aneg ( void )
{
	int tmo = 2500;

	printf ( "Aneg wait starts: %04x\n", mdio_read ( PHY_BMSR ) );

	while ( tmo-- ) {
	    if ( mdio_read ( PHY_BMSR ) & BMSR_ANEGCOMPLETE ) {
		printf ( "Aneg done in %d milliseconds\n", (2500-tmo)*2 );
		phy_link = 1;
		return;
	    }
	    thr_delay ( 2 );
	}

	phy_link = 0;
	printf ( "Autonegotiation timeout\n" );
}

/* And together our capabilities and the peer's capabilities */
static void
cpsw_phy_link_status ( void )
{
	int s;

	phy_speed = 10;
	phy_duplex = DUPLEX_HALF;

	s = mdio_read ( PHY_ADVERT );
	s &= mdio_read ( PHY_PEER );

	if ( s & (LPA_100FULL | LPA_100HALF) )
	    phy_speed = 100;

	if ( s & (LPA_100FULL | LPA_10FULL) )
	    phy_duplex = DUPLEX_FULL;
}

/* Can be called at any time - good for testing */
static void
cpsw_phy_aneg ( void )
{
	cpsw_phy_reset ();
	cpsw_phy_wait_aneg ();

	printf ( "Aneg wait finished: %04x\n", mdio_read ( PHY_BMSR ) );
	printf ( "Final status CR: %04x\n", mdio_read ( PHY_BMCR ) );

	if ( phy_link ) {
	    cpsw_phy_link_status ();
	    printf ( "link up (cpsw), speed %d, %s duplex\n",
		phy_speed, (phy_duplex == DUPLEX_FULL) ? "full" : "half" );
	} else
	    printf("link down (cpsw)\n");
}

/* Called at startup */
static void
cpsw_phy_init ( void )
{
	mdio_init ();
	cpsw_phy_verify ();

	cpsw_phy_aneg ();
}

/* ------------------------------------------------*/
/* ------------------------------------------------*/
/* ------------------------------------------------*/
/* Below here is the working driver */
/* ------------------------------------------------*/
/* ------------------------------------------------*/
/* ------------------------------------------------*/

/* XXX - just to provide some debug */
extern struct thread *cur_thread;

#ifdef KYU

static void show_dmastatus ( void );

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* XXX */
#define ENOENT          2       /* No such file or directory */
#define ENOMEM          12      /* Out of memory */
#define EBUSY           16      /* Device or resource busy */
#define ETIMEDOUT       110     /* Connection timed out */

/*
 * Maximum packet size; used to allocate packet storage.
 * TFTP packets can be 524 bytes + IP header + ethernet header.
 *
 * maximum packet size =  1518
 * maximum packet size and multiple of 32 bytes =  1536
 * i.e. 48 * 32 = 1536
 */
#define PKTSIZE                 1518
#define PKTSIZE_ALIGN           1536
#define PKTSIZE_ALIGN2          1536 + 64 * 64	/* KYU */

/* Receive packets.
 *  - usually configured for more than this ...
# define PKTBUFSRX      4
 */

/* There are "only" 8 DMA channels */
#define NUM_RX      32
static char            *NetRxPackets[NUM_RX];
#define PKTBUFSRX      NUM_RX

void NetReceive ( char *, int );

#endif	/* KYU */

#define BIT(x)				(1 << x)
#define BITMASK(bits)		(BIT(bits) - 1)

#define PHY_ID_MASK		0x1f
#define NUM_DESCS		(PKTBUFSRX * 2)
#define PKT_MIN			60
#define PKT_MAX			(1500 + 14 + 4 + 4)

/* bits in the mac_control register */
#define MYSTERY			BIT(5)
#define GIGABITEN		BIT(7)
#define FULLDUPLEXEN		BIT(0)
#define MIIEN			BIT(15)


#define ALE_TABLE_WRITE		BIT(31)

#define ALE_TYPE_FREE			0
#define ALE_TYPE_ADDR			1
#define ALE_TYPE_VLAN			2
#define ALE_TYPE_VLAN_ADDR		3

#define ALE_UCAST_PERSISTANT		0
#define ALE_UCAST_UNTOUCHED		1
#define ALE_UCAST_OUI			2
#define ALE_UCAST_TOUCHED		3

#define ALE_MCAST_FWD			0
#define ALE_MCAST_BLOCK_LEARN_FWD	1
#define ALE_MCAST_FWD_LEARN		2
#define ALE_MCAST_FWD_2			3

static inline int
ale_get_field(u32 *ale_entry, u32 start, u32 bits)
{
	int idx;

	idx    = start / 32;
	start -= idx * 32;
	idx    = 2 - idx; /* flip */
	return (ale_entry[idx] >> start) & BITMASK(bits);
}

static inline void
ale_set_field(u32 *ale_entry, u32 start, u32 bits, u32 value)
{
	int idx;

	value &= BITMASK(bits);
	idx    = start / 32;
	start -= idx * 32;
	idx    = 2 - idx; /* flip */
	ale_entry[idx] &= ~(BITMASK(bits) << start);
	ale_entry[idx] |=  (value << start);
}

#define DEFINE_ALE_FIELD(name, start, bits)				\
static inline int ale_get_##name(u32 *ale_entry)			\
{									\
	return ale_get_field(ale_entry, start, bits);		\
}									\
static inline void ale_set_##name(u32 *ale_entry, u32 value)	\
{									\
	ale_set_field(ale_entry, start, bits, value);		\
}

DEFINE_ALE_FIELD(entry_type,		60,	2)
DEFINE_ALE_FIELD(mcast_state,		62,	2)
DEFINE_ALE_FIELD(port_mask,		66,	3)
DEFINE_ALE_FIELD(ucast_type,		62,	2)
DEFINE_ALE_FIELD(port_num,		66,	2)
DEFINE_ALE_FIELD(blocked,		65,	1)
DEFINE_ALE_FIELD(secure,		64,	1)
DEFINE_ALE_FIELD(mcast,			40,	1)

/* The MAC address field in the ALE entry cannot be macroized as above */
static inline void
ale_get_addr(u32 *ale_entry, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++)
		addr[i] = ale_get_field(ale_entry, 40 - 8*i, 8);
}

static inline void
ale_set_addr(u32 *ale_entry, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++)
		ale_set_field(ale_entry, 40 - 8*i, 8, addr[i]);
}

#ifdef notdef
static inline void
ale_control(struct cpsw_priv *priv, int bit, int val)
{
	u32 tmp, mask = BIT(bit);

	tmp  = __raw_readl(priv->ale_base + ALE_CONTROL);
	tmp &= ~mask;
	tmp |= val ? mask : 0;
	__raw_writel(tmp, priv->ale_base + ALE_CONTROL);
}

/* always with val == 1 */
#define ale_enable(priv, val)		ale_control(priv, 31, val)
/* always with val == 1 */
#define ale_clear(priv, val)		ale_control(priv, 30, val)
/* always with val == 0 */
#define ale_vlan_aware(priv, val)	ale_control(priv,  2, val)
#endif

static void
ale_read ( u32 *buf, int idx )
{
	struct ale_regs *ale = (struct ale_regs *) ALE_BASE;
	int i;

	ale->table_control = idx;
	for ( i = 0; i < ALE_ENTRY_SIZE; i++ )
	    ale->table[i] = buf[i];
}

static void
ale_write ( u32 *buf, int idx )
{
	struct ale_regs *ale = (struct ale_regs *) ALE_BASE;
	int i;

	for ( i = 0; i < ALE_ENTRY_SIZE; i++ )
	    buf[i] = ale->table[i];
	ale->table_control = idx | ALE_TABLE_WRITE;
}

#ifdef notdef
static int
ale_readx(struct cpsw_priv *priv, int idx, u32 *ale_entry)
{
	int i;

	__raw_writel(idx, priv->ale_base + ALE_TABLE_CONTROL);

	for (i = 0; i < ALE_ENTRY_SIZE; i++)
		ale_entry[i] = __raw_readl(priv->ale_base + ALE_TABLE + 4 * i);

	return idx;
}

static int
ale_writex(struct cpsw_priv *priv, int idx, u32 *ale_entry)
{
	int i;

	// printf ("An ALE entry has %d words\n", ALE_ENTRY_SIZE );

	for (i = 0; i < ALE_ENTRY_SIZE; i++)
		__raw_writel(ale_entry[i], priv->ale_base + ALE_TABLE + 4 * i);

	__raw_writel(idx | ALE_TABLE_WRITE, priv->ale_base + ALE_TABLE_CONTROL);

	return idx;
}
#endif

static int
ale_match_addr(struct cpsw_priv *priv, u8* addr)
{
	u32 ale_entry[ALE_ENTRY_SIZE];
	int type, idx;

	for (idx = 0; idx < NUM_ALE_ENTRIES; idx++) {
		u8 entry_addr[6];

		ale_read ( ale_entry, idx );
		type = ale_get_entry_type(ale_entry);
		if (type != ALE_TYPE_ADDR && type != ALE_TYPE_VLAN_ADDR)
			continue;
		ale_get_addr(ale_entry, entry_addr);
		if (memcmp(entry_addr, addr, 6) == 0)
			return idx;
	}
	return -ENOENT;
}

static int
ale_match_free(struct cpsw_priv *priv)
{
	u32 ale_entry[ALE_ENTRY_SIZE];
	int type, idx;

	for (idx = 0; idx < NUM_ALE_ENTRIES; idx++) {
		ale_read ( ale_entry, idx );
		type = ale_get_entry_type(ale_entry);
		if (type == ALE_TYPE_FREE)
			return idx;
	}
	return -ENOENT;
}

static int
ale_find_ageable(struct cpsw_priv *priv)
{
	u32 ale_entry[ALE_ENTRY_SIZE];
	int type, idx;

	for (idx = 0; idx < NUM_ALE_ENTRIES; idx++) {
		ale_read ( ale_entry, idx );
		type = ale_get_entry_type(ale_entry);
		if (type != ALE_TYPE_ADDR && type != ALE_TYPE_VLAN_ADDR)
			continue;
		if (ale_get_mcast(ale_entry))
			continue;
		type = ale_get_ucast_type(ale_entry);
		if (type != ALE_UCAST_PERSISTANT &&
		    type != ALE_UCAST_OUI)
			return idx;
	}
	return -ENOENT;
}

/* ALE unicast entry flags - passed into ale_add_ucast() */
#define ALE_SECURE	1
#define ALE_BLOCKED	2

static int
ale_add_ucast(struct cpsw_priv *priv, u8 *addr, int port, int flags)
{
	u32 ale_entry[ALE_ENTRY_SIZE] = {0, 0, 0};
	int idx;

	ale_set_entry_type(ale_entry, ALE_TYPE_ADDR);
	ale_set_addr(ale_entry, addr);
	ale_set_ucast_type(ale_entry, ALE_UCAST_PERSISTANT);
	ale_set_secure(ale_entry, (flags & ALE_SECURE) ? 1 : 0);
	ale_set_blocked(ale_entry, (flags & ALE_BLOCKED) ? 1 : 0);
	ale_set_port_num(ale_entry, port);

	idx = ale_match_addr(priv, addr);
	if (idx < 0)
		idx = ale_match_free(priv);
	if (idx < 0)
		idx = ale_find_ageable(priv);
	if (idx < 0)
		return -ENOMEM;

	ale_write ( ale_entry, idx );
	return 0;
}

static int
ale_add_mcast(struct cpsw_priv *priv, u8 *addr, int port_mask)
{
	u32 ale_entry[ALE_ENTRY_SIZE] = {0, 0, 0};
	int idx, mask;

	idx = ale_match_addr(priv, addr);
	if (idx >= 0)
		ale_read ( ale_entry, idx );

	ale_set_entry_type(ale_entry, ALE_TYPE_ADDR);
	ale_set_addr(ale_entry, addr);
	ale_set_mcast_state(ale_entry, ALE_MCAST_FWD_2);

	mask = ale_get_port_mask(ale_entry);
	port_mask |= mask;
	ale_set_port_mask(ale_entry, port_mask);

	if (idx < 0)
		idx = ale_match_free(priv);
	if (idx < 0)
		idx = ale_find_ageable(priv);
	if (idx < 0)
		return -ENOMEM;

	ale_write ( ale_entry, idx );
	return 0;
}

/* Ethernet broadcast address */
static unsigned char NetBcastAddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* XXX merge with the above */
static void
ale_broadcast ( int port )
{
	ale_add_mcast ( &cpsw_private, NetBcastAddr, 1 << port);
}

#ifdef notdef
enum ale_port_state {
	ALE_PORT_STATE_DISABLE	= 0x00,
	ALE_PORT_STATE_BLOCK	= 0x01,
	ALE_PORT_STATE_LEARN	= 0x02,
	ALE_PORT_STATE_FORWARD	= 0x03,
};

static inline void
ale_port_state(struct cpsw_priv *priv, int port, int val)
{
	int offset = ALE_PORTCTL + 4 * port;
	u32 tmp, mask = 0x3;

	tmp  = __raw_readl(priv->ale_base + offset);
	tmp &= ~mask;
	tmp |= val & mask;
	__raw_writel(tmp, priv->ale_base + offset);
}
#endif

/* We only ever forward */
#define	ALE_PORT_STATE_DISABLE	0x00,
#define	ALE_PORT_STATE_BLOCK	0x01
#define	ALE_PORT_STATE_LEARN	0x02
#define	ALE_PORT_STATE_FORWARD	0x03

static void
ale_port_forward ( int port )
{
	struct ale_regs *ale = (struct ale_regs *) ALE_BASE;
	unsigned long val;

	// printf ( "ale_port_control = %08x\n", ale->port_control );

	val = ale->port_control[port] & ~0x3;
	ale->port_control[port] = val | ALE_PORT_STATE_FORWARD;
}

/* Three registers have self clearing bits that perform a reset.
 *  (amazingly the very same bit in each).
 * You set the bit, then spin til you see it autoclear.
 */
#define CLEAR_BIT		1

#ifdef notdef
static inline void
setbit_and_wait_for_clear32 ( volatile void *addr )
{
	__raw_writel(CLEAR_BIT, addr);
	while (__raw_readl(addr) & CLEAR_BIT)
		;
}
#endif

static void
reset_port ( struct cpsw_port *port )
{
	struct sliver_regs *sliver = port->sliver;

	// setbit_and_wait_for_clear32 ( &port->sliver->soft_reset );
	sliver->soft_reset = CLEAR_BIT;
	while ( sliver->soft_reset & CLEAR_BIT )
	    ;
}

static void
reset_controller ( void )
{
	struct cpsw_regs *rp = (struct cpsw_regs *) CPSW_BASE;

	// setbit_and_wait_for_clear32 ( &rp->soft_reset );
	rp->soft_reset = CLEAR_BIT;
	while ( rp->soft_reset & CLEAR_BIT )
	    ;
}

static void
reset_cpdma ( void )
{
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;

	// setbit_and_wait_for_clear32 ( &dma->soft_reset );
	dma->soft_reset = CLEAR_BIT;
	while ( dma->soft_reset & CLEAR_BIT )
	    ;
}

#define mac_hi(mac)	(((mac)[0] << 0) | ((mac)[1] << 8) |	\
			 ((mac)[2] << 16) | ((mac)[3] << 24))
#define mac_lo(mac)	(((mac)[4] << 0) | ((mac)[5] << 8))

static void
set_port_mac ( struct cpsw_port *port )
{
	struct port_regs *regp = port->regs;

	/*
	printf ( "Setting MAC address: %08x %08x\n", 
	    mac_hi(priv->enetaddr), mac_lo(priv->enetaddr) );
	*/
	regp->sa_hi = mac_hi ( cpsw_private.enetaddr );
	regp->sa_lo = mac_lo ( cpsw_private.enetaddr );
	// __raw_writel(mac_hi(priv->enetaddr), &port->regs->sa_hi);
	// __raw_writel(mac_lo(priv->enetaddr), &port->regs->sa_lo);
}

static struct cpdma_desc *
cpdma_desc_alloc(struct cpsw_priv *priv)
{
	struct cpdma_desc *desc = priv->desc_free;

	if (desc)
		priv->desc_free = desc->hw_next;

	return desc;
}

static void
cpdma_desc_free(struct cpsw_priv *priv, struct cpdma_desc *desc)
{
	if (desc) {
		desc->hw_next = priv->desc_free;
		priv->desc_free = desc;
	}
}

/* Used for both writes and reads.
 *  for a write, it queues the packet to be sent.
 *  for a read, it adds an empty buffer to the receive list.
 */
static int
cpdma_submit(struct cpsw_priv *priv, struct cpdma_chan *chan, void *buffer, int len)
{
	struct cpdma_desc *desc, *prev;
	u32 mode;

	desc = cpdma_desc_alloc(priv);
	if (!desc) {
	    printf ("cpdma_submit - dropping (no memory)\n" );
	    return -ENOMEM;
	}

	if (len < PKT_MIN)
		len = PKT_MIN;

	mode = CPDMA_DESC_OWNER | CPDMA_DESC_SOP | CPDMA_DESC_EOP;

	desc->hw_next = 0;
	desc->hw_buffer = buffer;
	desc->hw_len = len;
	desc->hw_mode = mode | len;

	desc->sw_buffer = buffer;
	desc->sw_len = len;

	if (!chan->head) {
		/* simple case - first packet enqueued */
		chan->head = desc;
		chan->tail = desc;
		// chan_write(chan, hdp, desc);
		*(chan->hdp) = (u32) desc;
		goto done;
	}

	/* not the first packet - enqueue at the tail */
	prev = chan->tail;
	prev->hw_next = desc;
	chan->tail = desc;

	/* next check if EOQ has been triggered already */
	if ( prev->hw_mode & CPDMA_DESC_EOQ)
		// chan_write(chan, hdp, desc);
		*(chan->hdp) = (u32) desc;

done:
	if (chan->rxfree)
		// chan_write(chan, rxfree, 1);
		// XXX ?! but it works.
		*(chan->rxfree) = 1;
	return 0;
}

static u32 lstatus = 0;

/* This is called for both reads and writes.
 * For writes, it reclaims packets when they have been fully sent.
 * For reads it harvests data that has arrived.
 */
static int
cpdma_process(struct cpsw_priv *priv, struct cpdma_chan *chan, void **buffer, int *len)
{
	struct cpdma_desc *desc;
	u32 status;

	desc = chan->head;
	if (!desc)
		return -ENOENT;

	status = desc->hw_mode;
	/* KYU
	if ( status != lstatus ) {
	    printf ( "cpdma_process %08x from %08x\n", status, desc );
	    lstatus = status;
	}
	*/

	if (len)
		*len = status & 0x7ff;

	if (buffer)
		*buffer = desc->sw_buffer;

	if (status & CPDMA_DESC_OWNER) {
		if ( chan->hdp == 0) {
			if ( desc->hw_mode & CPDMA_DESC_OWNER)
				// chan_write(chan, hdp, desc);
				*(chan->hdp) = (u32) desc;
		}

		return -EBUSY;
	}

	chan->head = desc->hw_next;
	// chan_write(chan, cp, desc);
	*(chan->cp) = (u32) desc;

	cpdma_desc_free(priv, desc);
	return 0;
}

static void
dma_enable ( void )
{
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;

	dma->tx_control = 1;
	dma->rx_control = 1;
}

static void
dma_disable ( void )
{
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;

	dma->tx_control = 0;
	dma->rx_control = 0;
}

static void
port_init(struct cpsw_port *port )
{
	struct sliver_regs *sliver = port->sliver;

	// setbit_and_wait_for_clear32(&port->sliver->soft_reset);
	reset_port ( port );

	/* setup priority mapping */
	port->sliver->rx_pri_map = 0x76543210;
	port->regs->tx_pri_map = 0x33221100;

	/* setup max packet size, and mac address */
	port->sliver->rx_maxlen = PKT_MAX;

	set_port_mac ( port );

	port->mac_control = 0;	/* no link yet */

	ale_port_forward ( port->port_num );
	ale_broadcast ( port->port_num );
}

static void
update_link ( struct cpsw_port *port )
{
	u32 mac_control;

	printf ( "Entering update_link for port %d\n", port->port_num );

	if (phy_link) { /* link up */
		mac_control = MYSTERY;
		if (phy_speed == 1000)
			mac_control |= GIGABITEN;
		if (phy_duplex == DUPLEX_FULL)
			mac_control |= FULLDUPLEXEN;
		if (phy_speed == 100)
			mac_control |= MIIEN;
	} else
		mac_control = 0;

	if (mac_control == port->mac_control)
		return;

	if (mac_control) {
		printf("link up on port %d, speed %d, %s duplex\n",
				port->port_num, phy_speed,
				(phy_duplex == DUPLEX_FULL) ? "full" : "half");
	} else {
		printf("link down on port %d\n", port->port_num);
	}

	port->sliver->mac_control = mac_control;

	port->mac_control = mac_control;
}

/* for ETH structure (init) */
/* Was cpsw_init */
static int
cpsw_setup( void )
{
	struct cpsw_priv	*priv = &cpsw_private;
	// struct cpsw_port	*port;
	struct host_regs *hreg = (struct host_regs *) PORT0_BASE;
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;
	struct stateram_regs *stram = (struct stateram_regs *) STATERAM_BASE;
	struct cpsw_regs *regs = (struct cpsw_regs *) CPSW_BASE;
	struct ale_regs *ale = (struct ale_regs *) ALE_BASE;
	struct cpdma_desc *bdram;
	int i, ret;

	reset_controller ();

	// printf ( "CPDMA rxfree = %08x\n", dma->rxfree );

	// initially all zeros
	// printf ( "ALE control = %08x\n", ale->control );
	ale->control |= ALE_CTL_ENABLE | ALE_CTL_CLEAR;
	ale->control &= ~ALE_CTL_VLAN_AWARE;

	/* initialize and reset the address lookup engine */
	// ale_enable(priv, 1);
	// ale_clear(priv, 1);
	// ale_vlan_aware(priv, 0); /* vlan unaware mode */

	/* setup host port priority mapping */
	hreg->cpdma_tx_pri_map = 0x76543210;
	hreg->cpdma_rx_chan_map = 0;

	/* disable priority elevation and enable statistics on all ports */
	regs->ptype = 0;

	/* enable statistics collection only on the host port */
	regs->stat_port_en = BIT(HOST_PORT);
	regs->stat_port_en = 0x7;

	// ale_port_state(priv, HOST_PORT, ALE_PORT_STATE_FORWARD);
	ale_port_forward ( HOST_PORT );

	ale_add_ucast(priv, priv->enetaddr, HOST_PORT, ALE_SECURE);

	// ale_add_mcast(priv, NetBcastAddr, 1 << HOST_PORT);
	ale_broadcast ( HOST_PORT );

	/* BBB only has one port */
	port_init ( &priv->ports[0] );
	update_link ( &priv->ports[0] );

	// priv->descs		= (struct cpdma_desc *) BDRAM_BASE;
	//	desc_write(&priv->descs[i], hw_next,
	//	   (i == (NUM_DESCS - 1)) ? 0 : &priv->descs[i+1]);
	/* init descriptor pool */

	bdram = (struct cpdma_desc *) BDRAM_BASE;
	for (i = 0; i < NUM_DESCS; i++) {
		bdram[i].hw_next = (i == (NUM_DESCS - 1)) ? 0 : &bdram[i+1];
		//desc_write(&bdram[i], hw_next,
			   //(i == (NUM_DESCS - 1)) ? 0 : &bdram[i+1]);
	}

	// priv->desc_free = &priv->descs[0];
	priv->desc_free = bdram;

	/* initialize channels */
	/* am335x devices are version 2 */
	memset(&priv->rx_chan, 0, sizeof(struct cpdma_chan));
	//priv->rx_chan.hdp       = priv->dma_regs + CPDMA_RXHDP_VER2;
	//priv->rx_chan.cp        = priv->dma_regs + CPDMA_RXCP_VER2;
	// priv->rx_chan.rxfree    = priv->dma_regs + CPDMA_RXFREE;

	priv->rx_chan.hdp = stram->rxhdp;
	priv->rx_chan.cp = stram->rxcp;
	priv->rx_chan.rxfree = dma->rxfree;

	memset(&priv->tx_chan, 0, sizeof(struct cpdma_chan));
	// priv->tx_chan.hdp       = priv->dma_regs + CPDMA_TXHDP_VER2;
	// priv->tx_chan.cp        = priv->dma_regs + CPDMA_TXCP_VER2;
	priv->tx_chan.hdp = stram->txhdp;
	priv->tx_chan.cp = stram->txcp;

	/* clear dma state */
	reset_cpdma ();

	// for (i = 0; i < priv->data.channels; i++) {
	for (i = 0; i < NUM_CPDMA_CHANNELS; i++) {
		dma->rxfree[i] = 0;
		stram->rxhdp[i] = 0;
		stram->rxcp[i] = 0;
		stram->txhdp[i] = 0;
		stram->txcp[i] = 0;
	}

	/* enable both rx and tx */
	dma_enable ();

	/*
	printf ( "Writing to dma regs at %08x\n", priv->dma_regs );
	printf ( "Writing to dma txcon at %08x\n", priv->dma_regs + CPDMA_TXCONTROL );

	show_dmastatus ();
	*/

	/* submit rx descs */
	for (i = 0; i < PKTBUFSRX; i++) {
		ret = cpdma_submit(priv, &priv->rx_chan, NetRxPackets[i],
				   PKTSIZE);
		if (ret < 0) {
			printf("error %d submitting rx desc\n", ret);
			break;
		}
	}

	return 0;
}

/* used to exist for ETH structure - never called at present */
static void
// cpsw_halt(struct eth_device *dev)
cpsw_halt( void )
{
	dma_disable ();
	reset_controller ();
	reset_cpdma ();
}

#define CPDMA_TIMEOUT		100 /* msecs */

/* was for ETH structure - never called at present */
static int
// cpsw_sendpk(struct eth_device *dev, void *packet, int length)
cpsw_sendpk( void *packet, int length )
{
	struct cpsw_priv	*priv = &cpsw_private;
	void *buffer;
	int len;
	int timeout = CPDMA_TIMEOUT;

	flush_dcache_range((unsigned long)packet,
			   (unsigned long)packet + length);

	/* first reap completed packets */
	while (timeout-- && (cpdma_process(priv, &priv->tx_chan, &buffer, &len) >= 0))
		;

	if (timeout == -1) {
		printf("cpdma_process timeout\n");
		return -ETIMEDOUT;
	}

	return cpdma_submit(priv, &priv->tx_chan, packet, length);
}

/* was for ETH structure - never called at present */
static int
// cpsw_recv(struct eth_device *dev)
cpsw_recv( void )
{
	struct cpsw_priv	*priv = &cpsw_private;
	void *buffer;
	int len;

	while (cpdma_process(priv, &priv->rx_chan, &buffer, &len) >= 0) {
		invalidate_dcache_range((unsigned long)buffer,
					(unsigned long)buffer + PKTSIZE_ALIGN);

		/* XXX - in U-boot this gets called for every packet received */
		NetReceive(buffer, len);

		cpdma_submit(priv, &priv->rx_chan, buffer, PKTSIZE);
	}

	return 0;
}

void
cpsw_register ( void )
{
	struct cpsw_priv	*priv = &cpsw_private;
	struct cpsw_port	*port;

	port = &priv->ports[0];
	port->port_num = ETH_PORT;
	port->regs	  = (struct port_regs *) PORT1_BASE;
	port->sliver = (struct sliver_regs *) SLIVER1_BASE;

	port = &priv->ports[1];
	port->port_num = ETH_PORT_EXTRA;
	port->regs	  = (struct port_regs *) PORT2_BASE;
	port->sliver = (struct sliver_regs *) SLIVER2_BASE;

	cm_get_mac0 ( priv->enetaddr );
}

#ifdef KYU_EEPROM
/*
 * TI AM335x parts define a system EEPROM that defines certain sub-fields.
 * We use these fields to in turn see what board we are on, and what
 * that might require us to set or not set.
 */
#define HDR_NO_OF_MAC_ADDR	3
#define HDR_ETH_ALEN		6
#define HDR_NAME_LEN		8

struct am335x_baseboard_id {
	unsigned int  magic;
	char name[HDR_NAME_LEN];
	char version[4];
	char serial[12];
	char config[32];
	char mac_addr[HDR_NO_OF_MAC_ADDR][HDR_ETH_ALEN];
};

static inline int
board_is_bone(struct am335x_baseboard_id *header)
{
	return !strncmp(header->name, "A335BONE", HDR_NAME_LEN);
}
#endif

/*
 * eth_init (previously board_eth_init())
 *
 * This is what we are using (for now) to kick off this driver.
 *
 * This function will:
 * Read the eFuse for MAC addresses, and set ethaddr/eth1addr/usbnet_devaddr
 * in the environment
 * Perform fixups to the PHY present on certain boards.  We only need this
 * function in:
 * - SPL with either CPSW or USB ethernet support
 * - Full U-Boot, with either CPSW or USB ethernet
 * Build in only these cases to avoid warnings about unused variables
 * when we build an SPL that has neither option but full U-Boot will.
 */
void
eth_init ( void )
{
#ifdef KYU_MAC
	/* not used */
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("ethaddr")) {
		printf("<ethaddr> not set. Validating first E-fuse MAC\n");

		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
	}

	mac_lo = readl(&cdev->macid1l);
	mac_hi = readl(&cdev->macid1h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("eth1addr")) {
		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("eth1addr", mac_addr);
	}
#endif

#ifdef KYU_EEPROM
	{
	struct am335x_baseboard_id header;

	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	if ( ! board_is_bone(&header) )
	    puts("Bogus Juju in eeprom?!\n" );
	}
#endif

	/* Kyu only supports the BBB */
	cm_mii_enable ();

	cpsw_register();
}

/* Called by cpsw_activate() to actually start packet reception */
static void
eth_start ( void )
{
	int i;
	unsigned long pkaddr;

	/* These packets are used by cpsw_setup () */
	/* Tricky alignment on 64 byte multiples */
	for ( i=0; i < NUM_RX; i++ ) {
	    pkaddr = (unsigned long ) malloc ( PKTSIZE_ALIGN2 );
	    if ( ! pkaddr )
		printf ("No memory for cpsw receive buffer\n" );
	    NetRxPackets[i] = (char *) ((pkaddr+63) & ~0x3f);
	}

	cpsw_setup ();

#ifdef notdef
	printf ("Waiting for packets\n" );

	/* Loop receiving packets */
	/* This is effectively polling, it returns all
	 * the time without a packet.  If you get lucky
	 * it will trigger a call to NetReceive()
	 */
	for ( ;; ) {
	    cpsw_recv ();
	}
#endif
}

static unsigned long
get_dmastat ( void )
{
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;

	return dma->status;
}

static void
show_dmastatus ( void )
{
	struct dma_regs *dma = (struct dma_regs *) CPDMA_BASE;

	printf ( "DMA base at %08x\n", dma );
	printf ( "DMA tx control: %08x\n", dma->tx_control );
	printf ( "DMA rx control: %08x\n", dma->rx_control );
	printf ( "DMA status:   %08x\n", dma->status );

}

#define TLIMIT 10
#define RLIMIT 30 * 20

void
eth_test ( int arg )
{
	unsigned long buf[128];
	int i;
	unsigned long dmastat;

	eth_init ();

	for ( i=0; i<128; i++ )
	    buf[i] = 0xdeadbeef;

	printf ( "Begin test on cpsw device\n" );

	eth_start ();

	/*
	show_dmastatus ();
	dma_enable ();
	show_dmastatus ();
	*/

	for ( i=0; i<TLIMIT ; i++ ) {
	    cpsw_recv ();
	    dmastat = get_dmastat ();
	    printf ( "Sending %d (dma status: %08x\n", i, dmastat );
	    cpsw_sendpk ( buf, 128*sizeof(unsigned long) );
	    thr_delay ( 100 );
	}

	for ( i=0; i<RLIMIT ; i++ ) {
	    cpsw_recv ();
	    thr_delay ( 50 );
	}

	/*
	show_dmastatus ();

	xwrapper ();
	*/

	printf ( "ALL DONE\n" );
}

static int pk_count = 0;

void 
NetReceive ( char *buffer, int len)
{
	printf ( "Packet received %d (%d bytes)\n", pk_count, len );
	dump_b ( buffer, 1 );
	pk_count++;
}

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* The current set of official Kyu entry points */

#include "netbuf.h"
#include "net.h"

static int rx_count = 0;
static int tx_count = 0;
static int tx_int_count = 0;

/* As a funky hack til we start using interrupts.
 * we launch this thread to harvest packets.
 * Mostly copied from cpsw_recv() above.
 */
static void
cpsw_reaper ( int xxx )
{
	struct cpsw_priv *priv = &cpsw_private;
	struct cpdma_chan *rx_chan = &priv->rx_chan;
	void *buffer;
	int len;
	struct netbuf *nbp;

	for ( ;; ) {
	    if ( cpdma_process ( priv, rx_chan, &buffer, &len ) >= 0 ) {
		invalidate_dcache_range((unsigned long)buffer,
					(unsigned long)buffer + PKTSIZE_ALIGN);

		++rx_count;

		/* KYU */
		/* get a netbuf and copy the packet into it */

		/* XXX only at interrupt level */
		/*
		nbp = netbuf_alloc_i ();
		*/
		nbp = netbuf_alloc ();

		if ( ! nbp ) {
		    thr_delay ( 1 );
		    continue;
		}

		/* 5-21-2015 - there is a trailing 4 bytes that is being treasured
		 * in the buffer that we don't want or need
		 */
		nbp->elen = len - 4;
		memcpy ( (char *) nbp->eptr, buffer, len - 4 );

		net_rcv ( nbp );

		/* recycle receive buffer */
		cpdma_submit ( priv, rx_chan, buffer, PKTSIZE );
	    } else {
		thr_delay ( 1 );
	    }
	}
}

/* I ain't never seen none of these */
void
cpsw_tx_isr ( int dummy )
{
	printf ( "Interrupt (tx)\n" );
	tx_int_count++;
}

/* Kyu entry point. */
/* Called first to initialize the device */
/* Then call cpsw_activate() */
int
cpsw_init ( void )
{
	printf ( "Starting cpsw_init\n" );
	cpsw_phy_init ();

	eth_init ();

	irq_hookup ( NINT_CPSW_TX, cpsw_tx_isr, 0 );
	printf ( "Finished cpsw_init\n" );
	return 1;
}

/* Needs to be above 30 while the
 * test thread runs at 30 with polling
 */
#define PRI_REAPER	25

void
cpsw_activate ( void )
{
	printf ( "Starting cpsw_activate\n" );
	eth_start ();

	(void) thr_new ( "cpsw_reaper", cpsw_reaper, (void *) 0, PRI_REAPER, 0 );
	printf ( "Finished cpsw_activate\n" );
}

void
get_cpsw_addr ( char *buf )
{
	memcpy ( buf, cpsw_private.enetaddr, ETH_ADDR_SIZE );
}

void
cpsw_show ( void )
{
	printf ( "Receive count: %d\n", rx_count );
	printf ( "Transmit count: %d\n", tx_count );
	printf ( "Transmit INT count: %d\n", tx_int_count );
}

void
cpsw_send ( struct netbuf *nbp )
{
	int len;

	/* Put our ethernet address in the packet */
	memcpy ( nbp->eptr->src, cpsw_private.enetaddr, ETH_ADDR_SIZE );

	len = nbp->ilen + sizeof(struct eth_hdr);

	/* Not needed if cpsw autopads */
	if ( len < ETH_MIN_SIZE )
	    len = ETH_MIN_SIZE;

	++tx_count;

	cpsw_sendpk ( nbp->eptr, len );
}

/* THE END */
