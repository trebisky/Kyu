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
 *
 * Here is a quick runthrough of relevant sections in the H3 manual,
 * along with code needed in other modules to support the TWI.
 *
 * section 8 - TWI section
 * gpio.c - pinmux settings
 * gic, via calls here to irq_hookup()
 *  interrupt numbers in h3_ints.h
 * test hooks in test_io.c
 *
 * Valuable information in Armbian/drivers/i2c/busses/i2c-sunxi.c
 *
 * Coding began 7-27-2020
 * A typo in the ccu.c file caused the release of reset for this
 *  unit to be misdirected.  Once that was fixed, we got results.
 * Another typo with a wrong interrupt number caused trouble.
 */

#include "kyu.h"
#include "thread.h"
#include "h3_ints.h"

#define TWI0_BASE      0x01c2ac00
#define TWI1_BASE      0x01c2b000
#define TWI2_BASE      0x01c2b400
#define TWI3_BASE      0x01f02400	/* called R_TWI */

struct twi_dev {
        volatile unsigned int addr;           /* 00 */
        volatile unsigned int xaddr;          /* 04 */
        volatile unsigned int data;           /* 08 */
        volatile unsigned int ctrl;           /* 0C */
        volatile unsigned int status;         /* 10 */
        volatile unsigned int ccr;            /* 14 */
        volatile unsigned int reset;          /* 18 */
        volatile unsigned int efr;            /* 1c */
        volatile unsigned int lcr;            /* 20 */
        volatile unsigned int dvfs;           /* 24 */
};

/* The addr and xaddr registers would only be used if we put ourselves into
 * slave mode, which we never do.
 */

#define	CTRL_INT_ENA	0x80
#define	CTRL_BUS_ENA	0x40	/* set this in master mode */
#define	CTRL_M_START	0x20	/* Master mode start */
#define	CTRL_M_STOP	0x10	/* Master mode stop */

#define	CTRL_INT_FLAG	0x08
#define	CTRL_A_ACK	0x04	/* Assert ACK */

#define	CCR_CLOCK_M	0x78	/* 4 bits (0xf<<3) */
#define	CCR_CLOCK_N	0x07	/* 3 bits */

#define LCR_IDLE	0x3a

/* Pins ---
 * TWI0_sck	PA11	fn 2
 * TWI0_sda	PA12	fn 2
 * TWI1_sck	PA18	fn 3
 * TWI1_sck	PA19	fn 3
 * TWI2_sck	PE12	fn 3
 * TWI2_sck	PE13	fn 3
 * TWI3_sck	PL0	fn 2	-- S_TWI
 * TWI3_sck	PL1	fn 2	-- S_TWI
 */

// #define NUM_TWI 2
#define NUM_TWI 3

enum twi_status { TWI_IDLE, TWI_RUN, TWI_DONE };

static struct twi_softc {
        struct twi_dev *base;
	int dev;
        int irq;
        char *tbufp;
        char *rbufp;
        int tcount;
        int rcount;
	int addr;
        struct sem *sem;
        enum twi_status status;
} twi_soft[NUM_TWI];

static void twi_stop ( struct twi_dev * );
static void twi_start ( struct twi_dev * );

/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */

static void
twi_clear_irq ( struct twi_dev *base )
{
	unsigned int creg;
	
	creg = base->ctrl;
	creg &= ~( CTRL_M_START	| CTRL_M_STOP );
	creg |= CTRL_INT_FLAG;
	base->ctrl = creg;

	/* As per linux driver */
	creg = base->ctrl;
	creg = base->ctrl;
}

/* I see 0x48 after sending an address with nothing on the bus.
 */

