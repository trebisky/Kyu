/* Hub75 panel
 *
 * Tom Trebisky  1-9-2025
 */

// First pin is J5-5 == T16 == emio16
// J5-5
// #define TEST_PIN	16
// J5-11
// #define TEST_PIN	17
// J5-12
// #define TEST_PIN	18
// J5-15
// #define TEST_PIN	19

#define OE_HUB	16
#define CLK_HUB	17
#define C_HUB	18
#define B2_HUB	19

// J4-5
// #define TEST_PIN	12
// J4-11  --  E
// #define TEST_PIN	13
// J4-12
// #define TEST_PIN	14
// J4-15
// #define TEST_PIN	15

#define A_HUB	12
#define E_HUB	13
#define R1_HUB	14
#define R2_HUB	15

// J3-5
// #define TEST_PIN	8
// J3-11
// #define TEST_PIN	9
// J3-12
// #define TEST_PIN	10
// J3-15
// #define TEST_PIN	11

#define B1_HUB	8
#define G1_HUB	9
#define G2_HUB	10
#define D_HUB	11

// J2-5
// #define TEST_PIN	4
// J2-11
// #define TEST_PIN	5

#define B_HUB	4
#define LAT_HUB	5

// The following are not used for hub, we use only 14 signals.
// J2-12
// #define TEST_PIN	6
// J2-15
#define TEST_PIN	7

static int hub_pin;
static int hub_state = 0;

/* Generate 100 Hz square wave on whatever pin is selected
 */
static void
hub_wave ( int xxx )
{
		if ( hub_state ) {
			emio_write ( hub_pin, 1 );
			hub_state = 0;
		} else {
			emio_write ( hub_pin, 0 );
			hub_state = 1;
		}
}

static void
hub_tester ( void )
{
		char buf[64];
		int nn;

		printf ( "Testing on pin %d\n", hub_pin );

		for ( ;; ) {
			printf ( "HUB, type number for emio: " );
			getline ( buf, 64 );
			if ( ! buf[0] )
				continue;
			if ( buf[0] == 'q' )
				break;
			nn = atoi ( buf );
			printf ( "You asked for %d\n", nn );
			if ( nn < 0 || nn > 47 ) {
				printf ( "Out of range\n" );
				continue;
			}
			emio_config_output ( nn );
			hub_pin = nn;
			printf ( "Testing on pin %d\n", hub_pin );
		}
}

static void
hub_diag_test ( void )
{
		hub_pin = TEST_PIN;

		emio_config_output ( hub_pin );

		/* run at 1000/5 = 200 Hz - gives 100 Hz square wave*/
		// (void) thr_new_repeat ( "hub", hub_wave, 0, 25, 0, 5 );
		/* run at 1000 = gives 500 Hz square wave*/
		(void) thr_new_repeat ( "hub", hub_wave, 0, 25, 0, 1 );

		hub_tester ();
}

/* The HUB75 interface is not standardized in any official way, but it seems
 * that outfits making this panels have enough sense to allow them to interoperate.
 * The cable has 16 pins.  There are 14 signals and a ground.
 * The extra pin is ambiguous -- it is usually a ground.
 * Here is what the signals are all about:
 * 6 bits for color (two lines, 3 bits for each)
 * 5 bits for a line address (used to be just 4, but that is only 16 lines)
 * The other 3 signals are the interesting ones.
 * clk - a clock to shift the colors into a line.
 * oe - to enable output, pull low to turn on the line.
 * lat - latch signal, pulses high when the line is complete.
 *
 * Different panels can have different rules about oe* and maybe even lat
 * a rising edge on clk should clock data into the panel
 */

static void
hub_init ( void )
{
		// make all emio signals outputs
		emio_config_output ( CLK_HUB );
		emio_config_output ( LAT_HUB );
		emio_config_output ( OE_HUB );

		emio_config_output ( A_HUB );
		emio_config_output ( B_HUB );
		emio_config_output ( C_HUB );
		emio_config_output ( D_HUB );
		emio_config_output ( E_HUB );

		emio_config_output ( R1_HUB );
		emio_config_output ( G1_HUB );
		emio_config_output ( B1_HUB );
		emio_config_output ( R2_HUB );
		emio_config_output ( G2_HUB );
		emio_config_output ( B2_HUB );

		/* initialize these */
		emio_write ( CLK_HUB, 0 );
		emio_write ( LAT_HUB, 0 );
		emio_write ( OE_HUB, 1 );
}

/* colors in a byte are:
 * 0 - 0 - b2, g2, r2 -- b1, g1, r1
 */
static void
hub_line ( int addr, char colors[], int ncolors )
{
		int i;
		int color;

		// turn off the line
		emio_write ( OE_HUB, 1 );

		// I am guessing that A is the lsb
		emio_write ( A_HUB, addr & 1 );
		emio_write ( B_HUB, (addr >> 1) & 1 );
		emio_write ( C_HUB, (addr >> 2) & 1 );
		emio_write ( D_HUB, (addr >> 3) & 1 );
		emio_write ( E_HUB, (addr >> 5) & 1 );

		for ( i=0; i<ncolors; i++ ) {
			color = colors[i];
			emio_write ( R1_HUB, color & 1 );
			emio_write ( G1_HUB, (color >> 1) & 1 );
			emio_write ( B1_HUB, (color >> 2) & 1 );
			emio_write ( R2_HUB, (color >> 3) & 1 );
			emio_write ( G2_HUB, (color >> 4) & 1 );
			emio_write ( B2_HUB, (color >> 5) & 1 );

			// pulse clk
			emio_write ( CLK_HUB, 0 );
			emio_write ( CLK_HUB, 0 );
			emio_write ( CLK_HUB, 0 );
			emio_write ( CLK_HUB, 1 );
		}

		// pulse lat
		emio_write ( LAT_HUB, 1 );
		emio_write ( LAT_HUB, 1 );
		emio_write ( LAT_HUB, 1 );
		emio_write ( LAT_HUB, 0 );

		// turn on the line
		emio_write ( OE_HUB, 0 );
}

static char hello_line[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static void
hub_demo_test ( void )
{
		hub_init ();
		hub_line ( 0, hello_line, 64 );
}

void
hub_test ( void )
{
		hub_diag_test ();
		hub_demo_test ();
}

/* THE END */
