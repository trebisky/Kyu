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

#ifndef KYU
#include <common.h>
#include <command.h>
#include <net.h>
#include <miiphy.h>
#include <malloc.h>
#include <net.h>
#include <netdev.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <phy.h>
#include <asm/arch/cpu.h>
#endif

/* Kyu project
 * Tom Trebisky 5-26-2015
 * Derived from the U-boot (2015.01) sources (from cpsw.c)
 *  many other files merged in to make it stand alone
 *  then the whole mess reworked heavily.
 *
 * The cpsw is a 3 port ethernet switch capable of gigabit
 *  two ports are ethernet, one is CPPI
 *  on the BBB only one port is used and has
 *   only a 10/100 media interface.
 * The media interface is a SMSC "LAN8710" chip.
 */

/* This next section is code I have added for Kyu, with every intention
 * of working up my own BBB specific driver for this chip.
 * As of 6-23-2016 this stuff up front is just "parked" here and
 * pretty much independent of the rest and untested.
 */
#include <kyu.h>
#include <kyulib.h>
#include <thread.h>
#include <malloc.h>
#include <omap_ints.h>

/* Hardware base addresses */
/* from the TRM, chapter 2, pages 175-176 */

#define CPSW_BASE	0x4a100000	/* Ethernet Switch Subsystem */
#define CPSW_PORT	0x4a100100	/* Ethernet Port control */
#define CPSW_CPDMA	0x4a100800	/* Ethernet DMA */
#define CPSW_STATS	0x4a100900	/* Ethernet Statistics */
#define CPSW_STATERAM	0x4a100A00	/* Ethernet State RAM */
#define CPSW_CPTS	0x4a100C00	/* Ethernet Time sync */
#define CPSW_ALE	0x4a100D00	/* Ethernet Address Lookup Engine */
#define CPSW_SL1	0x4a100D80	/* Ethernet "sliver" for Port 1 */
#define CPSW_SL2	0x4a100DC0	/* Ethernet "sliver" for Port 2 */
#define CPSW_MDIO	0x4a101000	/* Ethernet MDIO */
#define CPSW_WR		0x4a101200	/* Ethernet RMII/RGMII wrapper */

/* KYU */
#define CPSW_DMA_BASE                   0x4A100800

struct cpsw_slave_data {
	u32		slave_reg_ofs;
	u32		sliver_reg_ofs;
};

struct cpsw_platform_data {
	u32	cpsw_base;
	int	mdio_div;
	int	channels;	/* number of cpdma channels (symmetric)	*/
	u32	cpdma_reg_ofs;	/* cpdma register offset		*/
	int	slaves;		/* number of slave cpgmac ports		*/
	u32	ale_reg_ofs;	/* address lookup engine reg offset	*/
	int	ale_entries;	/* ale table size			*/
	u32	host_port_reg_ofs;	/* cpdma host port registers	*/
	u32	hw_stats_reg_ofs;	/* cpsw hw stats counters	*/
	u32	bd_ram_ofs;		/* Buffer Descriptor RAM offset */
	u32	mac_control;
	struct cpsw_slave_data	*slave_data;
	void	(*control)(int enabled);
	u32	host_port_num;
	u32	active_slave;
	u8	version;
};

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
	},
};

enum {
	CPSW_CTRL_VERSION_1 = 0,
	CPSW_CTRL_VERSION_2	/* am33xx like devices */
};

struct cpdma_desc {
	/* hardware fields */
	u32			hw_next;
	u32			hw_buffer;
	u32			hw_len;
	u32			hw_mode;
	/* software fields */
	u32			sw_buffer;
	u32			sw_len;
};

struct cpdma_chan {
	struct cpdma_desc	*head, *tail;
	void			*hdp, *cp, *rxfree;
};

static struct cpsw_platform_data cpsw_data = {
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};


struct cpsw_priv {
	struct cpsw_platform_data	data;
	int				host_port;
        unsigned char			enetaddr[6];

	struct cpsw_regs		*regs;
	void				*dma_regs;
	struct cpsw_host_regs		*host_port_regs;
	void				*ale_regs;

	struct cpdma_desc		*descs;
	struct cpdma_desc		*desc_free;
	struct cpdma_chan		rx_chan, tx_chan;

	struct cpsw_slave		*slaves;
};

static struct cpsw_priv cpsw_private;

/* This would be list header, but simply points to our
 * one instance of this structure.
 */
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

#define ACCESS_GO	0x80000000
#define ACCESS_WRITE	0x40000000
#define ACCESS_ACK	0x20000000
#define ACCESS_DATA	0xFFFF
// #define USERACCESS_READ		(0)

#define CTL_IDLE	0x80000000
#define CTL_ENABLE	0x40000000