static void
twi_engine ( struct twi_softc *sc )
{
        struct twi_dev *base = sc->base;
	int state;

	state = base->status;
        printf ( " twi_engine  (stat): %08x\n", state );
	state = base->status;
        printf ( " twi_engine+ (stat): %08x\n", state );

	switch ( state ) {
	case 0xf8: /* Bus is idle (does not interrupt) */
	    /* This is the usual stop state.
	     * We only see this when polling.
	     */
	    sc->status = TWI_DONE;
	    break;
	case 0x08: /* start successful, ready for address */
        case 0x10: /* repeated start */
	    base->data = sc->addr;
	    twi_clear_irq ( base );
	    break;
	case 0x48: /* no ACK after sending data (error) */
	    sc->status = TWI_DONE;
	    break;
	default:
	    sc->status = TWI_DONE;
	}

	if ( sc->status == TWI_DONE ) {
	    twi_stop ( base );
	    sc->status = TWI_IDLE;;
            sem_unblock ( sc->sem );
	}

}

static void
twi_handler ( int devnum )
{
        struct twi_softc *sc = &twi_soft[devnum];
        struct twi_dev *base = sc->base;

	/* ACK the interrupt */
	base->ctrl &= ~CTRL_INT_ENA;

        // printf ( " *** twi_handler (ctrl): %08x\n", base->ctrl );

	twi_engine ( sc );
}

#ifdef notdef
/* We use this until we get interrupts working.
 */
void
twi_poll ( struct twi_softc *sc )
{
	struct twi_dev *base = sc->base;
	int timeout;
	int limit;

	// for ( ;; ) {
	for ( limit = 0; limit < 10; limit++ ) {
	    timeout = 1000;
	    while ( ( (base->ctrl & CTRL_INT_FLAG) == 0 ) && timeout-- )
		;
	    printf ( "twi_poll %d\n", timeout );
	    twi_engine ( sc );
	    if ( sc->status == TWI_DONE )
		break;
	}
	sc->status = TWI_IDLE;
}
#endif

#define CLOCK_24M       24000000
#define CLOCK_48M       48000000

/* XXX - assumes 48M clock */
static void
twi_clock ( struct twi_dev *base, int fast )
{
	int m, n;

	if ( fast ) {	/* 400K */
	    m = 2;
	    n = 2;
	} else {	/* 100K */
	    m = 11;
	    n = 2;
	}
	base->ccr = m<<3 + n;
	// show_reg ( "TWI clock set: (ccr) ", &base->ccr );
}

static void
twi_reinit ( struct twi_dev *base )
{
	/* XXX - do soft reset */
        // base->ctrl |= CTRL_INT_ENA;
	twi_clock ( base, 0 );
}
static void
twi_stop ( struct twi_dev *base )
{
	unsigned int creg;
	int timeout;

	printf ( " --- twi_stop\n" );

	creg = base->ctrl;
	creg |= CTRL_M_STOP;
	creg |= CTRL_INT_FLAG;
	base->ctrl = creg;

	// twi_clear_irq ( base );

	timeout = 10000;
	while ( base->ctrl & CTRL_M_STOP && timeout-- )
	    ;
	printf ( "TWI stop, timeout: %d\n", timeout );
	show_reg ( "Stopped TWI (ctrl) ", &base->ctrl );
	show_reg ( "Stopped TWI (status) ", &base->status );

	/* At this point, status should be 0xf8, which will
	 * NOT interrupt (the bus is idle)
	 */
	if ( base->status != 0xf8 )
	    printf ( "Stop failed to return bus to Idle\n" );
}

