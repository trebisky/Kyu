/* i2c.c
 *
 * Tom Trebisky  Kyu project  5-25-2015
 *
 * The am3359 has 3 i2c interfaces.
 *  i2c0, i2c1, and i2c2
 * on the BBB, the i2c0 is used for a few on board
 *  devices and signals for it are not brought offboard.
 * i2c1 is available as 2 pins (SCL and SDA)
 * i2c2 is available as 2 pins (SCL and SDA)
 *
 */

#define I2C0_BASE      0x44E0B000
#define I2C1_BASE      0x4802A000
#define I2C2_BASE      0x4819C000

/* We only set the pinux for the "0" interface
 * initially.
 */
void
i2c_init ( void )
{
	setup_i2c0_mux ();
}

/* XXX - tentative names */

/* Puts this on P9-17 and P9-18 */
void
i2c_i2c1_setup ( void )
{
	setup_i2c1_mux ();
}

/* Puts this on P9-21 and P9-22 */
void
i2c_i2c2_setup ( void )
{
	setup_i2c2_mux ();
}

/* THE END */