#define MDIO_MAX        100 /* timeout */

/* Values obtained by print statements in working code */
static int mdio_div = 255;
static int xx_phy_id = 0;

static void 
mdio_init ( void )
{
	struct mdio_regs *mp = (struct mdio_regs *) CPSW_MDIO;

	mp->control = CTL_ENABLE | mdio_div;
	udelay(1000);
}

/* Neither of these spin wait routines does anything clever
 * if they timeout (but at least they don't spin forever)
 */
static int
mdio_access_wait ( void )
{
	int tmo = MDIO_MAX;
	int rv;
	struct mdio_regs *mp = (struct mdio_regs *) CPSW_MDIO;

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
	int tmo = MDIO_MAX;
	struct mdio_regs *mp = (struct mdio_regs *) CPSW_MDIO;

	while ( tmo-- ) {
	    if ( mp->control & CTL_IDLE )
		return;
	    udelay ( 10 );
	}
}

static int
mdio_read ( int reg )
{
	struct mdio_regs *mp = (struct mdio_regs *) CPSW_MDIO;
	unsigned int val;

	mdio_access_wait ();

	mp->access0 = ACCESS_GO | (reg << 21) | (xx_phy_id << 16);

	val = mdio_access_wait ();

	/* XXX only valid if ACK bit is set.
	 * We don't check (and what if the access timed out?)
	 */
	return val & ACCESS_DATA;
}

