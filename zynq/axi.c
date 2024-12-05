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

void
axi_test ( void )
{
		struct axi_test *ap = AXI_BASE;

		printf ( "AXI reg 0 = %08x\n", ap->reg0 );
		ap->reg0 = 0x8800;
		// printf ( "AXI reg 0 = %08x\n", ap->reg0 );

		printf ( "AXI reg 3 = %08x\n", ap->reg3 );
		ap->reg1 = 5;
		printf ( "AXI reg 3 = %d\n", ap->reg3 );
		ap->reg2 = 66;
		printf ( "AXI reg 3 = %d\n", ap->reg3 );
}

/* THE END */
