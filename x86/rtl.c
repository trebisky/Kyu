/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* rtl.c
 *
 * Driver for the Realtek 8139 network chip.
 * another network driver for skidoo.
 * Originally stolen from etherboot-5.1.4/rtl8139.c
 * but then made more like linux-2.6.10/8139too.c
 * This chip is for most practical purposes undocumented.
 * At least so it would seem.  The etherboot and linux
 * driver were my main sources of information, along
 * with what can be learned by monkeying with the chip.
 *
 * Tom Trebisky  tom@mmto.org  Skidoo project
 *	3-30-2005 began coding, steal framework from ee100.c
 *	4-12-2005 receiving packets, and sending too.
 *
 * I began working with a chip on an LTSP motherboard (TK-3350)
 * This chip is labeled a 8139C and gives PCI rev 0x10
 * Chip revision code 0x74 (yep, an 8139C)
 * MAC address: 00:50:8B:0D:B0:EB
 * This board has a weird issue where the interrupt on IRQ-15
 * Just makes skidoo give a diverr panic.
 *
 * Then I bought an 8139D (rev 0x10) on a trendnet PCI board
 * this gives a Chip revision code 0x74,
 * which is really an 8139C (hmmmm?!)
 * MAC address: 00:40:F4:C1:82:C5
 * This works just fine on IRQ-15 with skidoo.
 *
 * Chips with a PCI revision of 0x20 or greater are the snazzier
 * 8139C+ chips and are said to be "tulip like" and under linux
 * use the rtl8139cp.c driver.
 * I haven't seen one of those yet.
 */

#include "skidoo.h"
#include "sklib.h"
#include "intel.h"
#include "pci.h"
#include "net.h"
#include "netbuf.h"

/*
#define DEBUG_RX
#define DEBUG_TX
*/

#define USE_IO_BASE

#ifdef VXWORKS
#include "vxWorks.h"
#include "iv.h"
#endif

#ifdef USE_IO_BASE
#define ior_8(x)	inb((int)(x))
#define ior_16(x)	inw((int)(x))
#define ior_32(x)	inl((int)(x))
#define iow_8(b,x)	outb(b,(int)(x))
#define iow_16(b,x)	outw(b,(int)(x))
#define iow_32(b,x)	outl(b,(int)(x))
#else
#define ior_8(x)	(*(volatile unsigned char __force *) (x))
#define ior_16(x)	(*(volatile unsigned short __force *) (x))
#define ior_32(x)	(*(volatile unsigned long __force *) (x))
#define iow_8(b,x)	*(volatile unsigned char __force *) (x) = b
#define iow_16(b,x)	*(volatile unsigned short __force *) (x) = b
#define iow_32(b,x)	*(volatile unsigned long __force *) (x) = b
#endif

typedef void (*pcifptr) ( struct pci_dev * );

#define RTL_VENDOR	0x10ec
#define RTL_DEVICE	0x8139

#define EEPROM_SIZE	4

struct rtl_info {
	void *memaddr;
	void *ioaddr;
	void *iobase;
	int count;
	int revision;
	int irq;
	int cur_rx;
	int cur_tx;
	int tx_slots;
	unsigned short eeprom[EEPROM_SIZE];
	unsigned char eaddr[ETH_ADDR_SIZE];
	int active;
};

static struct rtl_info rtl_soft;

/* The RTL8139 allows only 3 possible rx ring sizes:
 * 0, 1, 2 is allowed - 8,16,32K rx buffer
 */
#define RX_BUF_LEN_IDX	2
#define RX_BUF_SIZE	(8192 << RX_BUF_LEN_IDX)
static char rx_ring[RX_BUF_SIZE];

/* Does not include frame checksum which is
 * 4 more bytes added by the chip.
 */
#define TX_BUF_SIZE	ETH_MAX_SIZE
#define NUM_TX_DESC     4       /* Number of Tx descriptor registers. */

static char tx_ring[TX_BUF_SIZE * 4];