static void
mdio_write ( int reg, int val )
{
	struct mdio_regs *mp = (struct mdio_regs *) CPSW_MDIO;

	mdio_access_wait ();
	mp->access0 = 
	    ACCESS_GO | ACCESS_WRITE | (reg << 21) | (xx_phy_id << 16) | (val & ACCESS_DATA);
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

void
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
void
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
#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01

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
void
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
void
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
void
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
void
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

/* From: ./arch/arm/include/asm/arch-am33xx/cpu.h: */
#define BIT(x)				(1 << x)

/* XXX */
#define ENOENT          2       /* No such file or directory */
#define ENOMEM          12      /* Out of memory */
#define EBUSY           16      /* Device or resource busy */
#define EINVAL          22      /* Invalid argument */
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

/* There are only 8 DMA channels */
#define NUM_RX      32
static char            *NetRxPackets[NUM_RX];
#define PKTBUFSRX      NUM_RX

/* Ethernet broadcast address */
/* extern unsigned char            NetBcastAddr[6]; */
static unsigned char		NetBcastAddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

void NetReceive ( char *, int );

#endif	/* KYU */

#define BITMASK(bits)		(BIT(bits) - 1)
#define PHY_REG_MASK		0x1f
#define PHY_ID_MASK		0x1f
#define NUM_DESCS		(PKTBUFSRX * 2)
#define PKT_MIN			60
#define PKT_MAX			(1500 + 14 + 4 + 4)

#define CLEAR_BIT		1
#define GIGABITEN		BIT(7)
#define FULLDUPLEXEN		BIT(0)
#define MIIEN			BIT(15)

/* Offsets to access CPDMA registers --
 * this driver doesn't use many DMA Registers
 */
#define CPDMA_TXCONTROL		0x004
#define CPDMA_RXCONTROL		0x014
#define CPDMA_SOFTRESET		0x01c
#define CPDMA_STATUS		0x024	/* KYU */
#define CPDMA_RXFREE		0x0e0
#define CPDMA_TXHDP_VER1	0x100
#define CPDMA_TXHDP_VER2	0x200
#define CPDMA_RXHDP_VER1	0x120
#define CPDMA_RXHDP_VER2	0x220
#define CPDMA_TXCP_VER1		0x140
#define CPDMA_TXCP_VER2		0x240
#define CPDMA_RXCP_VER1		0x160
#define CPDMA_RXCP_VER2		0x260

/* Descriptor mode bits */
#define CPDMA_DESC_SOP		BIT(31)
#define CPDMA_DESC_EOP		BIT(30)
#define CPDMA_DESC_OWNER	BIT(29)
#define CPDMA_DESC_EOQ		BIT(28)

/*
 * This timeout definition is a worst-case ultra defensive measure against
 * unexpected controller lock ups.  Ideally, we should never ever hit this
 * scenario in practice.
 */
#define MDIO_TIMEOUT            100 /* msecs */
#define CPDMA_TIMEOUT		100 /* msecs */

#ifdef KYU_OLD
struct cpsw_mdio_regs {
	u32	version;
	u32	control;

#define CONTROL_IDLE		BIT(31)
#define CONTROL_ENABLE		BIT(30)

	u32	alive;
	u32	link;
	u32	linkintraw;
	u32	linkintmasked;
	u32	__reserved_0[2];
	u32	userintraw;
	u32	userintmasked;
	u32	userintmaskset;
	u32	userintmaskclr;
	u32	__reserved_1[20];

	/* KYU - this weird construct is because there
	 * are two registers, repeated here, once for port 0
	 * and again for port 1.  saying user[2] would be clearer.
	 */
	struct {
		u32		access;
		u32		physel;
#define USERACCESS_GO		BIT(31)
#define USERACCESS_WRITE	BIT(30)
#define USERACCESS_ACK		BIT(29)
#define USERACCESS_READ		(0)
#define USERACCESS_DATA		(0xffff)
	} user[0];
};
#endif

struct cpsw_regs {
	vu32	id_ver;
	vu32	control;
	vu32	soft_reset;
	vu32	stat_port_en;
	vu32	ptype;
};

struct cpsw_slave_regs {
	u32	max_blks;
	u32	blk_cnt;
	u32	flow_thresh;
	u32	port_vlan;
	u32	tx_pri_map;
#ifdef CONFIG_AM33XX
	u32	gap_thresh;
#elif defined(CONFIG_TI814X)
	u32	ts_ctl;
	u32	ts_seq_ltype;
	u32	ts_vlan;
#endif
	u32	sa_lo;
	u32	sa_hi;
};

struct cpsw_host_regs {
	u32	max_blks;
	u32	blk_cnt;
	u32	flow_thresh;
	u32	port_vlan;
	u32	tx_pri_map;
	u32	cpdma_tx_pri_map;
	u32	cpdma_rx_chan_map;
};

struct cpsw_sliver_regs {
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

#define ALE_ENTRY_BITS		68
#define ALE_ENTRY_WORDS		DIV_ROUND_UP(ALE_ENTRY_BITS, 32)

/* ALE Registers */
#define ALE_CONTROL		0x08
#define ALE_UNKNOWNVLAN		0x18
#define ALE_TABLE_CONTROL	0x20
#define ALE_TABLE		0x34
#define ALE_PORTCTL		0x40

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

enum cpsw_ale_port_state {
	ALE_PORT_STATE_DISABLE	= 0x00,
	ALE_PORT_STATE_BLOCK	= 0x01,
	ALE_PORT_STATE_LEARN	= 0x02,
	ALE_PORT_STATE_FORWARD	= 0x03,
};

/* ALE unicast entry flags - passed into cpsw_ale_add_ucast() */
#define ALE_SECURE	1
#define ALE_BLOCKED	2

struct cpsw_slave {
	struct cpsw_slave_regs		*regs;
	struct cpsw_sliver_regs		*sliver;
	int				slave_num;
	u32				mac_control;
	struct cpsw_slave_data		*data;
};

#ifdef KYU
#define __raw_writel(v,a)	(*((volatile unsigned long *)(a)) = (v))
#define __raw_readl(a)		(*((volatile unsigned long *)(a)))
#endif

#define desc_write(desc, fld, val)	__raw_writel((u32)(val), &(desc)->fld)
#define desc_read(desc, fld)		__raw_readl(&(desc)->fld)
#define desc_read_ptr(desc, fld)	((void *)__raw_readl(&(desc)->fld))

#define chan_write(chan, fld, val)	__raw_writel((u32)(val), (chan)->fld)
#define chan_read(chan, fld)		__raw_readl((chan)->fld)
#define chan_read_ptr(chan, fld)	((void *)__raw_readl((chan)->fld))

#define for_active_slave(slave, priv) \
	slave = (priv)->slaves + (priv)->data.active_slave; if (slave)

#define for_each_slave(slave, priv) \
	for (slave = (priv)->slaves; slave != (priv)->slaves + \
				(priv)->data.slaves; slave++)

static inline int cpsw_ale_get_field(u32 *ale_entry, u32 start, u32 bits)
{
	int idx;

	idx    = start / 32;
	start -= idx * 32;
	idx    = 2 - idx; /* flip */
	return (ale_entry[idx] >> start) & BITMASK(bits);
}

static inline void cpsw_ale_set_field(u32 *ale_entry, u32 start, u32 bits,
				      u32 value)
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
static inline int cpsw_ale_get_##name(u32 *ale_entry)			\
{									\
	return cpsw_ale_get_field(ale_entry, start, bits);		\
}									\
static inline void cpsw_ale_set_##name(u32 *ale_entry, u32 value)	\
{									\
	cpsw_ale_set_field(ale_entry, start, bits, value);		\
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
static inline void cpsw_ale_get_addr(u32 *ale_entry, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++)
		addr[i] = cpsw_ale_get_field(ale_entry, 40 - 8*i, 8);
}

static inline void cpsw_ale_set_addr(u32 *ale_entry, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++)
		cpsw_ale_set_field(ale_entry, 40 - 8*i, 8, addr[i]);
}

static int cpsw_ale_read(struct cpsw_priv *priv, int idx, u32 *ale_entry)
{
	int i;

	__raw_writel(idx, priv->ale_regs + ALE_TABLE_CONTROL);

	for (i = 0; i < ALE_ENTRY_WORDS; i++)
		ale_entry[i] = __raw_readl(priv->ale_regs + ALE_TABLE + 4 * i);

	return idx;
}

static int cpsw_ale_write(struct cpsw_priv *priv, int idx, u32 *ale_entry)
{
	int i;

	for (i = 0; i < ALE_ENTRY_WORDS; i++)
		__raw_writel(ale_entry[i], priv->ale_regs + ALE_TABLE + 4 * i);

	__raw_writel(idx | ALE_TABLE_WRITE, priv->ale_regs + ALE_TABLE_CONTROL);

	return idx;
}

static int cpsw_ale_match_addr(struct cpsw_priv *priv, u8* addr)
{
	u32 ale_entry[ALE_ENTRY_WORDS];
	int type, idx;

	for (idx = 0; idx < priv->data.ale_entries; idx++) {
		u8 entry_addr[6];

		cpsw_ale_read(priv, idx, ale_entry);
		type = cpsw_ale_get_entry_type(ale_entry);
		if (type != ALE_TYPE_ADDR && type != ALE_TYPE_VLAN_ADDR)
			continue;
		cpsw_ale_get_addr(ale_entry, entry_addr);
		if (memcmp(entry_addr, addr, 6) == 0)
			return idx;
	}
	return -ENOENT;
}

static int cpsw_ale_match_free(struct cpsw_priv *priv)
{
	u32 ale_entry[ALE_ENTRY_WORDS];
	int type, idx;

	for (idx = 0; idx < priv->data.ale_entries; idx++) {
		cpsw_ale_read(priv, idx, ale_entry);
		type = cpsw_ale_get_entry_type(ale_entry);
		if (type == ALE_TYPE_FREE)
			return idx;
	}
	return -ENOENT;
}

static int cpsw_ale_find_ageable(struct cpsw_priv *priv)
{
	u32 ale_entry[ALE_ENTRY_WORDS];
	int type, idx;

	for (idx = 0; idx < priv->data.ale_entries; idx++) {
		cpsw_ale_read(priv, idx, ale_entry);
		type = cpsw_ale_get_entry_type(ale_entry);
		if (type != ALE_TYPE_ADDR && type != ALE_TYPE_VLAN_ADDR)
			continue;
		if (cpsw_ale_get_mcast(ale_entry))
			continue;
		type = cpsw_ale_get_ucast_type(ale_entry);
		if (type != ALE_UCAST_PERSISTANT &&
		    type != ALE_UCAST_OUI)
			return idx;
	}
	return -ENOENT;
}

static int
cpsw_ale_add_ucast(struct cpsw_priv *priv, u8 *addr,
			      int port, int flags)
{
	u32 ale_entry[ALE_ENTRY_WORDS] = {0, 0, 0};
	int idx;

	cpsw_ale_set_entry_type(ale_entry, ALE_TYPE_ADDR);
	cpsw_ale_set_addr(ale_entry, addr);
	cpsw_ale_set_ucast_type(ale_entry, ALE_UCAST_PERSISTANT);
	cpsw_ale_set_secure(ale_entry, (flags & ALE_SECURE) ? 1 : 0);
	cpsw_ale_set_blocked(ale_entry, (flags & ALE_BLOCKED) ? 1 : 0);
	cpsw_ale_set_port_num(ale_entry, port);

	idx = cpsw_ale_match_addr(priv, addr);
	if (idx < 0)
		idx = cpsw_ale_match_free(priv);
	if (idx < 0)
		idx = cpsw_ale_find_ageable(priv);
	if (idx < 0)
		return -ENOMEM;

	cpsw_ale_write(priv, idx, ale_entry);
	return 0;
}

static int
cpsw_ale_add_mcast(struct cpsw_priv *priv, u8 *addr, int port_mask)
{
	u32 ale_entry[ALE_ENTRY_WORDS] = {0, 0, 0};
	int idx, mask;

	idx = cpsw_ale_match_addr(priv, addr);
	if (idx >= 0)
		cpsw_ale_read(priv, idx, ale_entry);

	cpsw_ale_set_entry_type(ale_entry, ALE_TYPE_ADDR);
	cpsw_ale_set_addr(ale_entry, addr);
	cpsw_ale_set_mcast_state(ale_entry, ALE_MCAST_FWD_2);

	mask = cpsw_ale_get_port_mask(ale_entry);
	port_mask |= mask;
	cpsw_ale_set_port_mask(ale_entry, port_mask);

	if (idx < 0)
		idx = cpsw_ale_match_free(priv);
	if (idx < 0)
		idx = cpsw_ale_find_ageable(priv);
	if (idx < 0)
		return -ENOMEM;

	cpsw_ale_write(priv, idx, ale_entry);
	return 0;
}

static inline void
cpsw_ale_control(struct cpsw_priv *priv, int bit, int val)
{
	u32 tmp, mask = BIT(bit);

	tmp  = __raw_readl(priv->ale_regs + ALE_CONTROL);
	tmp &= ~mask;
	tmp |= val ? mask : 0;
	__raw_writel(tmp, priv->ale_regs + ALE_CONTROL);
}

#define cpsw_ale_enable(priv, val)	cpsw_ale_control(priv, 31, val)
#define cpsw_ale_clear(priv, val)	cpsw_ale_control(priv, 30, val)
#define cpsw_ale_vlan_aware(priv, val)	cpsw_ale_control(priv,  2, val)

static inline void cpsw_ale_port_state(struct cpsw_priv *priv, int port,
				       int val)
{
	int offset = ALE_PORTCTL + 4 * port;
	u32 tmp, mask = 0x3;

	tmp  = __raw_readl(priv->ale_regs + offset);
	tmp &= ~mask;
	tmp |= val & mask;
	__raw_writel(tmp, priv->ale_regs + offset);
}

#ifdef KYU_SPIN
/* The following function was compiling
 *   in some evil way until I added the printf
 *   statements, then it began to work.
 * Apparently this was somehow related to a faulty
 *   definition for the __raw_writel() macro I wrote.
 * I see no reason to have this inline as it gets
 *   called only a few times during initialization
 *   or shutdown of the device.
 * It seems OK in the original form now.
 */
static void
setbit_and_wait_for_clear32(volatile u32 *addr)
{
	int xx;

	__raw_writel(CLEAR_BIT, addr);

	xx = __raw_readl(addr);
	printf ( "read %08x --> %08x\n", addr, xx );
	udelay(100);
	xx = __raw_readl(addr);
	printf ( "read %08x --> %08x\n", addr, xx );

	while (__raw_readl(addr) & CLEAR_BIT)
		;
}
#else
/* Set a self-clearing bit in a register, and wait for it to clear */
static inline void
setbit_and_wait_for_clear32(volatile void *addr)
{
	__raw_writel(CLEAR_BIT, addr);
	while (__raw_readl(addr) & CLEAR_BIT)
		;
}
#endif

#define mac_hi(mac)	(((mac)[0] << 0) | ((mac)[1] << 8) |	\
			 ((mac)[2] << 16) | ((mac)[3] << 24))
