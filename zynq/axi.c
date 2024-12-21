/* Zynq-7000 driver for my custom IP experiments
 *
 * Tom Trebisky  12-2024
 *
 */

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#define BIT(x)  (1<<(x))

/* Asked for 7 registers, but am only using the first four
 */
struct axi_test {
		vu32	reg0;
		vu32	reg1;
		vu32	reg2;
		vu32	reg3;
};

#define AXI_BASE ((struct axi_test *) 0x43C00000)

void
axi_init ( void )
{
}

static int bl_state = 0;

static void
axi_blinker ( void )
{
		struct axi_test *ap = AXI_BASE;

		if ( bl_state ) {
			bl_state = 0;
			ap->reg1 = 0;
		} else {
			bl_state = 1;
			ap->reg1 = 0xf;
		}
}

void
axi_test ( void )
{
		struct axi_test *ap = AXI_BASE;
		int i;

		printf ( "AXI reg 0 = %08x\n", ap->reg0 );
		ap->reg0 = 0x8800;
		printf ( "AXI reg 0 = %08x\n", ap->reg0 );

		// reg1 is write for LEDS
		// printf ( "AXI reg 1 = %08x\n", ap->reg1 );

		printf ( "AXI reg 2 = %08x\n", ap->reg2 );
		printf ( "AXI reg 3 = %08x\n", ap->reg3 );
		thr_delay ( 2000 );
		printf ( "AXI reg 2 = %08x\n", ap->reg2 );
		printf ( "AXI reg 3 = %08x\n", ap->reg3 );

		/*
		printf ( "AXI reg 3 = %08x\n", ap->reg3 );
		ap->reg1 = 5;
		printf ( "AXI reg 3 = %d\n", ap->reg3 );
		ap->reg2 = 66;
		printf ( "AXI reg 3 = %d\n", ap->reg3 );
		*/

		/* The above gives:
		AXI reg 0 = abcd0012
		AXI reg 0 = abcd0012
		AXI reg 2 = 01d6a4a1
		AXI reg 3 = 00000000
		AXI reg 2 = 019c180c
		AXI reg 3 = 00000002
		*/

		/*
		for ( i=0; i<4; i++ ) {
			thr_delay ( 1000 );
			printf ( "AXI reg 3 = %08x\n", ap->reg3 );
		}
		*/

		(void) thr_new_repeat ( "axibl", axi_blinker, 0, 25, 0, 500 );
}

/* THE END */
