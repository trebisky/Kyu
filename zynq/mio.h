/* Zynq - mio definitions for the Antminer S9
 *
 * Tom Trebisky  11-25-2024
 */

/* The Zynq has 54 MIO pins that are routed to pins
 * and thus off-chip.
 * Some of these have dedicated purposes on our board
 * (such as NAND, ethernet, SD card)
 * They can all be configured via an array of 54 registers
 * in the SLCR section of the chip.
 * A diagram on page 52 of the TRM is entitled
 *  "MIO at a glance" and summarizes things.
 * The register array is on pages 1573 to 1575.
 */

/* pins 0-15 are NAND memory */

/* A sneaky super bright green LED on the board near the
 * Zynq chip on the side towards the ethernet connector.
 * Marked "D2" on the silkscreen.
 */
#define MIO_BOARD_LED	15

/* pin 16-27 are ethernet */

/* Here we have 9 pins, on in each of the 18 pin
 * connectors (J1 through J9)
 */
#define MIO_28_EN1		28
#define MIO_29_EN2		29
#define MIO_30_EN3		30
#define MIO_31_EN4		31
#define MIO_32_EN5		32
#define MIO_33_EN6		33
#define MIO_34_EN7		34
#define MIO_35_EN8		35
#define MIO_36_EN9		36

/* The two big LED visible near the S1 button */
#define MIO_RED_LED		37
#define MIO_GREEN_LED	38

/* components for beep are missing on my boards */
#define MIO_BEEP		39

/* pins 40-46 - SD card */

/* S1 button - "key reset" - I move jumpers so
 * that the button does hardware reset and this
 * is no longer connected.
 */
#define MIO_S1			47

/* Don't mess with these, but configure them */
#define MIO_UART_TX		48
#define MIO_UART_RX		49

/* pin 50 -- SD card write protect */

/* The other button - labeled "IP-GET"
 */
#define MIO_S2		51

/* pins 52 and 53 are MDC and MDIO for ethernet Phy */

/* THE END */
