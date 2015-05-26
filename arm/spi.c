/* spi.c
 *
 * Tom Trebisky  Kyu project  5-25-2015
 *
 * The am3359 has 2 spi interfaces.
 *  spi0, and spi1
 *
 * Each of these can drive 5 pins.
 *  (SCLK, D0, D1, CS0, and CS1)
 * Typically CS1 is not required and only 
 *  4 pins are used to connect a device
 *
 */

#define SPI0_BASE      0x48030000
#define SPI1_BASE      0x481A0000

void
spi_init ( void )
{
}

/* XXX - tentative name */

void
spi_spi0_setup ( void )
{
	setup_spi0_mux ();
}

/* THE END */
