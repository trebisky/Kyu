/*
 * Copyright (C) 2020  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

/* i2c_hw.c
 *
 * Hardware level i2c driver for the Allwiner H3 on the Orange Pi boards.
 *
 * Tom Trebisky  Kyu project  7-27-2020
 *
 * Allwinner calls i2c the "TWI" interface,
 *  where TWI is "two wire interface".
 *  This is to avoid trademark issues, but it is essentially the same.
 *
 * The H3 chip has 4 i2c devices.
 * Only the first two are routed to the IO connector on the Orange Pi.
 * So, we only provide for those two i2c devices here.
 *
 * The TWI supports standard (100K) and fast (400K) exchanges.
 * It also supports 10 bit addresses.
 */

#include "kyu.h"
#include "thread.h"
#include "h3_ints.h"

#define TWI0_BASE      0x01c2ac00
#define TWI1_BASE      0x01c2b000
#define TWI2_BASE      0x01c2b400
#define TWI3_BASE      0x01f02400	/* called R_TWI */

struct twi_dev {
        volatile unsigned long addr;           /* 00 */
        volatile unsigned long xaddr;          /* 04 */
        volatile unsigned long data;           /* 08 */
        volatile unsigned long ctrl;           /* 0C */
        volatile unsigned long status;         /* 10 */
        volatile unsigned long ccr;            /* 14 */
        volatile unsigned long reset;          /* 18 */
        volatile unsigned long efr;            /* 1c */
        volatile unsigned long lcr;            /* 20 */
        volatile unsigned long dvfs;           /* 24 */
};

#define	CTRL_INT_ENA	0x80
#define	CTRL_BUS_ENA	0x40	/* set this in master mode */
#define	CTRL_M_START	0x20	/* Master mode start */
#define	CTRL_M_STOP	0x10	/* Master mode stop */

#define	CTRL_INT_FLAG	0x08
#define	CTRL_A_ACK	0x04	/* Assert ACK */

/*
 * TWI0_sck	PA11	fn 2
 * TWI0_sda	PA12	fn 2
 * TWI1_sck	PA18	fn 3
 * TWI1_sck	PA19	fn 3
 * TWI2_sck	PE12	fn 3
 * TWI2_sck	PE13	fn 3
 * TWI3_sck	PL0	fn 2	-- S_TWI
 * TWI3_sck	PL1	fn 2	-- S_TWI
 */

#define NUM_TWI 2

static struct twi_softc {
        struct twi_dev *base;
        int irq;
        char *tbufp;
        char *rbufp;
        int tcount;
        int rcount;
        struct sem *sem;
        int status;
} twi_soft[NUM_TWI];

/* Despite contradictory statements in the TRM, you ack (and clear)
 * interrupts by writing a one to the relevant bit in the irqstatus register,
 * NOT the irqstatus_raw register.
 */
static void
twi_isr ( int arg )
{
        struct twi_softc *sc = (struct twi_softc *) arg;
        struct twi_dev *base = sc->base;

        int stat;
        int ch;

        printf ( "i2c_isr (stat): %08x\n", base->status );

#ifdef notyet
        if ( stat & IRQ_XRDY ) {
            base->data = sc->tbufp ? *sc->tbufp++ : 0xff;
            base->irqstatus = IRQ_XRDY;
        }

        if ( stat & IRQ_RRDY ) {
            ch = base->data;
            if ( sc->rbufp ) {
                *sc->rbufp++ = ch;
                sc->rcount++;
            }
            base->irqstatus = IRQ_RRDY;
        }

        /* Near as I can tell, this indicates the transfer is done */
        if ( stat & IRQ_ARDY ) {
            base->irqstatus = IRQ_ARDY;
            sem_unblock ( sc->sem );
        }

        if ( stat & IRQ_NACK ) {
            /* will this speed things up ?? */
            base->con = CON_ENABLE | CON_MASTER | CON_STOP;
            base->irqstatus = IRQ_NACK;
            sc->status = 1;
            // sem_unblock ( sc->sem );
        }
#endif
}

static void
twi_devinit ( int devnum, struct twi_dev *base, int irq )
{
        struct twi_softc *sc = &twi_soft[devnum];

        sc->base = base;
        sc->irq = irq;
        sc->rbufp = 0;
        sc->tbufp = 0;
        sc->sem = sem_signal_new ( SEM_PRIO );

#ifdef notyet
        twi_reset_i ( base );

        base->psc = PSC_2;
        base->scll = 120;
        base->sclh = 120;
        base->buf = BUF_RX_CLR | BUF_TX_CLR;

        base->irqenable_set = IRQ_ENABLES;
#endif

        irq_hookup ( irq, twi_isr, sc );

        base->ctrl = CTRL_INT_ENA;
}

static void
twi0_setup ( void )
{
        setup_twi0_mux ();
        twi_devinit ( 0, (struct twi_dev *) TWI0_BASE, IRQ_TWI0 );
}

/* Uses mode 2 to put these on P9-17 and P9-18
 *  P9-17 is SCL
 *  P9-18 is SDA
 */
static void
twi1_setup ( void )
{
        setup_twi1_mux ();
        twi_devinit ( 1, (struct twi_dev *) TWI1_BASE, IRQ_TWI1 );
}

/* Called during system startup.
 *  (from board.c)
 * XXX - note that we are in effect "claiming" these pins
 *  for i2c, maybe we should leave them for gpio unless the
 *  caller actually calls twi_hw_init ??
 */
void
i2c_init ( void )
{
        twi0_setup ();
        twi1_setup ();
}

/* Called from i2c.c */
void *
i2c_hw_init ( int devnum )
{
        if ( devnum < 0 || devnum > NUM_TWI )
            return (void *) 0;
        return (void *) &twi_soft[devnum];
}

static void
twi_show ( void )
{
	int devnum = 0;
	struct twi_dev *base;

	base = twi_soft[devnum].base;

	printf ( "Device  : %d\n", devnum );
	printf ( "Base    : %08x\n", base );
	printf ( "Status  : %08x\n", base->status );
}

void
twi_test ( void )
{
	twi_show ();
}

/* THE END */
