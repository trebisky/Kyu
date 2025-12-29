/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ccu_reset.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  8/24/2020
 *
 */
#include <arch/types.h>

#define BIT(nr)		(1<<(nr))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* In the R_CCU
 *  see reset/sun8i-r-ccu.h
 */
#define RST_APB0_IR             0
#define RST_APB0_TIMER          1
#define RST_APB0_RSB            2
#define RST_APB0_UART           3
/* 4 is reserved for RST_APB0_W1 on A31 */
#define RST_APB0_I2C            5

/* In the CCU
 *  see reset/sun8i-h3-ccu.h
 */
#define RST_USB_PHY0            0
#define RST_USB_PHY1            1
#define RST_USB_PHY2            2
#define RST_USB_PHY3            3

#define RST_MBUS                4

#define RST_BUS_CE              5
#define RST_BUS_DMA             6
#define RST_BUS_MMC0            7
#define RST_BUS_MMC1            8
#define RST_BUS_MMC2            9
#define RST_BUS_NAND            10
#define RST_BUS_DRAM            11
#define RST_BUS_EMAC            12
#define RST_BUS_TS              13
#define RST_BUS_HSTIMER         14
#define RST_BUS_SPI0            15
#define RST_BUS_SPI1            16
#define RST_BUS_OTG             17
#define RST_BUS_EHCI0           18
#define RST_BUS_EHCI1           19
#define RST_BUS_EHCI2           20
#define RST_BUS_EHCI3           21
#define RST_BUS_OHCI0           22
#define RST_BUS_OHCI1           23
#define RST_BUS_OHCI2           24
#define RST_BUS_OHCI3           25
#define RST_BUS_VE              26
#define RST_BUS_TCON0           27
#define RST_BUS_TCON1           28
#define RST_BUS_DEINTERLACE     29
#define RST_BUS_CSI             30
#define RST_BUS_TVE             31
#define RST_BUS_HDMI0           32
#define RST_BUS_HDMI1           33
#define RST_BUS_DE              34
#define RST_BUS_GPU             35
#define RST_BUS_MSGBOX          36
#define RST_BUS_SPINLOCK        37
#define RST_BUS_DBG             38
#define RST_BUS_EPHY            39
#define RST_BUS_CODEC           40
#define RST_BUS_SPDIF           41
#define RST_BUS_THS             42
#define RST_BUS_I2S0            43
#define RST_BUS_I2S1            44
#define RST_BUS_I2S2            45
#define RST_BUS_I2C0            46
#define RST_BUS_I2C1            47
#define RST_BUS_I2C2            48
#define RST_BUS_UART0           49
#define RST_BUS_UART1           50
#define RST_BUS_UART2           51
#define RST_BUS_UART3           52
#define RST_BUS_SCR0            53

/* New resets imported in H5 */
#define RST_BUS_SCR1            54

struct ccu_reset_map {
        // u16     reg;
        u32     reg;
        u32     bit;
};

/* See clk/sunxi-ng/ccu-sun8i-r.c
 */
static struct ccu_reset_map sun8i_h3_r_ccu_resets[] = {
        [RST_APB0_IR]           =  { 0xb0, BIT(1) },
        [RST_APB0_TIMER]        =  { 0xb0, BIT(2) },
        [RST_APB0_UART]         =  { 0xb0, BIT(4) },
        [RST_APB0_I2C]          =  { 0xb0, BIT(6) },
};

/* See clk/sunxi-ng/ccu-sun8i-h3.c
 */
static struct ccu_reset_map sun50i_h5_ccu_resets[] = {
        [RST_USB_PHY0]          =  { 0x0cc, BIT(0) },
        [RST_USB_PHY1]          =  { 0x0cc, BIT(1) },
        [RST_USB_PHY2]          =  { 0x0cc, BIT(2) },
        [RST_USB_PHY3]          =  { 0x0cc, BIT(3) },

        [RST_MBUS]              =  { 0x0fc, BIT(31) },