#define mac_lo(mac)	(((mac)[4] << 0) | ((mac)[5] << 8))

static void
cpsw_set_slave_mac(struct cpsw_slave *slave, struct cpsw_priv *priv)
{
	/*
	printf ( "Setting MAC address: %08x %08x\n", 
	    mac_hi(priv->enetaddr), mac_lo(priv->enetaddr) );
	*/

	__raw_writel(mac_hi(priv->enetaddr), &slave->regs->sa_hi);
	__raw_writel(mac_lo(priv->enetaddr), &slave->regs->sa_lo);
}

static void
cpsw_slave_update_link(struct cpsw_slave *slave, struct cpsw_priv *priv )
{
	u32 mac_control;

	printf ( "Entering cpsw_slave_update_link for slave %d\n", slave->slave_num );

	if (phy_link) { /* link up */
		mac_control = priv->data.mac_control;
		if (phy_speed == 1000)
			mac_control |= GIGABITEN;
		if (phy_duplex == DUPLEX_FULL)
			mac_control |= FULLDUPLEXEN;
		if (phy_speed == 100)
			mac_control |= MIIEN;
	} else
		mac_control = 0;

	if (mac_control == slave->mac_control)
		return;

	if (mac_control) {
		printf("link up on port %d, speed %d, %s duplex\n",
				slave->slave_num, phy_speed,
				(phy_duplex == DUPLEX_FULL) ? "full" : "half");
	} else {
		printf("link down on port %d\n", slave->slave_num);
	}

	__raw_writel(mac_control, &slave->sliver->mac_control);

	slave->mac_control = mac_control;
}

