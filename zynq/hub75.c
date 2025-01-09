/* Hub75 panel
 *
 * Tom Trebisky  1-9-2025
 */

// First pin is J5-5 == T16 == emio16
#define TEST_PIN	16

static int hub_pin;
static int hub_state = 0;

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

void
hub_test ( void )
{
		hub_pin = TEST_PIN;

		emio_config_output ( hub_pin );

		/* run at 1000/5 = 200 Hz */
		(void) thr_new_repeat ( "hub", hub_wave, 0, 25, 0, 5 );
}

/* THE END */
