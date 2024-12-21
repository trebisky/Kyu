/* Code to initialize FPGA fabric clocks
 *
 * pl_clocks.c
 * Tom Trebisky  12-10-2024
 *
 * Copied from /u1/Projects/Zynq/Git_ebaz/fabric/clocks.c
 * Tom Trebisky  2-28-2023
 *
 * See chapter 25 of the TRM
 *  in particular section 25.7
 *
 * Take a look in U-boot.git
 *  drivers/clk/clk_zynq.c
 *  arch/arm/mach-zynq/slcr.c
 */

typedef volatile unsigned int vu32;
typedef unsigned int u32;

#include "hardware.h"

#define BIT(x)  (1<<(x))

/* Before we do anything, we see this:
 * FPGA0 clock ctrl: 00200400
 * FPGA1 clock ctrl: 00500800
 */

#define	SRC_IO		0x00
#define	SRC_IOx		0x10
#define	SRC_ARM		0x20
#define	SRC_DDR		0x30

#define DIV0_SHIFT	8
#define DIV1_SHIFT	20

void fabric_50 ( void );

/* This makes changes to the first fabric clock only.
 * XXX - add code for the other 3 clocks.
 *
 * ***** These seem to be limited to 250 Mhz !!
 */
static void
fabric_set ( int src, int div0, int div1 )
{
	struct slcr_regs *sp;
	u32 val;

	sp = (struct slcr_regs *) ZYNQ_SYS_CTRL_BASEADDR;

	val = div1 << DIV1_SHIFT | div0 << DIV0_SHIFT + src;

	slcr_unlock();
	sp->fpga0_clk_ctrl = val;
	slcr_lock();
}

static void
reg_show ( char *msg, u32 val )
{
	printf ( "%s %X\n", msg, val );
}

static void
fabric_show ( char *msg, u32 val )
{
	int div1, div0;
	int div;
	int rate;
	int source;

	// reg_show ( msg, val );
	printf ( "%s clock ctrl:  %08x\n", msg, val );
	div1 = (val >> DIV1_SHIFT) & 0x3f;
	div0 = (val >> DIV0_SHIFT) & 0x3f;
	div = div1 * div0;
	rate = 1000 / div;
	printf ( "%s clock rate:  %d Mhz (div1 = %d, div0 = %d)\n", msg, rate, div1, div0 );
	source = (val >> 4) & 0x3;
	if ( source != 0 )
		printf ( "** unexpected source: %x\n", source );

}

void
fabric_test ( void )
{
	struct slcr_regs *sp;

	sp = (struct slcr_regs *) ZYNQ_SYS_CTRL_BASEADDR;

	reg_show ( "PLL -- arm:", sp->arm_pll_ctrl );
	reg_show ( "PLL -- ddr:", sp->ddr_pll_ctrl );
	reg_show ( "PLL -- io :", sp->io_pll_ctrl );
	/* These yield:
	 * PLL -- arm: 00028008
	 * PLL -- ddr: 00020008
	 * PLL -- io : 0001E008
	 *  We have a 33.33 Mhz crystal, so using the multipliers we get:
	 *  33.3 * 0x28 (40) = 1332 Mhz (cpu runs at half this, i.e. 666 Mhz
	 *  33.3 * 0x20 (32) = 1066 Mhz
	 *  33.3 * 0x18 (30) = 1000 Mhz
	 *
	 * But the fabric clocks come from the IO clock, which is 1000 Mhz
	 */

	// reg_show ( "FPGA0 clock ctrl:", sp->fpga0_clk_ctrl );
	// reg_show ( "FPGA1 clock ctrl:", sp->fpga1_clk_ctrl );
	// reg_show ( "FPGA2 clock ctrl:", sp->fpga2_clk_ctrl );
	// reg_show ( "FPGA3 clock ctrl:", sp->fpga3_clk_ctrl );

	fabric_show ( "FPGA0", sp->fpga0_clk_ctrl );
	fabric_show ( "FPGA1", sp->fpga1_clk_ctrl );
	fabric_show ( "FPGA2", sp->fpga2_clk_ctrl );
	fabric_show ( "FPGA3", sp->fpga3_clk_ctrl );

	/* These yield:
		FPGA0 clock ctrl:  00101400
		FPGA0 clock rate:  50 Mhz (div1 = 1, div0 = 20)
		FPGA1 clock ctrl:  00100a00
		FPGA1 clock rate:  100 Mhz (div1 = 1, div0 = 10)
		FPGA2 clock ctrl:  00101e00
		FPGA2 clock rate:  33 Mhz (div1 = 1, div0 = 30)
		FPGA3 clock ctrl:  00101400
		FPGA3 clock rate:  50 Mhz (div1 = 1, div0 = 20)
	 */

	/* In other words we come out of reset with FCLK-0 running
	 * at 50 Mhz.  It divides the 1000 Mhz IO clock by 20.
	 * Both div1 and div2 are 6 bit divisors.
	 * What happens if a divisor is 0?  Who knows.
	 */

	// printf ( "New fabric clock set to 50 Mhz\n" );
	// fabric_norm ();
	// fabric_50 ();
	// reg_show ( "FPGA0 clock ctrl:", sp->fpga0_clk_ctrl );
}

/* 50 Mhz */
void
fabric_50 ( void )
{
	fabric_set ( SRC_IO, 4, 5 );
}

/* 62.5 Mhz */
void
fabric_slower ( void )
{
	fabric_set ( SRC_IO, 4, 4 );
}

/* 250 Mhz */
void
fabric_faster ( void )
{
	fabric_set ( SRC_IO, 4, 1 );
}

/* 125 Mhz */
void
fabric_norm ( void )
{
	fabric_set ( SRC_IO, 4, 2 );
}

/* THE END */
