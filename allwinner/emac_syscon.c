/* ------------------------------------------------------------ */
/* Syscon stuff
 * Kyu - Tom Trebisky  4/14/2026
 *
 * This is section 4.5 in the H5 TRM, page 161
 * There are two registers.
 * - at offset 0x24 is what they call a "Version Register"
 *   we simply ignore this.
 * - at offset 0x30 is what they call the "EMAC_EPHY_CLK_REG"
 *   and that is what we are dealing with here.
 *
 * So why does this thing exist, and why is it outside of
 * the address space allocated to the emac registers.
 * My guess is this -- Allwinner decided to add the internal
 *  PHY and this was added along with it.  The internal
 *  PHY was/is all about making things even cheaper, and
 *  was a last minute add-on, along with theese registers.
 *
 * The so called "version register" is poorly named and
 *  contains one read only bit, described as
 *  U-Boot select pad status.
 * This is probably pin W6, which is labelled as "UBOOT".
 * The schematic for the Orange Pi PC2 (H5) shows this
 *  pin pulled to Vcc by a 47K resistor.
 * The Vcc is mysteriously labeled "VCC-KEY_PWR"
 */

/* This special syscon register for the emac is outside of
 * the regular address space for the emac.
 * It holds bits that by and large have to do with the
 * PHY interface.
 * In particular, there is a bit here that selects whether
 * the internal PHY is used (as for the H3) or
 * and external PHY (as for the H5 which uses the Realtek 8211).
 */

#include <arch/types.h>
#include <kyu.h>

#define EMAC_SYSCON	((u32 *) 0x01c00030)

/* This is a single 32 bit register that controlls EMAC clocks and such */
/* These are only some of the bit definitions, but more than we need */

/* The Orange Pi schematics show us the jumpers that set the PHY
 * device address (namely 1).  However, it seems to work perfectly
 * well also to just set the address field to 0
 */
#define PHY_ADDR		1
#define SYSCON_PHY_ADDR		(PHY_ADDR<<20)

#define SYSCON_CLK24			0x40000		/* set for 24 Mhz clock (else 25) */
#define SYSCON_LEDPOL			0x20000		/* set for LED active low polarity */
#define SYSCON_SHUTDOWN			0x10000		/* set to power down PHY */
#define SYSCON_EPHY_INTERNAL	0x08000		/* set to use internal PHY */

/* The shutdown bit pertains to the internal PHY as near as I can tell,
 * which sort of makes sense -- you may as well leave the internal
 * PHY powered down if you are using an external PHY instead.
 */

#define SYSCON_RXINV		0x10	/* set to invert Rx Clock */
#define SYSCON_TXINV		0x8		/* set to invert Tx Clock */
#define SYSCON_PIT			0x4		/* PHY interface type, set for RGMII, else MII */

/* TCS - transmitter clock source */
/* H3 uses MII, our H5 can only use RGMII, no GMII is available */
/* The H5 could use MII if not running gigabit ?? */
#define SYSCON_TCS_MII			0x0		/* MII clock */
#define SYSCON_TCS_RGMII_EXT	0x1		/* RGMII clock external */
#define SYSCON_TCS_RGMII_INT	0x2		/* RGMII clock internal */
#define SYSCON_TCS_XXX			0x3		/* - reserved - */

/* U-Boot has already set all this up properly,
 * and we could just rely on inheriting that setup
 * and do away with this routine entirely,
 * However, that is not how we want to play the game.
 */
void
emac_syscon_setup ( void )
{
	vu32 *sc = EMAC_SYSCON;

	printf ( "Emac Syscon = %08x\n", *sc );

#ifdef BOARD_H3
	*sc = SYSCON_EPHY_INTERNAL | SYSCON_CLK24 | SYSCON_PHY_ADDR;
#else
	*sc = SYSCON_CLK24 | SYSCON_PHY_ADDR | SYSCON_SHUTDOWN | SYSCON_PIT | SYSCON_TCS_RGMII_INT;
#endif

	printf ( "Emac Syscon = %08x\n", *sc );
}

/* On the H3, I see:
 * I see: Emac Syscon =     00148000 before and after
 * the datasheet default is 00058000
 * The "1" is the PHY ADDR
 * The "5" going to "4" is clearing the shutdown bit.
 * On the H3 I tried using the 25 Mhz clock and autonegotion
 * then simply fails.
// *sc = SYSCON_EPHY_INTERNAL | (SYSCON_PHY_ADDR<<20);
 */

/* On the H5, I see this coming from U-Boot:
 *    00050006
 * The shutdown bit is set.
 * The internal PHY bit is NOT set.
 * The Tx clock is RGMII internal
 */

/* THE END */
