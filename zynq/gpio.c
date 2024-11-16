/* Zynq gpio driver.
 *
 * Tom Trebisky  1-4-2017 2-17-2024
 */

/* We have 4 32 bit GPIO devices.
 * The manual calls this 4 banks.
 * Banks 0 and 1 go to the MIO.
 * We have 54 MIO bits.
 *  32 on bank 0, 22 on bank 1
 *
 * As for EMIO on banks 2 and 3, these go to the PL
 *  the manual says we get 192 GPIO signals between the
 *  PS and PL via the EMIO interface
 * 64 Inputs, 128 outputs
 *   (64 true outputs and 64 output enables)
 *
 * Page 385-394 in the TRM discuss the GPIO
 * Registers are described on pages 1345-1373
 *
 * The output registers with the low/high names are
 *  called "maskable data registers"
 * Each controls 16 bits.
 * The upper 16 bits is a mask.
 *   A "0" allows the value to be updated
 *   A "1" protects the value
 * There are also 32 bit output registers if you
 *   don't mind changing all 32 bits.
 *
 * The power on state of direction registers is "input".
 *
 * Bank 0 has MIO 0-31
 * Bank 1 has MIO 32-53
 */

typedef volatile unsigned int vu32;
typedef unsigned int u32;

struct zynq_gpio {
        vu32    output0_low;
        vu32    output0_high;
        vu32    output1_low;
        vu32    output1_high;
        vu32    output2_low;
        vu32    output2_high;
        vu32    output3_low;
        vu32    output3_high;

        int     _pad1[8];

        vu32    output0;        /* 0x40 */
        vu32    output1;
        vu32    output2;
        vu32    output3;

        int     _pad2[4];

        vu32    input0;         /* 0x60 */
        vu32    input1;
        vu32    input2;
        vu32    input3;

        int     _pad3[101];

        vu32    dir0;           /* 0x204 */
        vu32    oe0;
        vu32    im0;
        vu32    ie0;		/* 0x210 */
        vu32    id0;
        vu32    is0;
        vu32    it0;
        vu32    ip0;		/* 0x220 */
        vu32    iany0;

        int     _pad4[7];

        vu32    dir1;           /* 0x244 */
        vu32    oe1;
	/* incomplete */

        /* .... */
};

/* On the EBAZ 4205 these are inputs, connected to buttons */
#define MIO_S2_BUTTON   20	// in bank 0
#define MIO_S3_BUTTON   32	// in bank 1

#define GPIO_BASE       ((struct zynq_gpio *) 0xe000a000)

void
gpio_init ( void )
{
        // struct zynq_gpio *gp = GPIO_BASE;

        /* Make this an input */
        // gp->dir0 &= ~BIT(MIO_S2_BUTTON);
}

void
gpio_config_output ( int bit )
{
        struct zynq_gpio *gp = GPIO_BASE;

	printf ( "Config output for %d\n", bit );
	printf ( "dir0 %08x\n", &gp->dir0 );
	printf ( "dir1 %08x\n", &gp->dir1 );

	if ( bit < 32 ) {
	    gp->dir0 |= 1 << bit;
	    gp->oe0 |= 1 << bit;
	} else {
	    gp->dir1 |= 1 << (bit-32);
	    gp->oe1 |= 1 << (bit-32);
	}
}

int
gpio_read ( int who )
{
        struct zynq_gpio *gp = GPIO_BASE;

	if ( who == 0 )
	    return gp->input0;
	else
	    return gp->input1;
}

void
gpio_write ( int bit, int val )
{
        struct zynq_gpio *gp = GPIO_BASE;
	u32 m;

	// printf ( "Write: bit %d = %d\n", bit, val );

	if ( bit < 16 ) {
	    m = 1 << (16+bit);
	    m = (~m) & 0xffff0000;
	    m |= val << (bit);
	    gp->output0_low = m;
	    return;
	}
	if ( bit < 32 ) {
	    m = 1 << (bit);
	    m = (~m) & 0xffff0000;
	    m |= val << (bit-16);
	    gp->output0_high = m;
	    return;
	}

	bit -= 32;

	if ( bit < 16 ) {
	    m = 1 << (16+bit);
	    m = (~m) & 0xffff0000;
	    m |= val << (bit);
	    gp->output1_low = m;
	    return;
	}
	if ( bit < 32 ) {
	    m = 1 << (bit);
	    m = (~m) & 0xffff0000;
	    m |= val << (bit-16);
	    gp->output1_high = m;
	    return;
	}
}

/* THE END */
