/* pru.c
 *
 * Tom Trebisky  Kyu project  8-17-2016
 *
 * driver for the PRU on the am3359
 *
 * The linux driver for this is a 2-part epoxy:
 *
 * am335x_pru_package/pru_sw/app_loader/include/pruss_intc_mapping.h
 * am335x_pru_package/pru_sw/app_loader/include/prussdrv.h
 * am335x_pru_package/pru_sw/app_loader/interface/Makefile
 * am335x_pru_package/pru_sw/app_loader/interface/__prussdrv.h
 * am335x_pru_package/pru_sw/app_loader/interface/prussdrv.c
 * AND
 * linux-3.8.13/kernel/drivers/uio/uio_pruss.c
 * linux-3.8.13/kernel/include/linux/platform_data/uio_pruss.h
 */
#include "omap_ints.h"

/* This is PRU INTC stuff */
/* Someday will move to pru_int.c (maybe) */

struct pru_intc {
	volatile unsigned long id;
	volatile unsigned long cr;
	long _pad0;
	volatile unsigned long hcr;
	volatile unsigned long ger;	/* 10 */
	long _pad1[2];
	volatile unsigned long gnlr;
	volatile unsigned long sisr;	/* 20 */
	volatile unsigned long sicr;
	volatile unsigned long eisr;
	volatile unsigned long eicr;
	long _pad2;
	volatile unsigned long hieisr;	/* 34 */
	volatile unsigned long hidisr;
	long _pad3[17];
	volatile unsigned long gpir;	/* 80 */
	long _pad4[95];
	volatile unsigned long srsr1;	/* 200 */
	volatile unsigned long srsr2;
	long _pad5[30];
	volatile unsigned long secr1;	/* 280 */
	volatile unsigned long secr2;
	long _pad6[30];
	volatile unsigned long esr1;	/* 300 */
	volatile unsigned long esr2;
	long _pad7[30];
	volatile unsigned long ecr1;	/* 380 */
	volatile unsigned long ecr2;
	long _pad8[30];
	volatile unsigned long cmr[16];		/* 400 */
	long _pad9[240];
	volatile unsigned long hmr[3];		/* 800 */
	long _pad9b[61];
	volatile unsigned long hipir[10];	/* 900 */
	long _pad10[246];
	volatile unsigned long sipr1;		/* d00 */
	volatile unsigned long sipr2;
	long _pad11[30];
	volatile unsigned long sitr1;		/* d80 */
	volatile unsigned long sitr2;
	long _pad12[478];
	volatile unsigned long hier;		/* 1500 */
};

/* The HIER register enables host interrupts - 10 of them */
/* The HIEISR and HEDISR registers set and clear bits
 *  in this hier register, counting from the right
 *  and given an index (0-9)
 */
#define PRU_INTC_BASE	0x4a320000

/* index numbers for registers like hieisr */
#define PRU0                    0
#define PRU1                    1
#define PRU_EVTOUT0             2
#define PRU_EVTOUT1             3
#define PRU_EVTOUT2             4
#define PRU_EVTOUT3             5
#define PRU_EVTOUT4             6
#define PRU_EVTOUT5             7
#define PRU_EVTOUT6             8
#define PRU_EVTOUT7             9

/* sys events */
#define PRU0_PRU1_INTERRUPT     17
#define PRU1_PRU0_INTERRUPT     18
#define PRU0_ARM_INTERRUPT      19
#define PRU1_ARM_INTERRUPT      20
#define ARM_PRU0_INTERRUPT      21
#define ARM_PRU1_INTERRUPT      22

/* makes things more readable */
#define CHANNEL0                0
#define CHANNEL1                1
#define CHANNEL2                2
#define CHANNEL3                3
#define CHANNEL4                4
#define CHANNEL5                5
#define CHANNEL6                6
#define CHANNEL7                7
#define CHANNEL8                8
#define CHANNEL9                9


static void
set_x4 ( volatile unsigned long *reg, int chan, int host )
{
	int who, index;

	who = chan / 4;
	index = chan % 4;

	reg[who] |= (host & 0xf) << (index * 8);
}