static void
cpsw_update_link(struct cpsw_priv *priv)
{
	struct cpsw_slave *slave;

	/* Only calls one slave for BBB */
	for_active_slave(slave, priv)
		cpsw_slave_update_link(slave, priv);
}

static inline u32
cpsw_get_slave_port(struct cpsw_priv *priv, u32 slave_num)
{
	if (priv->host_port == 0)
		return slave_num + 1;
	else
		return slave_num;
}

static void
cpsw_slave_init(struct cpsw_slave *slave, struct cpsw_priv *priv)
{
	u32     slave_port;

	/*
	printf ("Resetting slave %d\n", slave->slave_num );
	printf ( "Using:  %08x\n", &slave->sliver->soft_reset );
	*/
	setbit_and_wait_for_clear32(&slave->sliver->soft_reset);
	/*
	printf ("Reset done\n");
	*/

	/* setup priority mapping */
	__raw_writel(0x76543210, &slave->sliver->rx_pri_map);
	__raw_writel(0x33221100, &slave->regs->tx_pri_map);

	/* setup max packet size, and mac address */
	__raw_writel(PKT_MAX, &slave->sliver->rx_maxlen);
	cpsw_set_slave_mac(slave, priv);

	slave->mac_control = 0;	/* no link yet */

	/* enable forwarding */
	slave_port = cpsw_get_slave_port(priv, slave->slave_num);
	cpsw_ale_port_state(priv, slave_port, ALE_PORT_STATE_FORWARD);

	cpsw_ale_add_mcast(priv, NetBcastAddr, 1 << slave_port);
}