/* Prototypes */
static void rtl_setup ( void );
static void rtl_eeprom ( void );
static void rtl_isr ( void * );
static void rtl_reset ( void );
static int read_eeprom (void *, int, int );

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH 256      /* In bytes, rounded down to 32 byte units. */
#define RX_FIFO_THRESH  4       /* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST    4       /* Maximum PCI burst, '4' is 256 bytes */
#define TX_DMA_BURST    4       /* Calculate as 16<<val. */

/* Symbolic offsets to registers. */
enum RTL_registers {
	MAC0 = 0,			/* Ethernet hardware address. */
	MAC4 = 4,
	MAR0 = 8,			/* Multicast filter. */
	MAR4 = 12,

	TxStatus0 = 0x10,		/* Transmit status (four 32bit registers). */
	TxStatus1 = 0x14,		/* Transmit status (four 32bit registers). */
	TxStatus2 = 0x18,		/* Transmit status (four 32bit registers). */
	TxStatus3 = 0x1C,		/* Transmit status (four 32bit registers). */

	TxAddr0 = 0x20,			/* Tx descriptors (also four 32bit). */
	TxAddr1 = 0x24,			/* Tx descriptors (also four 32bit). */
	TxAddr2 = 0x28,			/* Tx descriptors (also four 32bit). */
	TxAddr3 = 0x2C,			/* Tx descriptors (also four 32bit). */

	RxBuf = 0x30,
	RxEarlyCnt = 0x34,
	RxEarlyStatus = 0x36,
	ChipCmd = 0x37,
	RxBufPtr = 0x38,
	RxBufAddr = 0x3A,
	IntrMask = 0x3C,
	IntrStatus = 0x3E,
	TxConfig = 0x40,
	RxConfig = 0x44,
	Timer = 0x48,			/* general-purpose counter. */
	RxMissed = 0x4C,		/* 24 bits valid, write clears. */
	CFG_9346 = 0x50,
	Config0 = 0x51,
	Config1 = 0x52,
	TimerIntrReg = 0x54,		/* intr if gp counter reaches this value */
	MediaStatus = 0x58,
	Config3 = 0x59,
	Config4 = 0x5a,			/* not present on 8139A */
	MultiIntr = 0x5C,
	RevisionID = 0x5E,		/* same as PCI revision code */
	TxSummary = 0x60,
	MII_BMCR = 0x62,
	MII_BMSR = 0x64,
	NWayAdvert = 0x66,
	NWayLPAR = 0x68,
	NWayExpansion = 0x6A,
	DisconnectCnt = 0x6C,
	FalseCarrierCnt = 0x6E,
	/* undocumented, but required beyond here */
	NWayTestReg = 0x70,		/* FIFO control and test */
	RxCnt = 0x72,			/* packet received counter */
	CSCR = 0x74,			/* status and configuration */
	PhyParm1 = 0x78,
	TwisterParm = 0x7c,		/* magic xcvr parameter */
	PhyParm2 = 0x80,		/* undocumented */
	Config5 = 0xD8			/* absent on 8139A */

	/* from 0x84 onwards are a number of power management/wakeup frame
	 * definitions we will probably never need to know about.  */
};

/* We can read this register as well as write to it
 */
enum ChipCmdBits {
	CmdReset=0x10, CmdRxEnb=0x08, CmdTxEnb=0x04, RxBufEmpty=0x01, };

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
	PCIErr=0x8000, PCSTimeout=0x4000, CableLenChange= 0x2000,
	RxFIFOOver=0x40, RxUnderrun=0x20, RxOverflow=0x10,
	TxErr=0x08, TxOK=0x04, RxErr=0x02, RxOK=0x01,
};
enum TxStatusBits {
	TxHostOwns=0x2000, TxUnderrun=0x4000, TxStatOK=0x8000,
	TxOutOfWindow=0x20000000, TxAborted=0x40000000,
	TxCarrierLost=0x80000000,
};
enum RxStatusBits {
	RxMulticast=0x8000, RxPhysical=0x4000, RxBroadcast=0x2000,
	RxBadSymbol=0x0020, RxRunt=0x0010, RxTooLong=0x0008, RxCRCErr=0x0004,
	RxBadAlign=0x0002, RxStatusOK=0x0001,
};

