/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * emac_regs.c for the Orange Pi PC and PC Plus  (Allwinner H3)
 *
 * Tom Trebisky  1/10/2023
 *
 */

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

/* -- bits in the ctl0 register */

#define	CTL_FULL_DUPLEX	0x0001
#define	CTL_LOOPBACK	0x0002
#define	CTL_SPEED_1000	0x0000
#define	CTL_SPEED_100	0x000C
#define	CTL_SPEED_10	0x0008

/* -- bits in the ctl1 register */

#define CTL1_SOFT_RESET		0x01
#define CTL1_RX_TX		0x02		/* Rx DMA has prio over Tx when set */
#define CTL1_BURST_8		0x08000000;	/* DMA burst len = 8 (29:24)  */

/* bits in the int_ena and int_stat registers */
#define INT_TX			0x0001
#define INT_TX_DMA_STOP		0x0002
#define INT_TX_BUF_AVAIL	0x0004
#define INT_TX_TIMEOUT		0x0008
#define INT_TX_UNDERFLOW	0x0010
#define INT_TX_EARLY		0x0020
#define INT_TX_ALL	(INT_TX | INT_TX_BUF_AVAIL | INT_TX_DMA_STOP | INT_TX_TIMEOUT | INT_TX_UNDERFLOW )
#define INT_TX_MASK		0x00ff

#define INT_RX			0x0100
#define INT_RX_NOBUF		0x0200
#define INT_RX_DMA_STOP		0x0400
#define INT_RX_TIMEOUT		0x0800
#define INT_RX_OVERFLOW		0x1000
#define INT_RX_EARLY		0x2000
#define INT_RX_ALL	(INT_RX | INT_RX_NOBUF | INT_RX_DMA_STOP | INT_RX_TIMEOUT | INT_RX_OVERFLOW )
#define INT_RX_MASK		0xff00

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
#define	TX_MD			0x00000002

/* Bits in the Rx filter register */
#define	RX_FILT_DIS		0x80000000
#define	RX_DROP_BROAD		0x00020000
#define	RX_ALL_MULTI		0x00010000

/* THE END */