static struct
cpdma_desc *
cpdma_desc_alloc(struct cpsw_priv *priv)
{
	struct cpdma_desc *desc = priv->desc_free;

	if (desc)
		priv->desc_free = desc_read_ptr(desc, hw_next);

	return desc;
}

static void
cpdma_desc_free(struct cpsw_priv *priv, struct cpdma_desc *desc)
{
	if (desc) {
		desc_write(desc, hw_next, priv->desc_free);
		priv->desc_free = desc;
	}
}

/* Used for both writes and reads.
 *  for a write, it queues the packet to be sent.
 *  for a read, it adds an empty buffer to the receive list.
 */
static int
cpdma_submit(struct cpsw_priv *priv, struct cpdma_chan *chan,
			void *buffer, int len)
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

	desc_write(desc, hw_next,   0);
	desc_write(desc, hw_buffer, buffer);
	desc_write(desc, hw_len,    len);
	desc_write(desc, hw_mode,   mode | len);
	desc_write(desc, sw_buffer, buffer);
	desc_write(desc, sw_len,    len);

	if (!chan->head) {
		/* simple case - first packet enqueued */
		chan->head = desc;
		chan->tail = desc;
		chan_write(chan, hdp, desc);
		goto done;
	}

	/* not the first packet - enqueue at the tail */
	prev = chan->tail;
	desc_write(prev, hw_next, desc);
	chan->tail = desc;

	/* next check if EOQ has been triggered already */
	if (desc_read(prev, hw_mode) & CPDMA_DESC_EOQ)
		chan_write(chan, hdp, desc);

done:
	if (chan->rxfree)
		chan_write(chan, rxfree, 1);
	return 0;
}

static u32 lstatus = 0;

/* This is called for both reads and writes.
 * For writes, it reclaims packets when they have been fully sent.
 * For reads it harvests data that has arrived.
 */
static int
cpdma_process(struct cpsw_priv *priv, struct cpdma_chan *chan,
			 void **buffer, int *len)
{
	struct cpdma_desc *desc = chan->head;
	u32 status;

	if (!desc)
		return -ENOENT;

	status = desc_read(desc, hw_mode);
	/* KYU
	if ( status != lstatus ) {
	    printf ( "cpdma_process %08x from %08x\n", status, desc );
	    lstatus = status;
	}
	*/

	if (len)
		*len = status & 0x7ff;

	if (buffer)
		*buffer = desc_read_ptr(desc, sw_buffer);

	if (status & CPDMA_DESC_OWNER) {
		if (chan_read(chan, hdp) == 0) {
			if (desc_read(desc, hw_mode) & CPDMA_DESC_OWNER)
				chan_write(chan, hdp, desc);
		}

		return -EBUSY;
	}

	chan->head = desc_read_ptr(desc, hw_next);
	chan_write(chan, cp, desc);

	cpdma_desc_free(priv, desc);
	return 0;
}