enum MediaStatusBits {
	MSRTxFlowEnable=0x80, MSRRxFlowEnable=0x40, MSRSpeed10=0x08,
	MSRLinkFail=0x04, MSRRxPauseFlag=0x02, MSRTxPauseFlag=0x01,
};

enum MIIBMCRBits {
	BMCRReset=0x8000, BMCRSpeed100=0x2000, BMCRNWayEnable=0x1000,
	BMCRRestartNWay=0x0200, BMCRDuplex=0x0100,
};

enum CSCRBits {
	CSCR_LinkOKBit=0x0400, CSCR_LinkChangeBit=0x0800,
	CSCR_LinkStatusBits=0x0f000, CSCR_LinkDownOffCmd=0x003c0,
	CSCR_LinkDownCmd=0x0f3c0,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	RxCfgWrap=0x80,
	AcceptErr=0x20, AcceptRunt=0x10, AcceptBroadcast=0x08,
	AcceptMulticast=0x04, AcceptMyPhys=0x02, AcceptAllPhys=0x01,
};

/*
 * Some notes on chip revision codes:
 * (from the linux 8139too.c driver)
 *  0x40 - 8139
 *  0x60 - 8139 rev K
 *  0x70 - 8139A
 *  0x72 - 8139A rev G
 *  0x78 - 8139B
 *  0x7c - 8130
 *  0x74 - 8139C
 *  0x7a - 8100
 *  0x75 - 8100B / 8139D
 *  0x77 - 8101
 * Versions 0x72 and below have a Halt Clock to do low power mode.
 * Versions 0x78 and above have Wake on Lan.
 */

void
get_rtl_addr ( unsigned char *buf )
{
    	memcpy ( buf, rtl_soft.eaddr, ETH_ADDR_SIZE );
}

/*
#define IRQ_HACK
#define VIA_HACK
*/

void
rtl_probe ( struct pci_dev *pci_info )
{
#ifdef VIA_HACK
    if ( pci_info->vendor == 0x1106 && pci_info->device == 0x0686 ) {
	printf ( "Dork with VIA 82C686/A ioapic\n" );
	pci_dev_write_char ( pci_info, 0x58, 0x1f );
	return;
    }
#endif

    if ( pci_info->vendor != RTL_VENDOR )
	return;
    if ( pci_info->device != RTL_DEVICE )
	return;

    if ( rtl_soft.count ) {
	rtl_soft.count++;
	return;
    }

#ifdef IRQ_HACK
    /* hack to override BIOS and put this on IRQ 5 */
    pci_set_irq ( pci_info, 5 );
#endif

    rtl_soft.count = 1;
    rtl_soft.ioaddr = (void *) ((unsigned long) pci_info->base[0] & ~3);
    printf ( "IO: %08x\n", rtl_soft.ioaddr );
    rtl_soft.memaddr = pci_info->base[1];
    printf ( "MEM: %08x\n", rtl_soft.memaddr );
#ifdef USE_IO_BASE
    rtl_soft.iobase = rtl_soft.ioaddr;
#else
    rtl_soft.iobase = rtl_soft.memaddr;
#endif
    printf ( "BASE: %08x\n", rtl_soft.iobase );
    rtl_soft.irq = pci_info->irq;
    rtl_soft.revision = pci_info->rev;
} 

static void
rtl_locate ( void )
{
    rtl_soft.count = 0;
    pci_find_all ( 0, rtl_probe );
    if ( rtl_soft.count < 1 )
	return;

    printf ( "IO: %08x\n", rtl_soft.ioaddr );
    printf ( "rtl8139 (rev %02x) at %08x (IRQ %d)\n",
	    rtl_soft.revision, rtl_soft.iobase, rtl_soft.irq );
}

void
rtl_activate ( void )
{
    rtl_soft.active = 1;
}

