/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 *  Kyu Project
 *
 * cm.c - driver code for the am3359 control module.
 *  the pinmux part of this device is in mux.c
 *
 * Tom Trebisky  5-25-2015
 */

#include <kyulib.h>

#define CTRL_BASE	0x44e10000

struct device_ctrl {
	volatile unsigned int id;
        int _pad1[7];
        volatile unsigned int usb_ctrl0;
        int _pad2;
        volatile unsigned int usb_ctrl1;
        int _pad3;
        volatile unsigned int macid_0l;
        volatile unsigned int macid_0h;
        volatile unsigned int macid_1l;
        volatile unsigned int macid_1h;
        int _pad4[4];
        volatile unsigned int miisel;
        int _pad5[7];
        volatile unsigned int mreqprio_0;
        volatile unsigned int mreqprio_1;
        int _pad6[97];
        volatile unsigned int efuse_sma;
};

#define DEVICE_BASE	( (struct device_ctrl *) (CTRL_BASE + 0x600))

/* From arch/arm/include/asm/arch-am33xx/cpu.h */
/* gmii_sel register defines
 */
#define GMII1_SEL_MII           0x0
#define GMII1_SEL_RMII          0x1
#define GMII1_SEL_RGMII         0x2
#define GMII2_SEL_MII           0x0
#define GMII2_SEL_RMII          0x4
#define GMII2_SEL_RGMII         0x8
#define RGMII1_IDMODE           BIT(4)
#define RGMII2_IDMODE           BIT(5)
#define RMII1_IO_CLK_EN         BIT(6)
#define RMII2_IO_CLK_EN         BIT(7)

#define MII_MODE_ENABLE         (GMII1_SEL_MII | GMII2_SEL_MII)
#define RMII_MODE_ENABLE        (GMII1_SEL_RMII | GMII2_SEL_RMII)
#define RGMII_MODE_ENABLE       (GMII1_SEL_RGMII | GMII2_SEL_RGMII)
#define RGMII_INT_DELAY         (RGMII1_IDMODE | RGMII2_IDMODE)
#define RMII_CHIPCKL_ENABLE     (RMII1_IO_CLK_EN | RMII2_IO_CLK_EN)

void
cm_mii_enable ( void )
{
	struct device_ctrl *cmp = DEVICE_BASE;

	cmp->miisel = MII_MODE_ENABLE;
}

/* This first one we use */
void
cm_get_mac0 ( char *buf )
{
	unsigned int data;
	struct device_ctrl *cmp = DEVICE_BASE;

	data = cmp->macid_0h;
        buf[0] = data & 0xFF;
        buf[1] = (data & 0xFF00) >> 8;
        buf[2] = (data & 0xFF0000) >> 16;
        buf[3] = (data & 0xFF000000) >> 24;

	data = cmp->macid_0l;
        buf[4] = data & 0xFF;
        buf[5] = (data & 0xFF00) >> 8;
}

#ifdef why_bother
/* This second one is for the useless second interface */
void
cm_get_mac1 ( char *buf )
{
	unsigned int data;
	struct device_ctrl *cmp = DEVICE_BASE;

	data = cmp->macid_1h;
        buf[0] = data & 0xFF;
        buf[1] = (data & 0xFF00) >> 8;
        buf[2] = (data & 0xFF0000) >> 16;
        buf[3] = (data & 0xFF000000) >> 24;

	data = cmp->macid_1l;
        buf[4] = data & 0xFF;
        buf[5] = (data & 0xFF00) >> 8;
}
#endif

static void
show_mac ( char *mac )
{
	int i;

	for ( i=0; i<6; i++ ) {
	    if ( i == 0 )
		printf ( "%02x", mac[i] );
	    else
		printf ( " %02x", mac[i] );
	}
}

void
cm_adc_mux ( int val )
{
	volatile unsigned long *adcmux = (unsigned long *) (CTRL_BASE + 0xFD8);

	*adcmux = val;
}

/* XXX - Useless */
void
cm_init ( void )
{
#ifdef notdef
	char mac[6];

	/* This first one we use */
	cm_get_mac0 ( mac );

	printf ( "MAC 0: " );
	show_mac ( mac );
	printf ( "\n" );

	/* -- */

	/* This second one is for the useless second interface */
	cm_get_mac1 ( mac );

	printf ( "MAC 1: " );
	show_mac ( mac );
	printf ( "\n" );
#endif
}

/* THE END */