/* for ETH structure (init) */
/* Was cpsw_init */
static int
// cpsw_setup(struct eth_device *dev)
cpsw_setup( void )
{
	struct cpsw_priv	*priv = &cpsw_private;
	struct cpsw_slave	*slave;
	int i, ret;

	// printf ( "Resetting controller using %08x\n", &priv->regs->soft_reset );

	/* soft reset the controller and initialize priv */
	setbit_and_wait_for_clear32(&priv->regs->soft_reset);

	// printf ( "Reset done\n" );

	/* initialize and reset the address lookup engine */
	cpsw_ale_enable(priv, 1);
	cpsw_ale_clear(priv, 1);
	cpsw_ale_vlan_aware(priv, 0); /* vlan unaware mode */

	/* setup host port priority mapping */
	__raw_writel(0x76543210, &priv->host_port_regs->cpdma_tx_pri_map);
	__raw_writel(0, &priv->host_port_regs->cpdma_rx_chan_map);

	/* disable priority elevation and enable statistics on all ports */
	__raw_writel(0, &priv->regs->ptype);

	/* enable statistics collection only on the host port */
	__raw_writel(BIT(priv->host_port), &priv->regs->stat_port_en);
	__raw_writel(0x7, &priv->regs->stat_port_en);

	cpsw_ale_port_state(priv, priv->host_port, ALE_PORT_STATE_FORWARD);

	cpsw_ale_add_ucast(priv, priv->enetaddr, priv->host_port,
			   ALE_SECURE);

	cpsw_ale_add_mcast(priv, NetBcastAddr, 1 << priv->host_port);

	for_active_slave(slave, priv)
		cpsw_slave_init(slave, priv);

	cpsw_update_link(priv);

	/* init descriptor pool */
	for (i = 0; i < NUM_DESCS; i++) {
		desc_write(&priv->descs[i], hw_next,
			   (i == (NUM_DESCS - 1)) ? 0 : &priv->descs[i+1]);
	}
	priv->desc_free = &priv->descs[0];

	/* initialize channels */
	/* am335x devices are version 2 */
	if (priv->data.version == CPSW_CTRL_VERSION_2) {
		memset(&priv->rx_chan, 0, sizeof(struct cpdma_chan));
		priv->rx_chan.hdp       = priv->dma_regs + CPDMA_RXHDP_VER2;
		priv->rx_chan.cp        = priv->dma_regs + CPDMA_RXCP_VER2;
		priv->rx_chan.rxfree    = priv->dma_regs + CPDMA_RXFREE;

		memset(&priv->tx_chan, 0, sizeof(struct cpdma_chan));
		priv->tx_chan.hdp       = priv->dma_regs + CPDMA_TXHDP_VER2;
		priv->tx_chan.cp        = priv->dma_regs + CPDMA_TXCP_VER2;
	} else {
		memset(&priv->rx_chan, 0, sizeof(struct cpdma_chan));
		priv->rx_chan.hdp       = priv->dma_regs + CPDMA_RXHDP_VER1;
		priv->rx_chan.cp        = priv->dma_regs + CPDMA_RXCP_VER1;
		priv->rx_chan.rxfree    = priv->dma_regs + CPDMA_RXFREE;

		memset(&priv->tx_chan, 0, sizeof(struct cpdma_chan));
		priv->tx_chan.hdp       = priv->dma_regs + CPDMA_TXHDP_VER1;
		priv->tx_chan.cp        = priv->dma_regs + CPDMA_TXCP_VER1;
	}

	/* clear dma state */
	setbit_and_wait_for_clear32(priv->dma_regs + CPDMA_SOFTRESET);

	if (priv->data.version == CPSW_CTRL_VERSION_2) {
		for (i = 0; i < priv->data.channels; i++) {
			__raw_writel(0, priv->dma_regs + CPDMA_RXHDP_VER2 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_RXFREE + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_RXCP_VER2 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_TXHDP_VER2 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_TXCP_VER2 + 4 * i);
		}
	} else {
		for (i = 0; i < priv->data.channels; i++) {
			__raw_writel(0, priv->dma_regs + CPDMA_RXHDP_VER1 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_RXFREE + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_RXCP_VER1 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_TXHDP_VER1 + 4 * i);
			__raw_writel(0, priv->dma_regs + CPDMA_TXCP_VER1 + 4 * i);
		}
	}

	/* enable both rx and tx */
	__raw_writel(1, priv->dma_regs + CPDMA_TXCONTROL);
	__raw_writel(1, priv->dma_regs + CPDMA_RXCONTROL);

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

	/*
	printf ( "cpsw_setup done\n" );
	*/
	return 0;
}

/* was for ETH structure - never called at present */
static void
// cpsw_halt(struct eth_device *dev)
cpsw_halt( void )
{
	// struct cpsw_priv	*priv = dev->priv;
	struct cpsw_priv	*priv = &cpsw_private;

	__raw_writel(0, priv->dma_regs + CPDMA_TXCONTROL);
	__raw_writel(0, priv->dma_regs + CPDMA_RXCONTROL);

	/* soft reset the controller and initialize priv */
	setbit_and_wait_for_clear32(&priv->regs->soft_reset);

	/* clear dma state */
	setbit_and_wait_for_clear32(priv->dma_regs + CPDMA_SOFTRESET);

	priv->data.control(0);
}

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

