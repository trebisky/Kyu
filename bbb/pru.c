/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
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
	volatile unsigned long srsr[2];	/* 200 */
	long _pad5[30];
	volatile unsigned long secr[2];	/* 280 */
	long _pad6[30];
	volatile unsigned long esr[2];	/* 300 */
	long _pad7[30];
	volatile unsigned long ecr[2];	/* 380 */
	long _pad8[30];
	volatile unsigned long cmr[16];		/* 400 */
	long _pad9[240];
	volatile unsigned long hmr[3];		/* 800 */
	long _pad9b[61];
	volatile unsigned long hipir[10];	/* 900 */
	long _pad10[246];
	volatile unsigned long sipr[2];		/* d00 */
	long _pad11[30];
	volatile unsigned long sitr[2];		/* d80 */
	long _pad12[478];
	volatile unsigned long hier;		/* 1500 */
};

/* The HIER register enables host interrupts - 10 of them */
/* The HIEISR and HEDISR registers set and clear bits
 *  in this hier register, counting from the right
 *  and given an index (0-9)
 */
#define PRU_INTC_BASE	0x4a320000

/* index numbers for host events */
/* use these in hieisr and such */
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

/* System events */
/* XXX - skip 16 */
#define PRU0_PRU1_INTERRUPT     17	/* PRU event 1 */
#define PRU1_PRU0_INTERRUPT     18	/* PRU event 2 */
#define PRU0_ARM_INTERRUPT      19	/* PRU event 3 */
#define PRU1_ARM_INTERRUPT      20	/* PRU event 4 */
#define ARM_PRU0_INTERRUPT      21	/* PRU event 5 */
#define ARM_PRU1_INTERRUPT      22	/* PRU event 6 */

/* For a PRU to generate a system event, it writes to R31.
 * It sets a strobe bit (0x20) plus a value 0-15.
 * For the current setup:
 *   value = 0 is undefined (system event 16)
 *   value = 1 is from PRU0 to PRU1 (event 17)
 *   value = 2 is from PRU1 to PRU0 (event 18)
 *   value = 3 is from PRU0 to ARM (event 19)
 *   value = 4 is from PRU1 to ARM (event 20)
 *
 *   Note that PRU0 can lie and write the value 4 and
 *   generate an interrupt that looks like it came from PRU1.
 *   These are just conventions.
 *
 *   How does the ARM generate system events 21 and 22 ??
 */

static void
set_x4 ( volatile unsigned long *reg, int chan, int host )
{
	int who, index;

	who = chan / 4;
	index = chan % 4;

	reg[who] |= (host & 0xf) << (index * 8);
}

/* Many registers that hold bits for the 64 system events
 *  are of the sort that a write of zero has no effect.
 *  esr, ecr, secr
 */
void
set_64 ( volatile unsigned long *reg, int index )
{
	unsigned long mask = 1 << (index & 0x1f );
	
	if ( index > 31 )
	    reg[1] = mask;
	else
	    reg[0] = mask;
}

#ifdef TI_SETUP
/* This is only one of many possible setups.
 * It uses only 4 channels
 * As near as I can tell, this is the setup used in the TI driver
 *  for the PRU supplied by Texas Instuments.
 *  Channel 0 sends interrupts to PRU0 (from PRU1 or the ARM)
 *  Channel 1 sends interrupts to PRU1 (from PRU0 or the ARM)
 *  Channel 2 sends interrupts from PRU0 to the ARM on EVTOUT0
 *  Channel 3 sends interrupts from PRU1 to the ARM on EVTOUT1
 */

void
pru_intc_init_ti ( void )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;
	unsigned int m1, m2;
	int i;

	pi->sipr[0] = 0xffffffff;
	pi->sipr[1] = 0xffffffff;

	pi->sitr[0] = 0;
	pi->sitr[1] = 0;

	/* Channel map, maps "system event" to the 10 channels */
	for ( i=0; i<16; i++ )
	    pi->cmr[0] = 0;

	set_x4 ( pi->cmr, PRU0_PRU1_INTERRUPT, CHANNEL1 );
	set_x4 ( pi->cmr, PRU1_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, PRU0_ARM_INTERRUPT, CHANNEL2 );
	set_x4 ( pi->cmr, PRU1_ARM_INTERRUPT, CHANNEL3 );
	set_x4 ( pi->cmr, ARM_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, ARM_PRU1_INTERRUPT, CHANNEL1 );

	/* Host map, we have 10 channels and 10 host assignments.
         *  we use only 4.
         */
	for ( i=0; i<3; i++ )
	    pi->hmr[0] = 0;

	/* we have 10 channels, we set 4 */
	set_x4 ( pi->hmr, CHANNEL0, PRU0 );
	set_x4 ( pi->hmr, CHANNEL1, PRU1 );
	set_x4 ( pi->hmr, CHANNEL2, PRU_EVTOUT0 );
	set_x4 ( pi->hmr, CHANNEL3, PRU_EVTOUT1 );

	set_64 ( pi->esr, PRU0_PRU1_INTERRUPT );
	set_64 ( pi->esr, PRU1_PRU0_INTERRUPT );
	set_64 ( pi->esr, PRU0_ARM_INTERRUPT );
	set_64 ( pi->esr, PRU1_ARM_INTERRUPT );
	set_64 ( pi->esr, ARM_PRU0_INTERRUPT );
	set_64 ( pi->esr, ARM_PRU1_INTERRUPT );

	set_64 ( pi->secr, PRU0_PRU1_INTERRUPT );
	set_64 ( pi->secr, PRU1_PRU0_INTERRUPT );
	set_64 ( pi->secr, PRU0_ARM_INTERRUPT );
	set_64 ( pi->secr, PRU1_ARM_INTERRUPT );
	set_64 ( pi->secr, ARM_PRU0_INTERRUPT );
	set_64 ( pi->secr, ARM_PRU1_INTERRUPT );

 	pi->hieisr = PRU0;
 	pi->hieisr = PRU1;
 	pi->hieisr = PRU_EVTOUT0;
 	pi->hieisr = PRU_EVTOUT1;

	/* global interrupt bit on */
	pi->ger = 1;
}
#endif

