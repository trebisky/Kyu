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
 *  interrupt numbers in interrupts.h
 * test hooks in test_io.c
 *
 * Valuable information in Armbian/drivers/i2c/busses/i2c-sunxi.c
 *
 * Coding began 7-27-2020
 * A typo in the ccu.c file caused the release of reset for this
 *  unit to be misdirected.  Once that was fixed, we got results.
 * Another typo with a wrong interrupt number caused trouble.
 * A third big mistake was calling twi_init before gic_init.
 *
 * I was pulling my hair out with the unit stuck giving a 0xf9 status,
 * which is undefined/reserved.  Reset would not clear it, but a
 * power cycle straightened things out.
 * Perhaps the 9pulse trick would turn the trick.
 *  (apparently not).
 * This doesn't seem to arise with a proper driver, but it may
 *  if we try addessing a device this isn't actually present.
 *
 */

/*
#define USE_POLLING
 */

static int twi_debug = 0;

#include "kyu.h"
#include "thread.h"
#include "interrupts.h"

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

#define LCR_SDA_EN	0x01
#define LCR_SDA_LEVEL	0x02	/* force this level */
#define LCR_SCL_EN	0x04
#define LCR_SCL_LEVEL	0x08	/* force this level */
#define LCR_SDA_STATE	0x10	/* current level */
#define LCR_SCL_STATE	0x20	/* current level */

#define TWI_STATUS_IDLE 0xf8

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
	int error;
        char *tbufp;
        char *rbufp;
        int tcount;
        int rcount;
	int addr;
        struct sem *sem;
        enum twi_status status;
} twi_soft[NUM_TWI];

static void twi_stop ( struct twi_dev * );
static int twi_start ( struct twi_dev * );

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

/* A lot could be done here about error handling */

static void
twi_engine ( struct twi_softc *sc )
{
        struct twi_dev *base = sc->base;
	int state;

	state = base->status;
	if ( twi_debug )
	    printf ( " twi_engine  (stat): %08x\n", state );

#ifdef USE_POLLING
	/* This extra delay seems important */
	state = base->status;
	if ( twi_debug )
	    printf ( " twi_engine+ (stat): %08x\n", state );
#endif

	switch ( state ) {
	case 0xf8: /* Bus is idle (does not interrupt) */
	    /* This is the usual stop state.
	     * We only see this when polling.
	     * Not an error.
	     */
	    sc->status = TWI_DONE;
	    break;
	case 0x08: /* start successful, ready for address */
        case 0x10: /* repeated start */
	    if ( twi_debug )
		printf ( "TWI send addr: %02x\n", sc->addr );
	    base->data = sc->addr;
	    twi_clear_irq ( base );
	    break;
	case 0x18: /* address has been ACKed for write */
	    /* send second part of 10 bit address or first data byte */
	case 0xd0:
	case 0x28: /* data has been ACKed for write */
	    if ( sc->tcount ) {
		if ( twi_debug )
		    printf ( "TWI send: %02x\n", *sc->tbufp );
		base->data = *sc->tbufp++;
		twi_clear_irq ( base );
		sc->tcount--;
		break;
	    }
	    /* No more data -- if we sent a byte that
	     * the device isn't expecting, the next state
	     * will be 0x30 (we won't get an ACK)
	     */
	    if ( twi_debug )
		printf ( "TWI all done sending\n" );
	    sc->status = TWI_DONE;
	    break;
	case 0x40: /* address has been ACKed for read */
	    /* enable the ACK to receive */
	    base->ctrl |= CTRL_A_ACK;
	    twi_clear_irq ( base );
	    break;
	    /* next will be 0x50 */
	case 0x50: /* data received, ACK has been sent */
	    *sc->rbufp++ = base->data;
	    sc->rcount--;
	    /* Don't ACK the last byte */
	    if ( sc->rcount == 1 )
		base->ctrl &= ~CTRL_A_ACK;
	    twi_clear_irq ( base );
	    break;
	case 0x58: /* data received, no ACK was sent */
	    *sc->rbufp++ = base->data;
	    sc->rcount--;
	    // twi_clear_irq ( base );
	    if ( twi_debug )
		printf ( "TWI all done receiving\n" );
	    sc->status = TWI_DONE;
	    break;

	/* Various error cases */
	case 0x20: /* address failed to ACK for write */
	case 0x30: /* data sent, failed to ACK */
	case 0x38: /* lost arbitration */
	case 0x48: /* no ACK after sending data (error) */
	case 0xd8: /* no ACK after sending addr + write */
	case 0x0:  /* bus error */
	default:
	    sc->error = state;
	    sc->status = TWI_DONE;
	    break;
	}

	if ( sc->status == TWI_DONE ) {
	    twi_stop ( base );
	    sc->status = TWI_IDLE;;
#ifndef USE_POLLING
            sem_unblock ( sc->sem );
#endif
	}

}