static void
cpsw_slave_setup(struct cpsw_slave *slave, int slave_num, struct cpsw_priv *priv)
{
	void			*regs = priv->regs;
	// struct cpsw_slave_data	*data = priv->data.slave_data + slave_num;
	struct cpsw_slave_data	*data = &priv->data.slave_data[slave_num];

	slave->slave_num = slave_num;
	slave->data	= data;
	slave->regs	= regs + data->slave_reg_ofs;
	slave->sliver	= regs + data->sliver_reg_ofs;
}

int
cpsw_register(struct cpsw_platform_data *data)
{
	struct cpsw_priv	*priv;
	struct cpsw_slave	*slave;
	void			*regs = (void *)data->cpsw_base;

	/*
	dev = calloc(sizeof(*dev), 1);
	if (!dev)
		return -ENOMEM;

	priv = calloc(sizeof(*priv), 1);
	if (!priv) {
		free(dev);
		return -ENOMEM;
	}
	*/
	priv = &cpsw_private;

	priv->data = *data;

	priv->slaves = malloc(sizeof(struct cpsw_slave) * data->slaves);
	if (!priv->slaves) {
		return -ENOMEM;
	}

	priv->host_port		= data->host_port_num;
	priv->regs		= regs;
	priv->host_port_regs	= regs + data->host_port_reg_ofs;
	priv->dma_regs		= regs + data->cpdma_reg_ofs;
	priv->ale_regs		= regs + data->ale_reg_ofs;
	priv->descs		= (void *)regs + data->bd_ram_ofs;

	int idx = 0;

	for_each_slave(slave, priv) {
		/*
		printf ( "cpsw slave setup %d\n", idx );
		*/
		cpsw_slave_setup(slave, idx, priv);
		idx = idx + 1;
	}

	cm_get_mac0 ( priv->enetaddr );

	/* One instance of a eth_device structure */
	// eth_device = dev;

	return 1;
}

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
int
eth_init ( void )
{
	int rv, n = 0;

#ifdef KYU_MAC
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

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;

#ifndef KYU
/* Not for BBB */
	/*
	 *
	 * CPSW RGMII Internal Delay Mode is not supported in all PVT
	 * operating points.  So we must set the TX clock delay feature
	 * in the AR8051 PHY.  Since we only support a single ethernet
	 * device in U-Boot, we only do this for the first instance.
	 */
#define AR8051_PHY_DEBUG_ADDR_REG	0x1d
#define AR8051_PHY_DEBUG_DATA_REG	0x1e
#define AR8051_DEBUG_RGMII_CLK_DLY_REG	0x5
#define AR8051_RGMII_TX_CLK_DLY		0x100

	if (board_is_evm_sk(&header) || board_is_gp_evm(&header)) {
		const char *devname;
		devname = miiphy_get_current_dev();

		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_ADDR_REG,
				AR8051_DEBUG_RGMII_CLK_DLY_REG);
		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_DATA_REG,
				AR8051_RGMII_TX_CLK_DLY);
	}
#endif

	return n;
}

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

	/*
	printf ("Calling cpsw_setup\n" );
	cpsw_setup ( eth_device );
	printf ("Done with cpsw_setup\n" );
	*/
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
	struct cpsw_priv	*priv = &cpsw_private;
	// unsigned long *lp = (unsigned long *) (priv->dma_regs + CPDMA_STATUS);
	/*
	printf ("Get dma status from %08x (%08x)\n", lp, *lp );
	*/
	//return *lp;
	return * (unsigned long *) (priv->dma_regs + CPDMA_STATUS);
}

struct cpsw_dma {
	volatile unsigned long tx_ver;
	volatile unsigned long tx_control;
	volatile unsigned long tx_teardown;
	long	_gap0;
	volatile unsigned long rx_ver;
	volatile unsigned long rx_control;
	volatile unsigned long rx_teardown;

	volatile unsigned long soft_reset;
	volatile unsigned long control;
	volatile unsigned long status;
};

static void
show_dmastatus ( void )
{
	struct cpsw_dma *dp = (struct cpsw_dma *) CPSW_DMA_BASE;

	printf ( "DMA base at %08x\n", dp );
	printf ( "DMA tx control: %08x\n", dp->tx_control );
	printf ( "DMA rx control: %08x\n", dp->rx_control );
	printf ( "DMA status:   %08x\n", dp->status );

}

static void
dma_enable ( void )
{
	struct cpsw_dma *dp = (struct cpsw_dma *) CPSW_DMA_BASE;

	dp->tx_control = 1;
	dp->rx_control = 1;
}

#define TLIMIT 10
#define RLIMIT 30 * 20

void
eth_test ( int arg )
{
	unsigned long buf[128];
	int i;
	unsigned long dmastat;

	(void) eth_init ();

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