/* another way to do things */
#define ARM_EVT0      16	/* PRU event 0 */
#define ARM_EVT1      17	/* PRU event 1 */
#define ARM_EVT2      18	/* PRU event 2 */
#define ARM_EVT3      19	/* PRU event 3 */
#define ARM_EVT4      20	/* PRU event 4 */
#define ARM_EVT5      21	/* PRU event 5 */
#define ARM_EVT6      22	/* PRU event 6 */
#define ARM_EVT7      23	/* PRU event 7 */

#define ARM_PRU0_INT      24	/* PRU event 8 */
#define ARM_PRU1_INT      25	/* PRU event 9 */

#define PRU0_PRU1_INT     26	/* PRU event 10 */
#define PRU1_PRU0_INT     27	/* PRU event 11 */

void
pru_intc_init ( void )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;
	unsigned int m1, m2;
	int i;

	/* active high */
	pi->sipr[0] = 0xffffffff;
	pi->sipr[1] = 0xffffffff;

	/* pulse (also called level), not edge */
	pi->sitr[0] = 0;
	pi->sitr[1] = 0;

	/* Channel map, maps "system event" to the 10 channels */
	for ( i=0; i<16; i++ )
	    pi->cmr[0] = 0;

	set_x4 ( pi->cmr, PRU0_PRU1_INTERRUPT, CHANNEL1 );
	set_x4 ( pi->cmr, PRU1_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, ARM_PRU0_INTERRUPT, CHANNEL0 );
	set_x4 ( pi->cmr, ARM_PRU1_INTERRUPT, CHANNEL1 );

	set_x4 ( pi->cmr, PRU0_ARM_INTERRUPT, CHANNEL2 );
	set_x4 ( pi->cmr, PRU1_ARM_INTERRUPT, CHANNEL3 );

#ifdef notyet
	set_x4 ( pi->cmr, ARM_EVT0, CHANNEL2 );
	set_x4 ( pi->cmr, ARM_EVT1, CHANNEL3 );
	set_x4 ( pi->cmr, ARM_EVT2, CHANNEL4 );
	set_x4 ( pi->cmr, ARM_EVT3, CHANNEL5 );
	set_x4 ( pi->cmr, ARM_EVT4, CHANNEL6 );
	set_x4 ( pi->cmr, ARM_EVT5, CHANNEL7 );
#endif

	/* Host map, we have 10 channels and 10 host assignments.
         *  we use only 4.
         */
	for ( i=0; i<3; i++ )
	    pi->hmr[0] = 0;

	set_x4 ( pi->hmr, CHANNEL0, PRU0 );
	set_x4 ( pi->hmr, CHANNEL1, PRU1 );
	set_x4 ( pi->hmr, CHANNEL2, PRU_EVTOUT0 );
	set_x4 ( pi->hmr, CHANNEL3, PRU_EVTOUT1 );

	/* Add these for the heck of it */
	set_x4 ( pi->hmr, CHANNEL4, PRU_EVTOUT2 );
	set_x4 ( pi->hmr, CHANNEL5, PRU_EVTOUT3 );
	set_x4 ( pi->hmr, CHANNEL6, PRU_EVTOUT4 );
	set_x4 ( pi->hmr, CHANNEL7, PRU_EVTOUT5 );
	set_x4 ( pi->hmr, CHANNEL8, PRU_EVTOUT6 );
	set_x4 ( pi->hmr, CHANNEL9, PRU_EVTOUT7 );

	set_64 ( pi->esr, PRU0_PRU1_INTERRUPT );
	set_64 ( pi->esr, PRU1_PRU0_INTERRUPT );
	set_64 ( pi->esr, PRU0_ARM_INTERRUPT );
	set_64 ( pi->esr, PRU1_ARM_INTERRUPT );
	set_64 ( pi->esr, ARM_PRU0_INTERRUPT );
	set_64 ( pi->esr, ARM_PRU1_INTERRUPT );

	set_64 ( pi->secr, PRU0_PRU1_INTERRUPT );
	set_64 ( pi->secr, PRU1_PRU0_INTERRUPT );
	set_64 ( pi->secr, PRU0_ARM_INTERRUPT );
	set_64 ( pi->secr, PRU1_ARM_INTERRUPT );
	set_64 ( pi->secr, ARM_PRU0_INTERRUPT );
	set_64 ( pi->secr, ARM_PRU1_INTERRUPT );

 	pi->hieisr = PRU0;
 	pi->hieisr = PRU1;
 	pi->hieisr = PRU_EVTOUT0;
 	pi->hieisr = PRU_EVTOUT1;

	/* Enable these too */
 	pi->hieisr = PRU_EVTOUT2;
 	pi->hieisr = PRU_EVTOUT3;
 	pi->hieisr = PRU_EVTOUT4;
 	pi->hieisr = PRU_EVTOUT5;
 	pi->hieisr = PRU_EVTOUT6;
 	pi->hieisr = PRU_EVTOUT7;

	/* global interrupt bit on */
	pi->ger = 1;
}