static void
twi_handler ( int devnum )
{
        struct twi_softc *sc = &twi_soft[devnum];
        // struct twi_dev *base = sc->base;

	// printf ( " *** twi_handler (ctrl): %08x\n", base->ctrl );

	twi_engine ( sc );
}

#ifdef USE_POLLING
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

	    if ( twi_debug )
		printf ( "twi_poll %d\n", timeout );

	    /* We see this bogus status when we do unexpected things.
	     * The only fix I have found is a power cycle.
	     */
	    if ( base->status == 0xf9 ) {
		printf ( "TWI poll, nasty status: %02x\n", base->status );
		break;
	    }

	    twi_engine ( sc );

	    if ( sc->status == TWI_DONE || sc->status == TWI_IDLE ) {
		if ( twi_debug )
		    printf ( "TWI poll, finished\n" );
		break;
	    }
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
	//twi_clock ( base, 0 );
}

/* Called from in the interrupt routine.
 */
static void
twi_stop ( struct twi_dev *base )
{
	unsigned int creg;
	int timeout;

	if ( twi_debug )
	    printf ( " --- twi_stop\n" );

	creg = base->ctrl;
	creg |= CTRL_M_STOP;
	creg |= CTRL_INT_FLAG;
	base->ctrl = creg;

	// twi_clear_irq ( base );

	timeout = 10000;
	while ( base->ctrl & CTRL_M_STOP && timeout-- )
	    ;
	if ( twi_debug ) {
	    printf ( "TWI stop, timeout: %d\n", timeout );
	    show_reg ( "Stopped TWI (ctrl) ", &base->ctrl );
	    show_reg ( "Stopped TWI (status) ", &base->status );
	}

	/* At this point, status should be 0xf8, which will
	 * NOT interrupt (the bus is idle)
	 */
	if ( base->status != 0xf8 )
	    printf ( "TWI: stop failed to return bus to Idle\n" );
}

/* The bus is not idle, but it should be.
 * send 9 pulses on SDA an see if that releases it.
 */
static void
twi_kick_butt ( struct twi_dev *base )
{
	int repeat;
	int is_high;

	base->lcr |= LCR_SDA_EN;

	for ( repeat=0; repeat < 9; repeat++ ) {
	    is_high = base->lcr & LCR_SDA_STATE;
	    is_high &= base->lcr & LCR_SDA_STATE;
	    is_high &= base->lcr & LCR_SDA_STATE;
	    if ( is_high )
		break;

	    base->lcr &= ~LCR_SCL_LEVEL;
	    delay_us ( 1000 );
	    base->lcr |= LCR_SCL_LEVEL;
	    delay_us ( 1000 );
	}

	base->lcr &= ~LCR_SDA_EN;

	if ( ! base->lcr & LCR_SDA_STATE ) {
	    printf ( "TWI sda still stuck low\n" );
	    printf ( " TWI status: %02x\n", base->status );
	    printf ( " TWI lcr: %02x\n", base->lcr );
	}
}

