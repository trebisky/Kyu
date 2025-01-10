/* Hub75 panel
 *
 * Tom Trebisky  1-9-2025
 */

// First pin is J5-5 == T16 == emio16
// #define TEST_PIN	16

// J5-15
// #define TEST_PIN	17
// J5-12
// #define TEST_PIN	18
// J5-11
#define TEST_PIN	19

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
			if ( nn < 5 || nn > 47 ) {
				printf ( "Out of range\n" );
				continue;
			}
			emio_config_output ( nn );
			hub_pin = nn;
			printf ( "Testing on pin %d\n", hub_pin );
		}
}

void
hub_test ( void )
{
		hub_pin = TEST_PIN;

		emio_config_output ( hub_pin );

		/* run at 1000/5 = 200 Hz */
		(void) thr_new_repeat ( "hub", hub_wave, 0, 25, 0, 5 );

		hub_tester ();
}

/* THE END */
