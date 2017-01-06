/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* BBB mux control */

#define CTRL_BASE	0x44e10000

/* This is by far the ugliest part of the am3358
 *  namely the "pinmux" as it has been nicknamed.
 * Don't expect to find it under that name in the
 *  technical documentation though, and you will
 *  not find the full story in any one document either.
 * Part of what you need to know is under "Control Module"
 *  in section 9 of the TRM around page 1124.
 * The other part is in the data sheet in a big table
 *  that begins on page 19 entitled "Ball Characteristics"
 */

#include "omap_mux.h"

/* Constants from U-boot
 * arch/arm/include/asm/arch-am33xx/mux_am33xx.h */

#ifdef notdef
#define SLEWCTRL        (0x1 << 6)
#define RXACTIVE        (0x1 << 5)
#define PULLDOWN_EN     (0x0 << 4) /* Pull Down Selection */
#define PULLUP_EN       (0x1 << 4) /* Pull Up Selection */
#define PULLUDEN        (0x0 << 3) /* Pull up enabled */
#define PULLUDDIS       (0x1 << 3) /* Pull up disabled */
#endif

/* By default you get the pulldown enabled
 */
#define SLEWCTRL        0x40	/* set for slow (long wires) */
#define RXACTIVE        0x20	/* set to enable receiver (for input) */
#define PULLUP          0x10	/* Pull Up Selected (else pulldown) */
#define NOPULL          0x08	/* Pull up/down disabled */

#define MODE(val)       val     /* used for Readability */

#define MUX_BASE	(CTRL_BASE + 0x800)

void
mux_init ( void )
{
    int *psp = (int *) MUX_BASE;

    /*
     * Check offsets in structure
    printf ( "warmrstn at %08x\n", &psp[WARMRSTN] );
    printf ( "rtc_pwronrstn at %08x\n", &psp[RTC_PWRONRSTN] );
    printf ( "usb1_drvvbus at %08x\n", &psp[USB1_DRVVBUS] );
    */
}

void
setup_uart0_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_UART0_RXD] = MODE(0) | PULLUP | RXACTIVE;
    psp[MUX_UART0_TXD] = MODE(0);
}

void
setup_i2c0_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C0_SDA] = MODE(0) | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C0_SCL] = MODE(0) | RXACTIVE | SLEWCTRL;
}

void
setup_i2c1_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C1_SDA] = MODE(2) | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C1_SCL] = MODE(2) | RXACTIVE | SLEWCTRL;
}

void
setup_i2c2_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C2_SDA] = MODE(2) | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C2_SCL] = MODE(2) | RXACTIVE | SLEWCTRL;
}

/* For iic via gpio */
void
setup_iic_mux ( int pin )
{
    int *psp = (int *) MUX_BASE;

    psp[pin] = MODE(7) | RXACTIVE | SLEWCTRL;
}

void
setup_spi0_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_SPI0_SCLK] = MODE(0) | RXACTIVE;
    psp[MUX_SPI0_D0]  =  MODE(0) | PULLUP | RXACTIVE;
    psp[MUX_SPI0_D1]  =  MODE(0) | RXACTIVE;
    psp[MUX_SPI0_CS0]  = MODE(0) | PULLUP | RXACTIVE;
}

void
setup_led_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_GPMC_A5] = MODE(7) | NOPULL;
    psp[MUX_GPMC_A6] = MODE(7) | NOPULL;
    psp[MUX_GPMC_A7] = MODE(7) | NOPULL;
    psp[MUX_GPMC_A8] = MODE(7) | NOPULL;
}

void
setup_gpio_out ( int pin )
{
    int *psp = (int *) MUX_BASE;

    psp[pin] = MODE(7) | NOPULL;
}

/* input with pullup */
void
setup_gpio_in_up ( int pin )
{
    int *psp = (int *) MUX_BASE;

    psp[pin] = MODE(7) | PULLUP | RXACTIVE;
}

/* input with pulldown */
void
setup_gpio_in_down ( int pin )
{
    int *psp = (int *) MUX_BASE;

    psp[pin] = MODE(7) | RXACTIVE;
}

/* input with neither */
void
setup_gpio_in ( int pin )
{
    int *psp = (int *) MUX_BASE;

    psp[pin] = MODE(7) | RXACTIVE | NOPULL;
}

/* THE END */