void
pru_intc_init ( void )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;
	unsigned int m1, m2;
	int i;

	pi->sipr1 = 0xffffffff;
	pi->sipr2 = 0xffffffff;

	for ( i=0; i<16; i++ )
		pi->cmr[0] = 0;

	set_x4 ( pi->cmr, PRU0_PRU1_INTERRUPT, CHANNEL1 );
	set_x4 ( pi->cmr, PRU1_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, PRU0_ARM_INTERRUPT, CHANNEL2 );
	set_x4 ( pi->cmr, PRU1_ARM_INTERRUPT, CHANNEL3 );
	set_x4 ( pi->cmr, ARM_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, ARM_PRU1_INTERRUPT, CHANNEL1 );

	for ( i=0; i<3; i++ )
		pi->hmr[0] = 0;

	/* we have 10 channels, we set 4 */
	set_x4 ( pi->hmr, CHANNEL0, PRU0 );
	set_x4 ( pi->hmr, CHANNEL1, PRU1 );
	set_x4 ( pi->hmr, CHANNEL2, PRU_EVTOUT0 );
	set_x4 ( pi->hmr, CHANNEL3, PRU_EVTOUT1 );

	pi->sitr1 = 0;
	pi->sitr2 = 0;

	m1 = 0;
	m2 = 0;

	/* events 0-31 are in the "1" register */
	m1 |= 1 << PRU0_PRU1_INTERRUPT;
	m1 |= 1 << PRU1_PRU0_INTERRUPT;
	m1 |= 1 << PRU0_ARM_INTERRUPT;
	m1 |= 1 << PRU1_ARM_INTERRUPT;
	m1 |= 1 << ARM_PRU0_INTERRUPT;
	m1 |= 1 << ARM_PRU1_INTERRUPT;

	pi->esr1 = m1;
	pi->esr2 = m2;
	pi->secr1 = m1;
	pi->secr2 = m2;

 	pi->hieisr = PRU0;
 	pi->hieisr = PRU1;
 	pi->hieisr = PRU_EVTOUT0;
 	pi->hieisr = PRU_EVTOUT1;

	/* global interrupt bit on */
	pi->ger = 1;

}

/* When we enable EVTOUT0, we get event 2 (EVTOUT0)
 * We also see srsr1 set to 0x00080000.
 * This is shifted over 19 zeros (PRU0_ARM_INTERRUPT)
 */
void
pru_intc_ack ( int event )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;

	printf ( "SRSR 0 = %08x\n", pi->srsr1 );
	pi->secr1 = 1 << PRU0_ARM_INTERRUPT;
	//pi->hidisr = event;	/* disables */
	// pi->hieisr = event;	/* reenables */
}

void
pru_intc_test ( void )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;
	int i;

	printf ( "INTC hier: %08x\n", pi->hier );
	for ( i=0; i<10; i++ ) {
		pi->hieisr = i;
		printf ( "INTC hier %d: %08x\n", i, pi->hier );
	}

	pi->hier = 0x3ff;
	printf ( "INTC hier: %08x\n", pi->hier );

}

void
pru_int_status ( void )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;
	int i;

	for ( i=0; i<10; i++ ) {
		printf ( "HIPIR %d = %08x\n", i, pi->hipir[i] );
	}
}


/* End of PRU INTC stuff */

/* -----------------------------------------*/
/* -----------------------------------------*/
/* -----------------------------------------*/

#define PRU0_FILE "pru0.bin"
#define PRU1_FILE "pru1.bin"

#define	PRU_SIZE	8192

#define PRU_BASE	0x4a300000	/* PRU memory and control */

#define PRM_PER_BASE	0x44e00c00	/* PRU reset bit is in here */

struct prm {
	volatile unsigned long rstctrl;		/* 00 */
	long _pad0;
	volatile unsigned long pwrstst;		/* 08 */
	volatile unsigned long pwrstctl;	/* 0c */
};

#ifdef notdef
#define AM33XX_DATARAM0_PHYS_BASE            0x4a300000
#define AM33XX_DATARAM1_PHYS_BASE            0x4a302000
#define AM33XX_INTC_PHYS_BASE                0x4a320000
#define AM33XX_PRU0CONTROL_PHYS_BASE         0x4a322000
#define AM33XX_PRU0DEBUG_PHYS_BASE           0x4a322400
#define AM33XX_PRU1CONTROL_PHYS_BASE         0x4a324000
#define AM33XX_PRU1DEBUG_PHYS_BASE           0x4a324400
#define AM33XX_PRU0IRAM_PHYS_BASE            0x4a334000
#define AM33XX_PRU1IRAM_PHYS_BASE            0x4a338000
#define AM33XX_PRUSS_SHAREDRAM_BASE          0x4a310000
#define AM33XX_PRUSS_CFG_BASE                0x4a326000
#define AM33XX_PRUSS_UART_BASE               0x4a328000
#define AM33XX_PRUSS_IEP_BASE                0x4a32e000
#define AM33XX_PRUSS_ECAP_BASE               0x4a330000
#define AM33XX_PRUSS_MIIRT_BASE              0x4a332000
#define AM33XX_PRUSS_MDIO_BASE               0x4a332400

#endif

#define PRU_DRAM0_BASE	0x4a300000	/* 8K data */
#define PRU_DRAM1_BASE	0x4a302000	/* 8K data */
#define PRU_SRAM_BASE	0x4a310000	/* 8K shared */
#define PRU_IRAM0_BASE	0x4a334000	/* 8K instructions */
#define PRU_IRAM1_BASE	0x4a338000	/* 8K instructions */

#define PRU0_DEBUG_BASE	0x4a322400
#define PRU1_DEBUG_BASE	0x4a324400