static void
twi_start ( struct twi_dev *base )
{
	int timeout;

	printf ( " --- twi_start\n" );
	base->reset = 1;
	twi_reinit ( base );

	base->ctrl &= ~(CTRL_A_ACK | CTRL_INT_FLAG);
	base->efr = 0;
	base->lcr = LCR_IDLE;
        base->ctrl |= CTRL_INT_ENA;
        base->ctrl |= CTRL_BUS_ENA;
	base->ctrl |= CTRL_M_START;

	// show_reg ( "Starting TWI (ctrl) ", &base->ctrl );

	/* This did timeout before we set BUS_ENA */
	timeout = 10000;
	while ( base->ctrl & CTRL_M_START && timeout-- )
	    ;
	if ( timeout < 500 )
	    printf ( "TWI start, timeout: %d\n", timeout );

	// show_reg ( "Started TWI (ctrl) ", &base->ctrl );
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
	sc->status = TWI_IDLE;
	sc->dev = devnum;

#ifdef notyet
        twi_reset_i ( base );

        base->psc = PSC_2;
        base->scll = 120;
        base->sclh = 120;
        base->buf = BUF_RX_CLR | BUF_TX_CLR;
#endif

	// printf ( "TWI irq %d for device %d setup\n", irq, devnum );
        irq_hookup ( irq, twi_handler, devnum );

        // base->ctrl |= CTRL_INT_ENA;
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

static void
twi2_setup ( void )
{
        // setup_twi2_mux ();
        twi_devinit ( 2, (struct twi_dev *) TWI2_BASE, IRQ_TWI2 );
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
	twi_clocks_on ();

        twi0_setup ();
        twi1_setup ();
        twi2_setup ();

	twi_reinit ( twi_soft[0].base );
	twi_reinit ( twi_soft[1].base );
	twi_reinit ( twi_soft[2].base );
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */

/* Standard entry points follow */


void
iic_init ( int sda_pin, int scl_pin )
{
	/* stub */
}

int
iic_recv ( int addr, char *buf, int count )
{
	/* stub */
}

int
iic_send ( int addr, char *buf, int count )
{
	/* stub */
}

/* Called from i2c.c */
void *
i2c_hw_init ( int devnum )
{
        if ( devnum < 0 || devnum > NUM_TWI )
            return (void *) 0;
        return (void *) &twi_soft[devnum];
}

/* We could use 10 bit addresses rather than 7 bit as follows:
 * 
 * The first byte for a 7 bit address is the whole story and
 *  looks like:   xxxx xxxM
 *  Here M = 0 for write, 1 for read.
 * For a 10 bit address, the first byte is:
 *	b1 = 0x78 | ( (addr)>>8 ) & 0x03;
 *	b1 = b1<<1 | mode;
 *	where mode = 0 for write, 1 for read.
 *	i.e.  1111 xxxR
 * The second byte is simply the low 8 bits.
 *	b2 = addr & 0xff;
 */

int
i2c_hw_send ( void *ii, int addr, char *buf, int count )
{
	struct twi_softc *sc = (struct twi_softc *) ii;

	printf ( "i2c_hw_send for addr 0x%02x on port %d\n", addr, sc->dev );
	if ( sc->status != TWI_IDLE ) {
	    printf ( "i2c busy!\n" );
	    return 0;
	}

	sc->tbufp = buf;
	sc->tcount = count;
	sc->addr = addr<<1;	/* 7 bits addr, write */
	sc->status = TWI_RUN;

	twi_start ( sc->base );
	// twi_poll ( sc );
	sem_block ( sc->sem );
}

int
i2c_hw_recv ( void *ii, int addr, char *buf, int count )
{
	struct twi_softc *sc = (struct twi_softc *) ii;

	printf ( "i2c_hw_recv for addr 0x%02x on port %d\n", addr, sc->dev );
	if ( sc->status != TWI_IDLE ) {
	    printf ( "i2c busy!\n" );
	    return 0;
	}

	sc->rbufp = buf;
	sc->rcount = count;
	sc->addr = addr<<1 | 1;	/* 7 bits addr, read */
	sc->status = TWI_RUN;

	twi_start ( sc->base );
	// twi_poll ( sc );
	sem_block ( sc->sem );
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */
/* Test and debug follows */

#define TEST_DEV	2

static void
twi_show ( void )
{
	struct twi_dev *base;

	base = twi_soft[TEST_DEV].base;

	printf ( "Device  : %d\n", TEST_DEV );
	printf ( "Base    : %08x\n", base );
	show_reg ( "i2c Ctrl   :", &base->ctrl );
	show_reg ( "i2c Status :", &base->status );
	show_reg ( "i2c Clock  :", &base->ccr );
	show_reg ( "i2c LCR    :", &base->lcr );
}

void
twi_test ( void )
{
	/* Do this again, so we can hand trigger it */
	// i2c_init ();

	// twi_start ( twi_soft[TEST_DEV].base );
	// twi_show ();
	// gic_show ();
}

/* THE END */