static int
twi_start ( struct twi_dev *base )
{
	int timeout;
	int creg;

	if ( twi_debug )
	    printf ( " --- twi_start\n" );
	base->reset = 1;
	base->ctrl = CTRL_BUS_ENA;

	delay_us ( 100 );

	if ( base->status != TWI_STATUS_IDLE ) {
	    printf ( "TWI start, not idle (kicking it), status: %02x\n", base->status );
	    twi_kick_butt ( base );
	    if ( base->status != TWI_STATUS_IDLE ) {
		printf ( "TWI cannot start, not idle, status: %02x\n", base->status );
		return 1;
	    }
	}

	twi_clock ( base, 0 );

	base->efr = 0;
	base->lcr = LCR_IDLE;

	creg = base->ctrl;
	creg &= ~(CTRL_A_ACK | CTRL_INT_FLAG);
#ifndef USE_POLLING
        creg |= CTRL_BUS_ENA | CTRL_INT_ENA;
#else
        creg |= CTRL_BUS_ENA;
#endif
	base->ctrl = creg;
	creg |= CTRL_M_START;
	base->ctrl = creg;

	/* With interrupts, the following messages are
	 * basically useless.  Everything happens and then
	 * we see them when everthing finishes
	 */
#ifdef USE_POLLING
	show_reg ( "Starting TWI (ctrl) ", &base->ctrl );

	/* This did timeout before we set BUS_ENA */
	timeout = 10000;
	while ( base->ctrl & CTRL_M_START && timeout-- )
	    ;
	if ( timeout < 500 )
	    printf ( "TWI start, timeout: %d\n", timeout );
	show_reg ( "Started TWI (ctrl) ", &base->ctrl );
#endif

	return 0;
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
	sc->error = 0;

	// printf ( "TWI irq %d for device %d setup\n", irq, devnum );
        irq_hookup ( irq, twi_handler, devnum );

	// twi_clock ( base, 0 );

	printf ( "i2c device %d initialized\n", devnum );

        // base->ctrl |= CTRL_INT_ENA;
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
        setup_twi_mux ();

        twi_devinit ( 0, (struct twi_dev *) TWI0_BASE, IRQ_TWI0 );
        twi_devinit ( 1, (struct twi_dev *) TWI1_BASE, IRQ_TWI1 );
	// TWI 2 is not routed to a connector
        // twi_devinit ( 2, (struct twi_dev *) TWI2_BASE, IRQ_TWI2 );
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

	if ( twi_debug )
	    printf ( "i2c_hw_send for addr 0x%02x on port %d\n", addr, sc->dev );
	if ( sc->status != TWI_IDLE ) {
	    printf ( "i2c busy, cannot send!\n" );
	    return 0;
	}

	sc->tbufp = buf;
	sc->tcount = count;
	sc->addr = addr<<1;	/* 7 bits addr, write */
	sc->status = TWI_RUN;
	sc->error = 0;

	if ( twi_debug )
	    printf ( "-- TWI start for send\n" );

	if ( twi_start ( sc->base ) ) {
	    printf ( "i2c cannot start send\n" );
	    sc->status = TWI_IDLE;
	}

#ifdef USE_POLLING
	twi_poll ( sc );
#else
	sem_block ( sc->sem );
#endif
}

int
i2c_hw_recv ( void *ii, int addr, char *buf, int count )
{
	struct twi_softc *sc = (struct twi_softc *) ii;

	if ( twi_debug )
	    printf ( "i2c_hw_recv for addr 0x%02x on port %d\n", addr, sc->dev );
	if ( sc->status != TWI_IDLE ) {
	    printf ( "i2c busy, cannot recv!\n" );
	    return 0;
	}

	sc->rbufp = buf;
	sc->rcount = count;
	sc->addr = addr<<1 | 1;	/* 7 bits addr, read */
	sc->status = TWI_RUN;
	sc->error = 0;

	if ( twi_debug )
	    printf ( "-- TWI start for recv\n" );

	if ( twi_start ( sc->base ) ) {
	    printf ( "i2c cannot start recv\n" );
	    sc->status = TWI_IDLE;
	}

#ifdef USE_POLLING
	twi_poll ( sc );
#else
	sem_block ( sc->sem );
#endif
}

int
i2c_hw_error ( void *ii )
{
	struct twi_softc *sc = (struct twi_softc *) ii;

	if ( sc->base->status != TWI_STATUS_IDLE ) {
	    show_reg ( "TWI bus status: ", &sc->base->status );
	    return 1;
	}

	if ( sc->error ) {
	    printf ( "TWI error set: %d\n", sc->error );
	    return 1;
	}

	return 0;
}

/* -------------------------------------------------------- */
/* -------------------------------------------------------- */
/* Test and debug follows */

#define TEST_DEV	0

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

	// twi_show ();

	// twi_start ( twi_soft[TEST_DEV].base );
	// twi_show ();

	// hdc_test ();

	// My LCD demo that I ran for 2,000,000 updates over 2+ weeks
	// lcd_test ();

	dac_test ();
}

/* THE END */