/* Write to the SRSR register to send an "interrupt"
 * to the PRU.
 * Reading this register gives raw status.
 * The event index is 0-63
 */
void
post_interrupt ( int event )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;

	set_64 ( pi->srsr, event );
}

/* When we enable EVTOUT0, we get event 2 (EVTOUT0)
 * We also see srsr1 set to 0x00080000.
 * This is shifted over 19 zeros (PRU0_ARM_INTERRUPT)
 */
void
pru_intc_ack ( int sys_event )
{
	struct pru_intc  * pi = (struct pru_intc *) PRU_INTC_BASE;

	/* Clear the interrupt */
	set_64 ( pi->secr, sys_event );

	/* As near as I can tell, this is the same as: */
	// pi->sicr = sys_event;

	//pi->hidisr = event;	/* disables */
	// pi->hieisr = event;	/* reenables */
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
#endif

/* XXX - belongs in prcm.c */
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

/* untested */
void
pru_reset_pc ( int pru, unsigned int pc )
{
	struct pru_ctrl *p = PRU_CTRL_BASE(pru);

	p->control = pc << CTRL_PC_SHIFT;
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
pru_isr ( int sys_event )
{
	printf ( "PRU interrupt: %d\n", sys_event );
	pru_intc_ack ( sys_event );
}

/* This test is for Tom's blink firmware */

#define BL_HALT		0
#define BL_RUN		1
#define BL_COUNT	2

void
pru_test_blink ( void )
{
	int *dram = (int *) PRU_DRAM0_BASE;
	int i;

	pru_intc_init ();

	irq_hookup ( IRQ_PRU_EV0, pru_isr, PRU0_ARM_INTERRUPT );
	irq_hookup ( IRQ_PRU_EV1, pru_isr, PRU1_ARM_INTERRUPT );

#ifdef notdef
	dram[0] = BL_RUN;	/* run forever */
	dram[1] = 0;		/* ignored (count) */
	dram[2] = 0x00f00000;	/* delay */
#endif

	dram[0] = BL_COUNT;
	dram[1] = 10;
	dram[2] = 0x00f00000;	/* delay */

	pru_start ( 0 );
}

/* For use with the newer eblink2 */
void
pru_tester ( void *xxx )
{
	int *dram = (int *) PRU_DRAM0_BASE;
	int i;

	pru_intc_init ();

	irq_hookup ( IRQ_PRU_EV0, pru_isr, PRU0_ARM_INTERRUPT );
	irq_hookup ( IRQ_PRU_EV1, pru_isr, PRU1_ARM_INTERRUPT );

#ifdef notdef
	dram[0] = BL_RUN;	/* run forever */
	dram[1] = 0;		/* ignored (count) */
	dram[2] = 0x00f00000;	/* delay */
#endif

	/* First 5 pulses */
	dram[0] = BL_COUNT;
	dram[1] = 5;
	dram[2] = 0x00c00000;	/* delay */
	dram[3] = 0x00c00000;	/* delay */

	pru_start ( 0 );

	thr_delay ( 2000 );

	/* Then each time we interrupt, get 2 pulses from PRU firmware */
	for ( ;; ) {
	    // printf ( "Go\n" );
	    post_interrupt ( ARM_PRU0_INTERRUPT );
	    thr_delay ( 1000 );
	}
}

void
pru_test ( void )
{
	// (void) thr_new ( "pru_tester", pru_tester, (void *) 0, 13, 0 );
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

	// pru_mem_probe ();

	/* XXX - we could just be silent when these fail */
	if ( net_running() ) {
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
	}

	printf ( "PRU 0 control reg: %08x\n", pru_cstatus ( 0 ) );
	printf ( "PRU 1 control reg: %08x\n", pru_cstatus ( 1 ) );

	pru_test ();
}

/* THE END */