int
rtl_init ( void )
{
    rtl_locate ();
    if ( rtl_soft.count < 1 ) {
	printf ( "Cannot find a RealTek 8139 anywhere\n" );
	return 0;
    }

#ifdef VXWORKS
/* Bypass the first 32 vectors.
 * (used by processor hardware)
 */
#define IRQ_BASE_VEC	0x20

    {
	int num = IRQ_BASE_VEC + rtl_soft.irq;

	/*
	sysIntDisablePIC ( num )
	*/
	intConnect ( INUM_TO_IVEC(num), rtl_isr, 0 );
	sysIntEnablePIC ( num );
    }
#endif

#ifndef VXWORKS
    cpu_enter ();
    irq_hookup ( rtl_soft.irq, rtl_isr, &rtl_soft );
    /*
    diverr_hookup ( rtl_isr );
    */
    rtl_setup ();
    cpu_leave ();
#endif

    return 1;
}

static void
rtl_setup ( void )
{
	int speed10;
	int fullduplex;
	int rev1, rev2;
	void *io = rtl_soft.iobase;

	/* Bring the chip out of low-power mode. */
	iow_8 ( 0x00, io + Config1);

	rtl_eeprom ();

	rev1 = (ior_32 ( io + TxConfig ) >> 24) & 0x7f;
	rev2 = ior_8 ( io + RevisionID );	/* same is PCI rev */
	printf ( "RTL8139 revision code: %02x  %02x\n", rev1, rev2 );

	speed10 = ior_8 (io + MediaStatus) & MSRSpeed10;
	fullduplex = ior_16 (io + MII_BMCR) & BMCRDuplex;

	printf("%sMbps %s-duplex\n", 
	    speed10 ? "10" : "100",
	    fullduplex ? "full" : "half");

	rtl_reset();

	printf ( "Rx buffer at: %08x\n", rx_ring );

	if ( ior_8(io + MediaStatus) & MSRLinkFail )
	    printf("Cable not connected or other link failure\n");
}

static void
rtl_reset ( void )
{
	void * io = rtl_soft.iobase;
	long timeout;
	int i;
	int stat;

	iow_8 ( CmdReset, io + ChipCmd);

	stat = ior_8 ( io + ChipCmd );

	timeout = 1000;
	while ( timeout-- && (ior_8 (io + ChipCmd) & CmdReset) ) {
	    udelay ( 10 );
	}

	stat = ior_8 ( io + ChipCmd );

	rtl_soft.cur_rx = 0;
	rtl_soft.cur_tx = 0;
	rtl_soft.tx_slots = 1;

	iow_32 ( * (long *) rtl_soft.eaddr, io + MAC0 );
	iow_32 ( * (long *) &rtl_soft.eaddr[4], io + MAC4 );

#ifdef never
	/* writing the 6th byte hangs the PCI bus on some systems
	 * when writing the mac address byte by byte.
	 * Doing the write as 2 longs (above) works much better.
	 * BUT .. I have even seen this hang doing long accesses,
	 * maybe it is some PCI write caching thing (yuk!)
	 * with memory mapped io registers (the linux driver has
	 * mention of this as a known rtl8139 bug).
	 */
	for (i = 0; i < ETH_ADDR_SIZE; i++)
	    iow_8 ( rtl_soft.eaddr[i], io + MAC0 + i);
#endif

	/* Must enable Tx/Rx before setting transfer thresholds! */
	iow_8 (CmdRxEnb | CmdTxEnb, io + ChipCmd);
	iow_32 ((RX_FIFO_THRESH<<13) | (RX_BUF_LEN_IDX<<11) | (RX_DMA_BURST<<8),
		io + RxConfig);		/* accept no frames yet!  */

	iow_32 ( (TX_DMA_BURST<<8)|0x03000000, io + TxConfig);

	iow_32 ( (unsigned long) rx_ring, io + RxBuf);

	/* Start the chip's Tx and Rx process. */
	iow_32 (0, io + RxMissed);

	/* set_rx_mode */
	iow_8 (AcceptBroadcast|AcceptMyPhys, io + RxConfig);

	/* If we add multicast support, the MAR0 register would have to be
	 * initialized to 0xffffffffffffffff (two 32 bit accesses).
	 */
	iow_8 (CmdRxEnb | CmdTxEnb, io + ChipCmd);

/*
#define RTL_INT_ALL (PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK)
*/
#define RTL_INT_ALL (PCIErr | PCSTimeout | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK)

	/* Allow interrupts */
	iow_16 ( RTL_INT_ALL, io + IntrMask );
}

