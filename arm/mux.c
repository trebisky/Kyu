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

#define SLEWCTRL        (0x1 << 6)
#define RXACTIVE        (0x1 << 5)
#define PULLDOWN_EN     (0x0 << 4) /* Pull Down Selection */
#define PULLUP_EN       (0x1 << 4) /* Pull Up Selection */
#define PULLUDEN        (0x0 << 3) /* Pull up enabled */
#define PULLUDDIS       (0x1 << 3) /* Pull up disabled */
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

    psp[MUX_UART0_RXD] = MODE(0) | PULLUP_EN | RXACTIVE;
    psp[MUX_UART0_TXD] = MODE(0) | PULLUDEN;
}

void
setup_i2c0_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C0_SDA] = MODE(0) | PULLUDEN | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C0_SCL] = MODE(0) | PULLUDEN | RXACTIVE | SLEWCTRL;
}

void
setup_i2c1_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C1_SDA] = MODE(2) | PULLUDEN | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C1_SCL] = MODE(2) | PULLUDEN | RXACTIVE | SLEWCTRL;
}

void
setup_i2c2_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_I2C2_SDA] = MODE(2) | PULLUDEN | RXACTIVE | SLEWCTRL;
    psp[MUX_I2C2_SCL] = MODE(2) | PULLUDEN | RXACTIVE | SLEWCTRL;
}

void
setup_spi0_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_SPI0_SCLK] = MODE(0) | PULLUDEN | RXACTIVE;
    psp[MUX_SPI0_D0]  =  MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE;
    psp[MUX_SPI0_D1]  =  MODE(0) | PULLUDEN | RXACTIVE;
    psp[MUX_SPI0_CS0]  = MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE;
}

void
setup_led_mux ( void )
{
    int *psp = (int *) MUX_BASE;

    psp[MUX_GPMC_A5] = MODE(7) | PULLUDDIS;
    psp[MUX_GPMC_A6] = MODE(7) | PULLUDDIS;
    psp[MUX_GPMC_A7] = MODE(7) | PULLUDDIS;
    psp[MUX_GPMC_A8] = MODE(7) | PULLUDDIS;
}

/* THE END */
