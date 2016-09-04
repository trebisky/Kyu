/* pru.c
 *
 * Tom Trebisky  Kyu project  8-17-2016
 *
 * driver for the PRU on the am3359
 */

#define PRU1_FILE "pru1.bin"
#define PRU2_FILE "pru2.bin"

#define	PRU_SIZE	8192

#define PRU_BASE	0x4a300000	/* PRU memory and control */

#define PRM_PER_BASE	0x44e00c00	/* PRU reset bit is in here */

struct prm {
	volatile unsigned long rstctrl;		/* 00 */
	long _pad0;
	volatile unsigned long pwrstst;		/* 08 */
	volatile unsigned long pwrstctl;	/* 0c */
};

#define PRU_DRAM0_BASE	0x4a300000	/* 8K data */
#define PRU_DRAM1_BASE	0x4a302000	/* 8K data */
#define PRU_SRAM_BASE	0x4a310000	/* 8K shared */
#define PRU_IRAM0_BASE	0x4a334000	/* 8K instructions */
#define PRU_IRAM1_BASE	0x4a338000	/* 8K instructions */

static char pru_buf[PRU_SIZE];

#ifdef notdef
/* XXX - Hack for now */
#define PRU_IRAM0_BASE	pru_buf
#define PRU_IRAM1_BASE	pru_buf
#endif

static void
probeit ( unsigned long addr )
{
	int s;

	s = data_abort_probe ( addr );
	if ( s )
		printf ( "Probe at %08x, Fails\n", addr );
	else
		printf ( "Probe at %08x, ok\n", addr );
}

void
pru_mem_probe ( void )
{
	probeit ( PRU_IRAM0_BASE );
	probeit ( PRU_IRAM1_BASE );
	probeit ( PRU_DRAM0_BASE );
	probeit ( PRU_DRAM1_BASE );
	probeit ( PRU_SRAM_BASE );
}

void
pru_init ( void )
{
	int n;
	char *p;

#ifdef notdef
	printf ( "Probing PRU addresses\n" );
	/* Probe to ensure these addresses are valid */
	/* And they are not, both yield data aborts!! */
	/* same accessed as bytes or longs */
	/* AND they yield data aborts with my old boards
	 * that use the 2013 U-Boot which does NOT enable
	 * the data cache, (and does NOT enable the MMU)
	 * as well as the 2015 U-Boot that does */
	p = (char *) PRU_IRAM0_BASE;
	*p = 0;
	p = (char *) PRU_IRAM1_BASE;
	*p = 0;
	printf ( "Probing done\n" );
#endif
	pru_mem_probe ();

	/* XXX - we could just be silent when these fail */
	n = tftp_fetch ( PRU1_FILE, PRU_IRAM0_BASE, PRU_SIZE );
	if ( n == 0 )
	    printf ( "Did not find %s\n", PRU1_FILE ); 

	n = tftp_fetch ( PRU1_FILE, PRU_IRAM1_BASE, PRU_SIZE );
	if ( n == 0 )
	    printf ( "Did not find %s\n", PRU2_FILE ); 
}

void
pru_reset ( void )
{
	struct prm *pp = (struct prm *) PRM_PER_BASE;

	/* pulse the reset bit */
	pp->rstctrl != 0x2;
	pp->rstctrl &= ~0x2;
}

/* THE END */