void
rtl_nope ( void )
{
	/* Disable all known interrupts */
	iow_16 ( 0x0000, rtl_soft.iobase + IntrMask );
}

void
rtl_go ( void )
{
	iow_16 ( RTL_INT_ALL, rtl_soft.iobase + IntrMask );
}

static void
rtl_eeprom ( void )
{
    int i;
    int eealen, val;

    val = read_eeprom ( rtl_soft.iobase, 0, 8 );
    rtl_soft.eeprom[0] = val;

    /* bits in the eeprom address */
    eealen = val == 0x8129 ? 8 : 6;

    for ( i=0; i<3; i++ )
	rtl_soft.eeprom[i+1] = read_eeprom ( rtl_soft.iobase, i + 7, eealen );

    printf ( "ether addr = " );
    for ( i=0; i<ETH_ADDR_SIZE; i++ ) {
        val = rtl_soft.eeprom[1+i/2];
        if ( i & 1 )
            val >>= 8;
        rtl_soft.eaddr[i] = val & 0xff;
        printf ( "%02x", rtl_soft.eaddr[i] );
        if ( i < ETH_ADDR_SIZE-1 )
            printf ( ":" );
    }
    printf ( "\n" );
}


/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK		0x04	/* EEPROM shift clock. */
#define EE_CS			0x08	/* EEPROM chip select. */
#define EE_DATA_WRITE		0x02	/* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x02
#define EE_DATA_READ		0x01	/* EEPROM chip data out. */
#define EE_ENB			(0x80 | EE_CS)

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
 */

#define eeprom_delay()	ior_32(rtlee_addr)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD		(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD		(7)

static int
read_eeprom (void *ioaddr, int location, int addr_len)
{
	int i;
	unsigned retval = 0;
	void *rtlee_addr = ioaddr + CFG_9346;
	int read_cmd = location | (EE_READ_CMD << addr_len);

	iow_8 (EE_ENB & ~EE_CS, rtlee_addr);
	iow_8 (EE_ENB, rtlee_addr);
	eeprom_delay ();

	/* Shift the read command bits out. */
	for (i = 4 + addr_len; i >= 0; i--) {
		int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		iow_8 (EE_ENB | dataval, rtlee_addr);
		eeprom_delay ();
		iow_8 (EE_ENB | dataval | EE_SHIFT_CLK, rtlee_addr);
		eeprom_delay ();
	}
	iow_8 (EE_ENB, rtlee_addr);
	eeprom_delay ();

	for (i = 16; i > 0; i--) {
		iow_8 (EE_ENB | EE_SHIFT_CLK, rtlee_addr);
		eeprom_delay ();
		retval =
		    (retval << 1) | ((ior_8 (rtlee_addr) & EE_DATA_READ) ? 1 :
				     0);
		iow_8 (EE_ENB, rtlee_addr);
		eeprom_delay ();
	}

	/* Terminate the EEPROM access. */
	iow_8 (~EE_CS, rtlee_addr);
	eeprom_delay ();

	return retval;
}

static int rcv_count = 0;

/* tjt - added code 4-8-2005 to just leave packets in the ring and skip them
 * unless they are ARP or IP.  Should add a flag to turn this optimization on
 * or off (and it may just be a BAD idea).
 */
