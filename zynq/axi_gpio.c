/* Drive for an axi gpio device that has been
 * established in the PL by downloading an appropriate
 * bitstream to the Zynq FPGA.
 *
 * Tom Trebisky  6-7-2022
 *
 * Copied from /u1/Projects/Zynq/Git_ebaz/fabric/axi_gpio.c
 */

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#define BIT(x)  (1<<(x))

/* Places to look:
 *
 * The best is PG144, the product guide for the AXI GPIO block we are using.
 *
 * But example code can be found in the SDK:
 *
 * In /u1/Xilinx/SDK/2019.1
 *  data/embeddedsw/XilinxProcessorIPLib/drivers/gpio_v4_4/src/xgpio.c
 *  data/embeddedsw/XilinxProcessorIPLib/drivers/gpio_v4_3/src/xgpio.c
 * In /u1/Xilinx/Vitis/2021.2
 *  data/embeddedsw/XilinxProcessorIPLib/drivers/gpio_v4_9/src/xgpio.c
 */

/* dir is called "tri" and tri is more accurate.
 * for input, we use this to tristate the output signal buffer.
 */
struct axi_gpio {
	vu32	data;
	vu32	dir;	// "tri"
	vu32	data2;
	vu32	dir2;
};

// The first address is the usual default.
// I used "address editor" as an experiment to switch to the second.
// #define AXI_GPIO_BASE	((struct axi_gpio *) 0x41200000)
#define AXI_GPIO_BASE	((struct axi_gpio *) 0x41000000)

void
axi_gpio_write ( u32 val )
{
	struct axi_gpio *ap = AXI_GPIO_BASE;

	ap->data = val;
}

u32
axi_gpio_read ( void )
{
	struct axi_gpio *ap = AXI_GPIO_BASE;

	return ap->data;
}

/* Set 1 for inputs */
void
axi_gpio_dir ( u32 val )
{
	struct axi_gpio *ap = AXI_GPIO_BASE;

	ap->dir = val;
}

static int blink_state = 0;

/* blinkob for "blink onboard"
 * This was for a bitstream that contolled all
 * four on board LED on the s9 board
 * 12-18-2024
 * This is tolerable with a 1ms delay.
 * Anything more will blind you.
 */
static void
blinkob ( void )
{
	if ( blink_state ) {
		blink_state = 0;
	    axi_gpio_write ( 0x05 );
	    thr_delay ( 1 );
	    axi_gpio_write ( 0x0f );
	} else {
		blink_state = 1;
	    axi_gpio_write ( 0x0a );
	    thr_delay ( 1 );
	    axi_gpio_write ( 0x0f );
	}
}

void led_run ( void );

void
axi_gpio_test ( void )
{
	axi_gpio_dir ( 0 );

	// Controlled by FPGA
	// led_run ();

	// Controlled by user code.
	(void) thr_new_repeat ( "ledob", blinkob, 0, 25, 0, 1000 );
}

/* ============================================ */
/* Below here depends on fancy.v in the PL
 * This was used with bare metal code on ebaz
 * with just 2 LED
 */

/* ledio bitstream documentation --
 * We do a 32 bit write, but only the low 8 bits matter.
 * There are two 4 bit groups (and they are identical)
 * The upper controls the green LED, the lower does the red.
 *
 * The 4 bits are:  UPSS
 * The U bit allows direct control of the LED by software on the ARM side.
 * The P bit (phase) lets us invert the signal to the LED.
 *  with P = 0, you write a 0 to U to turn the LED on
 *  with P = 1, you write a 1 to U to turn the LED on
 * The SS bits give you four choices:
 *  00 says to let the U bit run the show
 *  01 says fast blink
 *  10 says medium blink
 *  11 says slow blink
 */

/* CPU hog, but didn't matter for bare metal */
static void
xxx_blink ( void )
{
	for ( ;; ) {
		axi_gpio_write ( 0xC4 );
		thr_delay ( 100 );
		axi_gpio_write ( 0x44 );
		thr_delay ( 5 );
	}
}


void
led_all_off ( void )
{
	axi_gpio_write ( 0x88 );
}

void
led_run (void )
{
	// Both LED blink slow and out of phase.
	// axi_gpio_write ( 0x26 );
	axi_gpio_write ( 0x22 );
	axi_gpio_write ( 0x11 );

	// Both LED blink fast and out of phase.
	// axi_gpio_write ( 0x15 );
}

#ifdef notdef
void
led_cmd ( int c )
{
	led_all_off ();
}
#endif

/* THE END */
