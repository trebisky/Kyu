/* Zynq - mio pin driver
 *
 * Tom Trebisky  6-17-2024
 */

typedef volatile unsigned int vu32;

#define BIT(bn)                 (1 << (bn))

/* We have 54 MIO pins (0-53).
 * We have thusly 54 mio pin registers
 */

#define NUM_MIO		54

struct mio_pins {
	vu32	ctrl[NUM_MIO];
};

/* Addresses from f800_0000 to f800_0bff are SLCR registers
 * the MIO pin registers are from 700 to 7d4
 * See page 1574 in the TRM, and pages 1668-1669
 */
#define MIO_PIN_BASE	(struct mio_pins *) 0xf8000700

/* Only the low 14 bits in the registers are used.
 */

#define	MIO_DISRCVR	BIT(13)
#define	MIO_PULLUP	BIT(12)
// IO buffertype	11:9
#define	MIO_SPEED	BIT(8)
// Mux select		7:1
#define	MIO_TRIENA	BIT(0)

// IO buffer type values
#define MIO_IO_18	0x200
#define MIO_IO_25	0x400
#define MIO_IO_33	0x600
#define MIO_IO_HSTL	0x800

/* The bootrom write 0xe0 to set up the uart pins.
 * The same for both Rx and Tx.
 * So it leaves all the control values at 0
 * and just sets the mux.
 */

/* The mux bits vary from pin to pin,
 * but for all 54, it seems that
 * setting 0 selects GPIO
 */
#define MIO_MUX_GPIO	0

/* Do not enable tri-state, let the gpio handle that */
// #define MIO_GPIO_VAL	( MIO_DISRCVR | MIO_IO_33 )
#define MIO_GPIO_VAL	0x0

void
mio_init ( void )
{
	// nothing to do
}

void
mio_gpio ( int pin )
{
	struct mio_pins *mp = MIO_PIN_BASE;


	printf ( "MIO for pin %d = %08x\n", pin, &mp->ctrl[pin] );
	mp->ctrl[pin] = MIO_GPIO_VAL;
}

/* THE END */
