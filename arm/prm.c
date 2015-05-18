/* prm.c
 *
 * Tom Trebisky  Kyu project  5-11-2015
 *
 * Simple driver for the PRM on the am3359
 */

/* This is actually just part of the PRCM,
 *  namely PRM_DEVICE.
 * All we are doing for now is providing a way for
 *  softare to reset the chip.
 */
#define PRM_BASE      0x44E00F00

struct prm {
	volatile unsigned long rstctl;
	volatile unsigned long rsttime;
	volatile unsigned long rstst;
	volatile unsigned long sram_count;
	volatile unsigned long ldo_sram_core_setup;
	volatile unsigned long ldo_sram_core_ctrl;
	volatile unsigned long ldo_sram_mpu_setup;
	volatile unsigned long ldo_sram_mpu_ctrl;
};

/* The difference is unclear.
 * The TRM says that if you do a cold reset, you
 *  "must ensure the SDRAM is properly put in sef-refresh
 *   mode before applying this reset"
 */
#define	WARM_RESET	0x01
#define	COLD_RESET	0x02

void
reset_cpu ( void )
{
	struct prm *base = (struct prm *) PRM_BASE;

	base->rstctl = WARM_RESET;
}

/* THE END */