#define PRU_CTRL0_BASE	0x4a322000	/* PRU 0 - control registers */
#define PRU_CTRL1_BASE	0x4a324000	/* PRU 1 - control registers */

struct pru_ctrl {
	volatile unsigned long control;		/* 00 */
	volatile unsigned long status;		/* 04 */
	volatile unsigned long wakeup_en;	/* 08 */
	volatile unsigned long cycle;		/* 0c */
	volatile unsigned long stall;		/* 10 */
	long _pad0[3];
	volatile unsigned long ctbir0;		/* 20 */
	volatile unsigned long ctbir1;		/* 24 */
	volatile unsigned long ctppr0;		/* 28 */
	volatile unsigned long ctppr1;		/* 2c */
};

#define PRU_CTRL_BASE(x)	(struct pru_ctrl *) ((x) ? PRU_CTRL1_BASE : PRU_CTRL0_BASE)

#define CTRL_PC_SHIFT	16
#define CTRL_RUN	0x8000
#define CTRL_SINGLE	0x0100
#define CTRL_CCEN	0x0008
#define CTRL_SLEEP	0x0004
#define CTRL_ENA	0x0002
#define CTRL_NO_RESET	0x0001

struct pru_debug {
	volatile unsigned long gpreg[32];	/* 00 */
	volatile unsigned long ct_reg[32];	/* 00 */
};

#ifdef notdef
static char pru_buf[PRU_SIZE];

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
pru_reset_module ( void )
{
	struct prm *pp = (struct prm *) PRM_PER_BASE;

	/* pulse the reset bit */
	pp->rstctrl != 0x2;
	pp->rstctrl &= ~0x2;
}

void
pru_start ( int pru )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	p->control = CTRL_CCEN | CTRL_ENA | CTRL_NO_RESET;
}

/* This works by clearning everything AND
 *  most importantly, setting the last bit (reset) low.
 */
void
pru_reset ( int pru )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	p->control = 0;
}

/* As above, we clear the last bit (causing a reset),
 * but also set the enable bit.
 * Experiment needs to check, but this may do a restart.
 */
void
pru_restart ( int pru )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	p->control = CTRL_ENA;
}

/* clear the enable bit, but no reset */
void
pru_halt ( int pru )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	p->control = CTRL_CCEN | CTRL_NO_RESET;
}

unsigned long
pru_cstatus ( int pru )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	return p->control;
}

void
pru_isr ( int event )
{
	printf ( "PRU interrupt: %d\n", event );
	pru_intc_ack ( event );
}

/* This test is for Tom's blink firmware */

#define BL_HALT		0
#define BL_RUN		1
#define BL_COUNT	2

void
pru_test ( void )
{
	int *dram = (int *) PRU_DRAM0_BASE;
	int i;

	pru_intc_init ();

	irq_hookup ( IRQ_PRU_EV0, pru_isr, PRU_EVTOUT0 );
	irq_hookup ( IRQ_PRU_EV1, pru_isr, PRU_EVTOUT1 );

#ifdef notdef
	dram[0] = BL_RUN;	/* run forever */
	dram[1] = 0;		/* ignored (count) */
	dram[2] = 0x00f00000;	/* delay */
#endif

	dram[0] = BL_COUNT;
	dram[1] = 10;
	dram[2] = 0x00f00000;	/* delay */

	pru_start ( 0 );

/*
	for ( i=0; i< 12; i++ ) {
		thr_delay ( 1000 );
		pru_int_status ();
	}

	printf ( "INTC: %08x\n", &pi->srsr1 );
	printf ( "INTC: %08x\n", &pi->cmr[0]);
	printf ( "INTC: %08x\n", &pi->sitr1 );
	printf ( "INTC: %08x\n", &pi->hier );
*/
}

/* For use from shell */

void
pru_h ( void )
{
	pru_halt ( 0 );
}

void
pru_n ( void )
{
	pru_restart ( 0 );
}

void
pru_r ( void )
{
	pru_reset ( 0 );
}

void
pru_init ( void )
{
	int n;

	pru_mem_probe ();

	/* XXX - we could just be silent when these fail */
	n = tftp_fetch ( PRU0_FILE, PRU_IRAM0_BASE, PRU_SIZE );
	if ( n == 0 )
	    printf ( "Did not find %s\n", PRU0_FILE ); 
	else
	    printf ( "%d bytes loaded for PRU 0\n", n );

	n = tftp_fetch ( PRU1_FILE, PRU_IRAM1_BASE, PRU_SIZE );
	if ( n == 0 )
	    printf ( "Did not find %s\n", PRU1_FILE ); 
	else
	    printf ( "%d bytes loaded for PRU 1\n", n );

	printf ( "PRU 0 control reg: %08x\n", pru_cstatus ( 0 ) );
	printf ( "PRU 1 control reg: %08x\n", pru_cstatus ( 1 ) );

	pru_test ();
}

/* THE END */
