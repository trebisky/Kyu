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

/* This gives a nice 318 kHz square wave.
 * The on period is 1.56 us
 */
static void
hub_check ( void )
{
		for ( ;; ) {
			emio_write ( CLK_HUB, 1 );
			emio_write ( CLK_HUB, 0 );
		}
}

/* colors in a byte are:
 * 0 - 0 - rgb2 - rgb1
 *
 * Using a scope, I see this code gets run at about 800 Hz.
 * The LAT pulse is 4.5 us in size with 3 writes high.
 */
static void
hub_line ( int addr, char colors[], int ncolors )
{
		int i;
		int color;

		// printf ( "+" );

		// turn off the line
		emio_write ( OE_HUB, 1 );

		// I am guessing that A is the lsb
		emio_write ( A_HUB, addr & 1 );
		emio_write ( B_HUB, (addr >> 1) & 1 );
		emio_write ( C_HUB, (addr >> 2) & 1 );
		emio_write ( D_HUB, (addr >> 3) & 1 );
		emio_write ( E_HUB, (addr >> 4) & 1 );

		for ( i=0; i<ncolors; i++ ) {
			color = colors[i];
			emio_write ( B1_HUB, color & 1 );
			emio_write ( G1_HUB, (color >> 1) & 1 );
			emio_write ( R1_HUB, (color >> 2) & 1 );
			emio_write ( B2_HUB, (color >> 3) & 1 );
			emio_write ( G2_HUB, (color >> 4) & 1 );
			emio_write ( R2_HUB, (color >> 5) & 1 );

			// pulse clk
			emio_write ( CLK_HUB, 1 );
			emio_write ( CLK_HUB, 0 );
		}

		// pulse lat
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
	0, 0, 0, 0, 0, 4, 2, 1
};

/* This is the first thing that ever worked.
 * It lights up the last 3 pixels on the top line RGB
 * Putting this in a loop simply reduces the brightness,
 * as we have the LED's off during the update.
 */
static void
hub_test1 ( void )
{
		hub_line ( 0, hello_line, 64 );
}

/* The idea here is to use the scope, and look at
 * LAT to see how fast we can crank out lines going
 * as fast as possible.
 * It looks like 1100 Hz !!
 * This sort of makes sense.  There are 521 calls
 * to emio_write() involved sending a line, each
 * yields a 1.56 us timing, so that totals to
 * 813 us which would be 1230 Hz.  We must have
 * a bit of extra overhead and/or that 1.56 timing
 * is not the whole story.
 * Whatever the case, to send 32 lines and refresh
 * the entire panel can only be done in this manner
 * and get a 34 Hz refresh rate.
 */
static void
hub_timing ( void )
{
		for ( ;; )
			hub_line ( 0, hello_line, 64 );
}

#define H1_RED		4
#define H1_GREEN	2
#define H1_BLUE		1
#define H2_RED		0x20
#define H2_GREEN	0x10
#define H2_BLUE		8

static int t2_line;
static char t2_data[64];

static void
t2_mkline ( int line, char data[] )
{
		int i;

		for ( i=0; i<64; i++ )
			data[i] = 0;

		data[line] = H1_RED;
		data[line+32] = H1_GREEN;
		data[31-line] |= H2_BLUE;
		data[63-line] |= H2_RED;
}

/* Runs as a Kyu thread at 1000 Hz
 */
static void
hub_t2 ( int xxx )
{
		t2_mkline ( t2_line, t2_data );
		hub_line ( t2_line, t2_data, 64 );

		t2_line += 1;
		if ( t2_line >= 32 )
			t2_line = 0;
}

static void
hub_test2 ( void )
{
		t2_line = 0;
		(void) thr_new_repeat ( "hub_t2", hub_t2, 0, 25, 0, 1 );
}

/* ================================================== */
/* ================================================== */

// enum color { RED. GREEN, BLUE };

static int row_walk;	// which line
static int col_walk;	// where in line
static int color_walk;
static int walk_count;

static char walk_data[64];

static void
rwalk_show_pos ( void )
{
		int i;

		for ( i=0; i<64; i++ )
			walk_data[i] = 0;

		if ( row_walk < 32 )
			walk_data[col_walk] = color_walk;
		else
			walk_data[col_walk] = color_walk << 3;

		hub_line ( row_walk % 32, walk_data, 64 );
}

/* random color every time! */
static int
cycle_color ( int xxx )
{
		// avoid 0
		return 1 + gb_unif_rand ( 7 );
}

#ifdef notdef
/* random color every 20 seconds */
static int
cycle_color ( int color )
{
		walk_count++;
		if ( walk_count < 20*5 ) {
			return color;
		}

		walk_count = 0;

		// avoid 0
		return 1 + gb_unif_rand ( 7 );
}

static int
cycle_color ( int color )
{
		walk_count++;
		if ( walk_count < 30*5 ) {
			return color;
		}

		walk_count = 0;

		switch ( color ) {
			case H1_RED:
				return H1_GREEN;
			case H1_GREEN:
				return H1_BLUE;
			case H1_BLUE:
				return H1_RED;
			default:
				return H1_RED;
		}
}
#endif

static int row_next[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
static int col_next[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

/* Runs as a Kyu thread at 2 Hz
 */
static void
hub_rwalk_run ( int xxx )
{
		long rnum;
		int rnext, cnext;

		for ( ;; ) {
			rnum = gb_unif_rand ( 8 );
			rnext = row_walk + row_next[rnum];
			cnext = col_walk + col_next[rnum];
			if ( rnext < 0 || rnext >= 64 )
				continue;
			if ( cnext < 0 || cnext >= 64 )
				continue;
			break;
		}

		/* Change color every 30 seconds */
		color_walk = cycle_color ( color_walk );

		row_walk = rnext;
		col_walk = cnext;
		rwalk_show_pos ();
}

static void
hub_rwalk ( void )
{
		// long seed = 99999999977L;
		long seed = 1215752169;

		gb_init_rand ( seed );
		walk_count = 0;
		color_walk = H1_RED;

		row_walk = 32;
		col_walk = 32;
		rwalk_show_pos ();

		// (void) thr_new_repeat ( "hub_rw", hub_rwalk_run, 0, 25, 0, 500 );
		(void) thr_new_repeat ( "hub_rw", hub_rwalk_run, 0, 25, 0, 200 );
}

static void
hub_demo_test ( void )
{
		printf ( "Running HUB75 demo\n" );

		hub_init ();

		// hub_check ();
		// hub_timing ();

		// hub_test1 ();
		// hub_test2 ();
		hub_rwalk ();

		printf ( "HUB75 demo is launched (done)\n" );
}

void
hub_test ( void )
{
		// hub_diag_test ();
		hub_demo_test ();
}

/* THE END */