        [RST_BUS_CE]            =  { 0x2c0, BIT(5) },
        [RST_BUS_DMA]           =  { 0x2c0, BIT(6) },
        [RST_BUS_MMC0]          =  { 0x2c0, BIT(8) },
        [RST_BUS_MMC1]          =  { 0x2c0, BIT(9) },
        [RST_BUS_MMC2]          =  { 0x2c0, BIT(10) },
        [RST_BUS_NAND]          =  { 0x2c0, BIT(13) },
        [RST_BUS_DRAM]          =  { 0x2c0, BIT(14) },
        [RST_BUS_EMAC]          =  { 0x2c0, BIT(17) },
        [RST_BUS_TS]            =  { 0x2c0, BIT(18) },
        [RST_BUS_HSTIMER]       =  { 0x2c0, BIT(19) },
        [RST_BUS_SPI0]          =  { 0x2c0, BIT(20) },
        [RST_BUS_SPI1]          =  { 0x2c0, BIT(21) },
        [RST_BUS_OTG]           =  { 0x2c0, BIT(23) },
        [RST_BUS_EHCI0]         =  { 0x2c0, BIT(24) },
        [RST_BUS_EHCI1]         =  { 0x2c0, BIT(25) },
        [RST_BUS_EHCI2]         =  { 0x2c0, BIT(26) },
        [RST_BUS_EHCI3]         =  { 0x2c0, BIT(27) },
        [RST_BUS_OHCI0]         =  { 0x2c0, BIT(28) },
        [RST_BUS_OHCI1]         =  { 0x2c0, BIT(29) },
        [RST_BUS_OHCI2]         =  { 0x2c0, BIT(30) },
        [RST_BUS_OHCI3]         =  { 0x2c0, BIT(31) },

        [RST_BUS_VE]            =  { 0x2c4, BIT(0) },
        [RST_BUS_TCON0]         =  { 0x2c4, BIT(3) },
        [RST_BUS_TCON1]         =  { 0x2c4, BIT(4) },
        [RST_BUS_DEINTERLACE]   =  { 0x2c4, BIT(5) },
        [RST_BUS_CSI]           =  { 0x2c4, BIT(8) },
        [RST_BUS_TVE]           =  { 0x2c4, BIT(9) },
        [RST_BUS_HDMI0]         =  { 0x2c4, BIT(10) },
        [RST_BUS_HDMI1]         =  { 0x2c4, BIT(11) },
        [RST_BUS_DE]            =  { 0x2c4, BIT(12) },
        [RST_BUS_GPU]           =  { 0x2c4, BIT(20) },
        [RST_BUS_MSGBOX]        =  { 0x2c4, BIT(21) },
        [RST_BUS_SPINLOCK]      =  { 0x2c4, BIT(22) },
        [RST_BUS_DBG]           =  { 0x2c4, BIT(31) },

        [RST_BUS_EPHY]          =  { 0x2c8, BIT(2) },

        [RST_BUS_CODEC]         =  { 0x2d0, BIT(0) },
        [RST_BUS_SPDIF]         =  { 0x2d0, BIT(1) },
        [RST_BUS_THS]           =  { 0x2d0, BIT(8) },
        [RST_BUS_I2S0]          =  { 0x2d0, BIT(12) },
        [RST_BUS_I2S1]          =  { 0x2d0, BIT(13) },
        [RST_BUS_I2S2]          =  { 0x2d0, BIT(14) },

        [RST_BUS_I2C0]          =  { 0x2d8, BIT(0) },
        [RST_BUS_I2C1]          =  { 0x2d8, BIT(1) },
        [RST_BUS_I2C2]          =  { 0x2d8, BIT(2) },
        [RST_BUS_UART0]         =  { 0x2d8, BIT(16) },
        [RST_BUS_UART1]         =  { 0x2d8, BIT(17) },
        [RST_BUS_UART2]         =  { 0x2d8, BIT(18) },
        [RST_BUS_UART3]         =  { 0x2d8, BIT(19) },
        [RST_BUS_SCR0]          =  { 0x2d8, BIT(20) },
        [RST_BUS_SCR1]          =  { 0x2d8, BIT(20) },
};

#define R_CCU_BASE	(void *) 0x01f01400
#define CCU_BASE 	(void *) 0x01c20000

void
reset_release_all ( void )
{
	int i, n;
	vu32 *lp;
	struct ccu_reset_map *rp;

	printf ( "BIT(31) = %08x\n", BIT(31) );

	n = ARRAY_SIZE(sun8i_h3_r_ccu_resets);

	for ( i=0; i<n; i++ ) {
	    rp = &sun8i_h3_r_ccu_resets[i];
	    if ( rp->reg == 0 )
		continue;
	    lp = (vu32 *) (R_CCU_BASE + rp->reg);
	    *lp |= rp->bit;
	}

	show_reg ( "  R ccu reset ", lp );

	n = ARRAY_SIZE(sun50i_h5_ccu_resets);

	for ( i=0; i<n; i++ ) {
	    rp = &sun50i_h5_ccu_resets[i];
	    if ( rp->reg == 0 )
		continue;
	    lp = (vu32 *) (CCU_BASE + rp->reg);
	    *lp |= rp->bit;
	}

	show_reg ( "  - ccu reset ", lp );
}

/* THE END */