static int
rtl_rcv ( void )
{
	unsigned int status;
	unsigned int ring_offs;
	unsigned int rx_size;
	unsigned int rx_status;
	void * io = rtl_soft.iobase;
	int len, len1;
	unsigned short type;
	int stat;
	struct netbuf *nbp;

#ifdef	DEBUG_RX
	int skip = 1;
#endif

	status = ior_16 ( io + IntrStatus );

	/* See below for the rest of the interrupt acknowledges.  */
	iow_16 ( status & ~(RxFIFOOver | RxOverflow | RxOK), io + IntrStatus);

	ring_offs = rtl_soft.cur_rx;
	rx_status = * (unsigned long *) (rx_ring + ring_offs);
	rx_size = rx_status >> 16;
	rx_status &= 0xffff;

	if ((rx_status & (RxBadSymbol|RxRunt|RxTooLong|RxCRCErr|RxBadAlign)) ||
	    (rx_size < ETH_MIN_SIZE) || (rx_size > ETH_MAX_SIZE + 4)) {
		printf("rx error %hX\n", rx_status);
		rtl_reset();	/* this clears all interrupts still pending */
		return 0;
	}

	/* Received a good packet */
	len = rx_size - 4;	/* no one cares about the FCS */
	++rcv_count;

	/* Tom's funky optimization, see if we can just
	 * avoid copying this packet out of the ring.
	 */
	if (ring_offs+4+len <= RX_BUF_SIZE) {
	    /* common case, not wrapped */
	    type = *(unsigned short *) (rx_ring + ring_offs + 4 + 12);
	    if ( type != ETH_ARP_SWAP && type != ETH_IP_SWAP )
		goto skip;

	} else {
	    /* packet wrapped around end of buffer */
	    len1 = RX_BUF_SIZE - (ring_offs + 4);
	    if ( len1 >= 12 ) {
		type = *(unsigned short *) (rx_ring + ring_offs + 4 + 12);
		if ( type != ETH_ARP_SWAP && type != ETH_IP_SWAP )
		    goto skip;
	    }
	}

#ifdef	DEBUG_RX
	if (ring_offs+4+len <= RX_BUF_SIZE) {
	    printf("rx packet %d: %d bytes", rcv_count, len);
	} else {
	    printf("rx packet %d: %d+%d bytes", rcv_count, len1, len - len1 );
	}
#endif

#ifdef PARANOIA
	/* This should never happen,
	 * but it seemed to once.
	 * Do damage control (and some debug).
	 */
	if ( len > NETBUF_MAX ) {
	    printf ("RTL - monster packet: %d, %08x\n", len, * (unsigned long *) (rx_ring + ring_offs) );
	    printf ("RTL - rx ring at: %08x, pointer at: %08x\n", rx_ring, rx_ring + ring_offs );
	    rtl_reset ();
	    return 0;
	}
#endif

	nbp = netbuf_alloc_i ();
	if ( ! nbp )
	    goto skip;

	nbp->elen = len;

	/* We may still copy some packets that we don't
	 * care about here if they were wrapped around
	 * the ring such that the type field was in the
	 * second fragment, but that is OK, with the
	 * higher layers, they expect them anyhow
	 */
	if (ring_offs+4+len <= RX_BUF_SIZE) {
	    memcpy ( (char *) nbp->eptr, &rx_ring[ring_offs+4], len );
	} else {
	    memcpy ( (char *) nbp->eptr, &rx_ring[ring_offs+4], len1 );
	    memcpy ( ((char *)(nbp->eptr)) + len1, rx_ring, len - len1 );
	}

#ifdef	DEBUG_RX
	type = nbp->eptr->type;
	stat = ior_16 ( io + IntrStatus );

	printf(" at %08x type %04x rxstatus/istatus %04x/%04x\n",
		(unsigned long)(rx_ring+ring_offs+4),
		type, rx_status, stat );
	skip = 0;
#endif

	net_rcv ( nbp );

skip:

#ifdef	DEBUG_RX
	if ( skip )
	    printf("rx skip   %d: %d bytes (type %04x)\n", rcv_count, len, type);
#endif
	rtl_soft.cur_rx = (rtl_soft.cur_rx + rx_size + 4 + 3) & ~3;
	iow_16 ( rtl_soft.cur_rx - 16, io + RxBufPtr);
	rtl_soft.cur_rx %= RX_BUF_SIZE;

	/* See RTL8139 Programming Guide V0.1 for the official handling of
	 * Rx overflow situations.  The document itself contains basically no
	 * usable information, except for a few exception handling rules.  */
	iow_16 ( status & (RxFIFOOver | RxOverflow | RxOK), io + IntrStatus);

	return 1;
}

