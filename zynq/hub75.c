/* Hub75 panel
 *
 * Tom Trebisky  1-9-2025
 */

#define BIT(x)	(1<<(x))

#define FAST_COLOR

/* First a big messy section that has important values
 * for pin allocation and emio assignments.
 * Note that this expects emio47.bit to be in use
 * or some other bitstream that maps these emio bits
 * to board connections in the same way.
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

/* Prototypes --
 */
static void hub_line ( int, char *, int );

/* ================================================== */
/* ================================================== */

/* First we have some very basic diagnostics and
 *  hardware related tests.
 */

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

/* The idea here is similar, but I am going to use a
 * stopwatch to measure how long it takes to crank
 * out 10,000 line
 * I got 4.1 seconds with a 10 Mhz ARM clock.
 */
static void
hub_timing2 ( void )
{
		int i;

		for ( ;; ) {
			printf ( "HUB start\n" );
			for ( i=0; i<10000; i++ )
				hub_line ( 0, hello_line, 64 );
			printf ( "HUB end\n" );
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

/* ================================================== */
/* ================================================== */

/* The HUB75 interface is not standardized in any official way, but it seems
 * that outfits making this panels have enough sense to allow them to interoperate.
 * The cable has 16 pins.  There are 14 signals and a ground.
 * The extra pin is ambiguous -- it is usually a ground.
 * Here is what the signals are all about:
 * 6 bits for color (two lines, 3 bits for each)
 * 5 bits for a line address (used to be just 4, but that is only 16 lines)
 *
 * The other 3 signals are the interesting ones.
 * clk - a clock to shift the colors into a line (rising edge does it)
 * oe - to enable output, pull low to turn on the line.
 * lat - latch signal, pulses high when the line is complete.
 *
 * Different panels can have different rules about oe*
 *  (or so I have been told).
 */

#ifdef FAST_COLOR
#define F1_RED		BIT(14)
#define F1_GREEN	BIT(9)
#define F1_BLUE		BIT(8)
#define F2_RED		BIT(15)
#define F2_GREEN	BIT(10)
#define F2_BLUE		BIT(19)
#endif

#define H1_RED		4
#define H1_GREEN	2
#define H1_BLUE		1
#define H2_RED		0x20
#define H2_GREEN	0x10
#define H2_BLUE		8

#ifdef FAST_COLOR
/* XXX would be better to remap entire line outside of
 * the display loop, than to call this there.
 * We could do this with a lookup table.
 */
static int
to_fast ( int color )
{
		int rv = 0;

		if ( color & H1_RED )
			rv |= F1_RED;
		if ( color & H1_GREEN )
			rv |= F1_GREEN;
		if ( color & H1_BLUE )
			rv |= F1_BLUE;

		if ( color & H2_RED )
			rv |= F2_RED;
		if ( color & H2_GREEN )
			rv |= F2_GREEN;
		if ( color & H2_BLUE )
			rv |= F2_BLUE;

		return rv;
}

static int color_mask;
#endif

static void
hub_init ( void )
{
#ifdef FAST_COLOR
		color_mask = 0;
		color_mask |= F1_RED;
		color_mask |= F1_GREEN;
		color_mask |= F1_BLUE;
		color_mask |= F2_RED;
		color_mask |= F2_GREEN;
		color_mask |= F2_BLUE;
		printf ( "HUB -- color mask: %08x\n", color_mask );
#endif

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
 * 0 - 0 - rgb2 - rgb1
 *
 * Using a scope, I see this code gets run at about 800 Hz.
 * The LAT pulse is 4.5 us in size with 3 writes high.
 */
static void
hub_line ( int addr, char *colors, int ncolors )
{
		int i;
		int color;

		// printf ( "+" );

		// no need to do that here
		// turn off the line
		// emio_write ( OE_HUB, 1 );

		for ( i=0; i<ncolors; i++ ) {
			color = colors[i];
#ifdef FAST_COLOR
			emio_write_m ( to_fast(color), color_mask );
			// emio_write_m ( 0, color_mask );
#endif
#ifndef FAST_COLOR
			emio_write ( B1_HUB, color & 1 );
			emio_write ( G1_HUB, (color >> 1) & 1 );
			emio_write ( R1_HUB, (color >> 2) & 1 );
			emio_write ( B2_HUB, (color >> 3) & 1 );
			emio_write ( G2_HUB, (color >> 4) & 1 );
			emio_write ( R2_HUB, (color >> 5) & 1 );
#endif

#ifdef notdef
			// if we clock things like this the display
			// is entirely blank -- proving that the
			// data gets latched on a rising edge.
			emio_write ( CLK_HUB, 0 );
			emio_write_m ( 0, color_mask );
			emio_write ( CLK_HUB, 1 );
#endif
			// pulse clk
			// rising edge clocks in the data
			emio_write ( CLK_HUB, 1 );
			// emio_write_m ( 0, color_mask );
			emio_write ( CLK_HUB, 0 );
		}

		// blank display while switching lines
		emio_write ( OE_HUB, 1 );

		// A is the lsb
		emio_write ( A_HUB, addr & 1 );
		emio_write ( B_HUB, (addr >> 1) & 1 );
		emio_write ( C_HUB, (addr >> 2) & 1 );
		emio_write ( D_HUB, (addr >> 3) & 1 );
		emio_write ( E_HUB, (addr >> 4) & 1 );

		// pulse lat
		emio_write ( LAT_HUB, 1 );
		emio_write ( LAT_HUB, 0 );

		// turn on the line
		emio_write ( OE_HUB, 0 );
}

/* This just copies the tail end code of the above,
 * the idea being that we have the line data already
 * clocked out and we now just want to display it on
 * a different line.
 * We don't need or want to fool with LAT
 * We do blank the display since we are changing
 * address bits one by one, and that will yield
 * a bunch of crazy addresses while in transition.
 */
static void
hub_dup ( int addr )
{
		// blank display while switching lines
		emio_write ( OE_HUB, 1 );

		// A is the lsb
		emio_write ( A_HUB, addr & 1 );
		emio_write ( B_HUB, (addr >> 1) & 1 );
		emio_write ( C_HUB, (addr >> 2) & 1 );
		emio_write ( D_HUB, (addr >> 3) & 1 );
		emio_write ( E_HUB, (addr >> 4) & 1 );

		// turn display back on
		emio_write ( OE_HUB, 0 );

		/* if we don't insert some delay, the display is
		 * off more than it is on and is not
		 * very bright.
		 */

		// OK, barely noticeable flicker
		// I don't know for sure what the delay_us()
		// calibration is yet.
		delay_us ( 20 );

		// gives wacky refresh waves
		// delay_us ( 100 );
}

/* ================================================== */

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

/* Some things that are common between the original
 * simple random walk and the multi-pixel version.
 */

// never used
// enum color { RED. GREEN, BLUE };

static int row_next[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
static int col_next[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

/* random color every time! */
static int
cycle_color ( int xxx )
{
		// avoid 0
		return 1 + gb_unif_rand ( 7 );
}

/* ================================================== */
/* ================================================== */

/* This is my original simple random walk with only
 * a single pixel
 */

static int row_walk;	// which line
static int col_walk;	// where in line
static int color_walk;
static int walk_count;

static char walk_data[64];

static void hub_rwalk_run_ez ( int );
static void rwalk_show_pos_ez ( void );

static void
hub_rwalk_ez ( void )
{
		// long seed = 99999999977L;
		long seed = 1215752169;

		gb_init_rand ( seed );
		walk_count = 0;
		color_walk = H1_RED;

		row_walk = 32;
		col_walk = 32;
		rwalk_show_pos_ez ();

		// (void) thr_new_repeat ( "hub_rw", hub_rwalk_run_ez, 0, 25, 0, 500 );
		(void) thr_new_repeat ( "hub_rw", hub_rwalk_run_ez, 0, 25, 0, 200 );
}

static void
rwalk_show_pos_ez ( void )
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


/* Runs as a Kyu thread at 2 Hz
 */
static void
hub_rwalk_run_ez ( int xxx )
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
		rwalk_show_pos_ez ();
}


/* ================================================== */
/* ================================================== */

#ifdef notdef
static int row_walk;	// which line
static int col_walk;	// where in line
static int color_walk;
static int walk_count;

static char walk_data[64];
#endif

static char walk_data_hi[64];

static int size_walk;
static int walk_skip;

/* To put more than one line up, we need this which
 * runs at 1000 Hz
 * It scans through as many lines as needed to
 *  be duplicated on the panel.
 */

static int walk_upline;
static int walk_skipper;

static void
rwalk_updater ( int xxx )
{
		int line;

		/* Control brightness */
		if ( walk_skipper < walk_skip ) {
			walk_skipper++;
			/* blank the display this tick */
			emio_write ( OE_HUB, 0 );
			return;
		}
		walk_skipper = 0;

		walk_upline++;
		if ( walk_upline >= size_walk )
			walk_upline = 0;

		line = row_walk + walk_upline;
		if ( line < 32 )
			hub_line ( line, walk_data, 64 );
		else
			hub_line ( line - 32, walk_data_hi, 64 );
}

static void
rwalk_show_pos ( void )
{
		int i;
		int color;

		for ( i=0; i<64; i++ ) {
			walk_data[i] = 0;
			walk_data_hi[i] = 0;
		}

		for ( i=col_walk; i<col_walk+size_walk; i++ ) {
			walk_data[i] = color_walk;
			walk_data_hi[i] = color_walk << 3;
		}

		// let the updater handle this.
		// hub_line ( row_walk % 32, walk_data, 64 );
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
			if ( rnext < 0 || rnext + (size_walk-1) >= 64 )
				continue;
			if ( cnext < 0 || cnext + (size_walk-1) >= 64 )
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

		// Allow 2x2 or 4x4 or ... */
		size_walk = 1;
		walk_upline = 0;

		/* Control brightness */
		walk_skipper = 0;
		// too slow with 3x3
		// walk_skip = 20;

		// OK with 2x2
		// walk_skip = 10;

		walk_skip = 25;

		// starting position
		row_walk = 32;
		col_walk = 32;
		rwalk_show_pos ();

		(void) thr_new_repeat ( "hub_rw", hub_rwalk_run, 0, 40, 0, 200 );
		(void) thr_new_repeat ( "hub_up", rwalk_updater, 0, 50, 0, 1 );
}

/* ================================================== */
/* ================================================== */

/* This lights up the whole display.
 *
 * Using the 1000 Hz timer via the Kyu task delay yields
 *  quite a bit of flicker.
 * With a 1000 Hz line update rate and a loop over 32 lines,
 *   that is a 31 Hz update, and that just doesn't cut it
 *   for a nice display.
 * When I do the refresh via a endless loop, I never get
 *   the Kyu prompt, but the display is decent with a
 *   barely detectable flicker.
 *
 * The idea here was to demonstrate that once we have clocked
 *  in a line we only need to change the display address.
 */

static char sweep_data[64];

static int sweep_line;

static int
sweeper ( int xxx )
{
		hub_dup ( sweep_line );

		sweep_line++;
		if ( sweep_line > 31 )
			sweep_line = 0;
}

static void
hub_sweep ( void )
{
		int i;
		int data;

		data = H1_RED | H2_BLUE;
		for ( i=0; i<32; i++ )
			sweep_data[i] = data;

		data = H1_GREEN | H2_BLUE | H2_GREEN;
		for ( i=32; i<64; i++ )
			sweep_data[i] = data;

		sweep_line = 0;

		/* Clock the line in once */
		hub_line ( sweep_line, sweep_data, 64 );

		for ( ;; )
			sweeper ( 0 );

		// (void) thr_new_repeat ( "hub_sw", sweeper, 0, 50, 0, 1 );
}

/* ================================================== */
/* ================================================== */

static void
hub_demo_test ( void )
{
		printf ( "Running HUB75 demo\n" );

		hub_init ();

		// hub_check ();
		// hub_timing ();
		// hub_timing2 ();
		// lat_loop ();

		// hub_test1 ();
		// hub_test2 ();

		hub_rwalk ();
		// hub_rwalk_ez ();
		// hub_sweep ();

		printf ( "HUB75 demo is launched (done)\n" );
}

void
hub_test ( void )
{
		// hub_diag_test ();
		hub_demo_test ();
}

/* THE END */