void
rtl_send ( struct netbuf *nbp )
{
	unsigned int status;
	unsigned long txstatus;
	int cur_tx;
	void * io = rtl_soft.iobase;
	int len;

	/* XXX - spin here !!! yuk.
	 */
	if ( ! rtl_soft.tx_slots ) {
	    printf ("RTL Spin waiting to send packet\n");
	    while ( ! rtl_soft.tx_slots )
		;
	}
	rtl_soft.tx_slots--;

	memcpy ( nbp->eptr->src, rtl_soft.eaddr, ETH_ADDR_SIZE);

	cur_tx = rtl_soft.cur_tx;

	/* Note: RTL8139 doesn't auto-pad, send minimum payload (another 4
	 * bytes are sent automatically for the FCS, totalling to 64 bytes).
	 * Don't sweat filling with zeros, just send any junk.
	 */
	len = nbp->ilen + sizeof(struct eth_hdr);
	if ( len < ETH_MIN_SIZE )
	    len = ETH_MIN_SIZE;

#ifdef	DEBUG_TX
	printf ("RTL sending %d bytes on slot %d\n", len, cur_tx );
#endif

	iow_32 ( (int) nbp->eptr, io + TxAddr0 + cur_tx*4);

	iow_32 ( ((TX_FIFO_THRESH<<11) & 0x003f0000) | len,
		io + TxStatus0 + cur_tx*4);
}

static int isr_count = 0;
static int isr_status = 0x0000;
static int rx_count = 0;
static int tx_count = 0;
static int tx_status = 0x0000;

/* XXX - pull the network cable loose, and you get
 * interrupt 0x20 (Underflow/Link Change) and the show stops
 */

static void
rtl_isr ( void * arg )
{
	void * io = rtl_soft.iobase;

	isr_status = ior_16 ( io + IntrStatus );

#ifdef notdef
	if ( isr_status != RxOK )
	    printf ("RTL interupt: %04x\n", isr_status );

	/* XXX - Disable all further interrupts */
	iow_16 ( 0, io + IntrMask );


	/* now Ack everything !! */
	iow_16 ( isr_status, io + IntrStatus );
#endif

	++isr_count;

	while ( ! (ior_8 ( io + ChipCmd ) & RxBufEmpty) ) {
	    ++rx_count;
	    rtl_rcv ();
	}

	if ( isr_status & (TxOK | TxErr | PCIErr) ) {
	    iow_16 ( (TxOK | TxErr | PCIErr), io + IntrStatus );
	    tx_status = ior_32(io + TxStatus0 + rtl_soft.cur_tx*4);

	    /* click to next transmit register */
	    if (isr_status & TxOK) {
		rtl_soft.cur_tx = (rtl_soft.cur_tx + 1) % NUM_TX_DESC;
	    } else {
		rtl_reset();
	    }
	    rtl_soft.tx_slots++;
	}

#ifdef	DEBUG_TX
	if ( isr_status & TxOK ) {
	    printf("tx done: isr_status %04x tx_status %08x\n",
		isr_status, tx_status);
	}
	if ( isr_status & (TxErr|PCIErr) ) {
	    printf("tx error: isr_status %04x tx_status %08x\n",
		isr_status, tx_status);
	}
#endif
}

static int last_count = 0;

/* debugging entry point, called once a second usually */
void
rtl_show ( void )
{
    	int stat1;
	int stat2;
	void * io = rtl_soft.iobase;

	if ( ! io || ! rtl_soft.active )
	    return;

	stat1 = ior_8 ( io + ChipCmd );
	stat2 = ior_16 ( io + IntrStatus );

	printf ( "RTL count = %d, status (cmd/intr): %02x,  %04x\n", isr_count, stat1, stat2 );

	if ( isr_count > last_count ) {
	    last_count = isr_count;
	    printf ( "isr status: %04x\n", isr_status );
	    printf ( "tx status: %04x\n", tx_status );
	    printf ( "rx count: %d\n", rx_count );
	    printf ( "tx count: %d\n", tx_count );
	}
}

/* THE END */
