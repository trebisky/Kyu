/* i2c.c
 *
 * Tom Trebisky  Kyu project  5-25-2015
 *
 * The am3359 has 3 i2c interfaces.
 *  i2c-0, i2c-1, and i2c-2
 * on the BBB, the i2c-0 is used for a few on board
 *  devices and signals for it are not brought offboard.
 *
 * i2c-1 is available as 2 pins (SCL and SDA)
 * i2c-2 is available as 2 pins (SCL and SDA)
 *
 * This controller supports:
 *  standard -- 100 kb/s
 *  fast -- 400 kb/s
 *
 * The devices on i2c-0 are:
 * 1) 0x?? - TPS 65217 PMIC
 * 2) 0x70 - TDA19988 HDMI core
 * 2) 0x34 - TDA19988 CEC core
 * 3) 0x50 - 24LC32A board ID EEPROM (32Kbit) U7 (U8?)
 *
 * See Chapter 21 in the AM3359 TRM
 *
 * A lot can be learned from the linux driver for this thing.
 *  see:  drivers/i2c/busses/i2c-omap.c
 *
 * What the heck is OMAP you ask?
 *  It is a family of "high integration processors" cranked out
 *  by Texas Instruments.
 * OMAP is "Open Multimedia Applications Platform"
 *  Early versions had a TMS320 DSP as a core processor.
 * The 3358/3359 on the Beagleboard Black is not an OMAP, but a lot
 *  of the integrated peripherals share OMAP drivers
 *   (or Davinci drivers, which is a whole 'nuther story).
 *
 * This driver reported "bus busy" without anything connected until I
 *   added a pair of external 4.7K pullup resistors.
 */

#define I2C0_BASE      0x44E0B000
#define I2C1_BASE      0x4802A000
#define I2C2_BASE      0x4819C000

struct i2c_dev {
	volatile unsigned long revlo;		/* 00 */
	volatile unsigned long revhi;		/* 04 */
	long _pad0[2];
	volatile unsigned long sysc;		/* 10 */
	long _pad1[4];
	volatile unsigned long irqstatus_raw;	/* 24 */
	volatile unsigned long irqstatus;	/* 28 */
	volatile unsigned long irqenable_set;	/* 2c */
	volatile unsigned long irqenable_clr;	/* 30 */
	volatile unsigned long we;		/* 34 */
	volatile unsigned long dma_rx_set;	/* 38 */
	volatile unsigned long dma_tx_set;	/* 3c */
	volatile unsigned long dma_rx_clr;	/* 40 */
	volatile unsigned long dma_tx_clr;	/* 44 */
	volatile unsigned long dma_rx_wake_ena;	/* 48 */
	volatile unsigned long dma_tx_wake_ena;	/* 4c */
	long _pad2[16];
	volatile unsigned long syss;		/* 90 */
	volatile unsigned long buf;		/* 94 */
	volatile unsigned long count;		/* 98 */
	volatile unsigned long data;		/* 9c */
	long _pad3[1];
	volatile unsigned long con;		/* a4 */
	volatile unsigned long oa;		/* a8 */
	volatile unsigned long sa;		/* ac */
	volatile unsigned long psc;		/* b0 */
	volatile unsigned long scll;		/* b4 */
	volatile unsigned long sclh;		/* b8 */
	volatile unsigned long test;		/* bc */
	volatile unsigned long bufstat;		/* c0 */
	volatile unsigned long oa1;		/* c4 */
	volatile unsigned long oa2;		/* c8 */
	volatile unsigned long oa3;		/* cc */
	volatile unsigned long actoa;		/* d0 */
	volatile unsigned long sblock;		/* d4 */
};

/* Bits in the IRQSTATUS register */
#define IRQ_XDR		0x4000
#define IRQ_RDR		0x2000
#define IRQ_BB		0x1000		/* bus busy */
#define IRQ_ROVR	0x0800
#define IRQ_XUDF	0x0400
#define IRQ_AAS		0x0200
#define IRQ_BF		0x0100
#define IRQ_AERR	0x0080		/* access error */
#define IRQ_STC		0x0040
#define IRQ_GC		0x0020
#define IRQ_XRDY	0x0010		/* xmitter ready for data*/
#define IRQ_RRDY	0x0008		/* rcv data ready */
#define IRQ_ARDY	0x0004
#define IRQ_NACK	0x0002
#define IRQ_AL		0x0008

/* Bits in the CON register */
#define CON_ENABLE	0x8000
#define CON_MASTER	0x0400
#define CON_TRX		0x0200		/* 1 for Tx, 0 for Rx */
#define CON_XSA		0x0100		/* 1 for 10 bit slave address */
#define CON_STOP	0x0002
#define CON_START	0x0001

/* Bits in the PSC register - prescale values */
#define PSC_1		0x0000
#define PSC_2		0x0001
#define PSC_4		0x0003
#define PSC_256		0x00ff

/* Bits in the SYSC register - */
#define SYSC_RESET	0x0002

/* Bits in the BUF register - */
#define BUF_RX_CLR	0x4000
#define BUF_TX_CLR	0x0040

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

static int i2c_slave_addr;

void
i2c_set_slave ( int addr )
{
	i2c_slave_addr = addr;
}

static void
i2c_reset_i ( struct i2c_dev *base )
{
	int tmo = 1000;

	base->sysc = SYSC_RESET;

	while ( tmo-- ) {
	    if ( ! (base->sysc & SYSC_RESET) ) {
		// printf ( "Reset finished: %08x %d\n", base->sysc, tmo );
		return;
	    }
	}

	printf ( "Reset timed out: %d\n", tmo );
}

/* The clock, what oh what is the clock rate?
 * Diagrams show a 192 Mhz clock being divided by 4 outside of
 *  the i2c block.  So, let's say it is 48 Mhz.
 *  Divide this by 2 to get 24.
 *  (XXX - maybe we should divide by 4 to get 12?)
 * To get 100 kHz, 120 ticks hi and 120 ticks lo.
 */
static void
i2c_devinit ( struct i2c_dev *base )
{
	i2c_reset_i ( base );

	base->psc = PSC_2;
	base->scll = 120;
	base->sclh = 120;
	base->buf = BUF_RX_CLR | BUF_TX_CLR;

	base->con = CON_ENABLE;
}

/* XXX */
void
i2c_reset ( void )
{
	i2c_devinit ( (struct i2c_dev *) I2C1_BASE );
}

static int
i2c_irq_waitset ( int flag )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int tmo = 10000;

	// printf ( "irq_waitset: IRQ status R: %08x\n", base->irqstatus_raw );

	while ( tmo-- ) {
	    if ( base->irqstatus_raw & flag )
		return 0;
	}

	printf ( "irq_waitset TIMEOUT: IRQ status R: %08x\n", base->irqstatus_raw );
	return 1;
}

static int
i2c_irq_waitclr ( int flag )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int tmo = 10000;

	// printf ( "irq_waitclr: IRQ status R: %08x\n", base->irqstatus_raw );

	while ( tmo-- ) {
	    if ( ! (base->irqstatus_raw & flag) )
		return 0;
	}

	printf ( "irq_waitclr TIMEOUT: IRQ status R: %08x\n", base->irqstatus_raw );
	return 1;
}

/* The i2c bus is open drain with pull up resistors.
 *  so an idle bus will have both signals pulled high
 *  start pulls SDA low, while SCL stays high, then pulls SCL low
 *  stop releases first SCL, then releases SDA
 */

int
i2c_start ( void )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int tmo = 0;

	// printf ( "irq_start: IRQ status R: %08x\n", base->irqstatus_raw );
	// the above printf yields about 3 ms of delay

	if ( base->irqstatus_raw & IRQ_BB ) {
	    printf ( "i2c_start - sorry, bus busy: %08x\n", base->irqstatus_raw );
	    return 1;
	}

	base->con = CON_ENABLE | CON_MASTER | CON_START;

	// 10000 doesn't cut it
	while ( tmo < 10000000 ) {
	    if ( ! (base->con & CON_START) ) {
		// printf ( "Start finished: %08x %d\n", base->con, tmo );
		return 0;
	    }
	    tmo++;
	}

	printf ( "irq_start: IRQ status R: %08x\n", base->irqstatus_raw );
	printf ( "irq_start: con: %08x\n", base->con );

	printf ( "Start timed out: %d\n", tmo );
	return 1;
}

int
i2c_stop ( void )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int tmo = 0;

	printf ( "irq_stop: IRQ status R: %08x\n", base->irqstatus_raw );

	if ( base->irqstatus_raw & IRQ_BB ) {
	    printf ( "i2c_stop - sorry, bus busy: %08x\n", base->irqstatus_raw );
	    return 1;
	}

	base->con = CON_ENABLE | CON_MASTER | CON_STOP;

	// 10000 doesn't cut it
	while ( tmo < 10000000 ) {
	    if ( ! (base->con & CON_STOP) ) {
		printf ( "Start finished: %08x %d\n", base->con, tmo );
		return 0;
	    }
	    tmo++;
	}

	printf ( "irq_stop: IRQ status R: %08x\n", base->irqstatus_raw );
	printf ( "irq_stop: con: %08x\n", base->con );

	printf ( "Stop timed out: %d\n", tmo );
	return 1;
}

static int
i2c_con_waitclr ( int flag )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int tmo = 100000;

	// printf ( "con_waitclr: IRQ status R: %08x\n", base->con );

	while ( tmo-- ) {
	    if ( ! (base->con & flag) )
		return 0;
	}

	printf ( "con_waitclr TIMEOUT: IRQ status R: %08x\n", base->con );
	return 1;
}


int
i2c_send ( char *buf, int count )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	i2c_irq_waitclr ( IRQ_BB );
	if ( base->irqstatus_raw & IRQ_BB ) {
	    printf ( "i2c_send - sorry, bus busy: %08x\n", base->irqstatus_raw );
	    return 1;
	}

	base->sa = i2c_slave_addr;
	base->count = count;
	base->buf = BUF_RX_CLR | BUF_TX_CLR;

	// base->con = CON_ENABLE | CON_MASTER | CON_TRX;
	// base->con = CON_ENABLE | CON_MASTER | CON_TRX | CON_START;
	base->con = CON_ENABLE | CON_MASTER | CON_TRX | CON_START | CON_STOP;

	while ( count-- ) {
	    if ( i2c_irq_waitset( IRQ_XRDY ) )
		return 1;
	    base->data = *buf++;
	}

	/* XXX - We may be able to just tack this on at the start */
	// base->con = CON_ENABLE | CON_MASTER | CON_STOP;
	i2c_con_waitclr ( CON_STOP );

	return 0;
}

int
i2c_recv ( char *buf, int count )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	i2c_irq_waitclr ( IRQ_BB );
	if ( base->irqstatus_raw & IRQ_BB ) {
	    printf ( "i2c_send - sorry, bus busy: %08x\n", base->irqstatus_raw );
	    return 1;
	}

	base->sa = i2c_slave_addr;
	base->count = count;
	base->buf = BUF_RX_CLR | BUF_TX_CLR;

	base->con = CON_ENABLE | CON_MASTER | CON_START | CON_STOP;

	while ( count-- ) {
	    if ( i2c_irq_waitset( IRQ_RRDY ) )
		return 1;
	    *buf++ = base->data;
	}
	i2c_con_waitclr ( CON_STOP );

	return 0;
}

/* XXX - tentative names */
void
i2c_i2c0_setup ( void )
{
	setup_i2c0_mux ();
	// i2c_devinit ( (struct i2c_dev *) I2C0_BASE );
}

/* Uses mode 2 to put these on P9-17 and P9-18
 *  P9-17 is SCL
 *  P9-18 is SDA
 */
void
i2c_i2c1_setup ( void )
{
	setup_i2c1_mux ();
	i2c_devinit ( (struct i2c_dev *) I2C1_BASE );
}

/* Uses mode 2 to put these on P9-21 and P9-22
 *  P9-21 is SCL
 *  P9-22 is SDA
 */
void
i2c_i2c2_setup ( void )
{
	setup_i2c2_mux ();
	i2c_devinit ( (struct i2c_dev *) I2C2_BASE );
}

/* Called during system startup.
 * We only set the pin mux for the "0" interface
 * initially.
 */
void
i2c_init ( void )
{
	i2c_i2c0_setup ();
	i2c_i2c1_setup ();
	// i2c_i2c2_setup ();
}

void
i2c_show ( void )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	printf ( "Base: %08x\n", base );

	/*
	char *p1, *p2;
	printf ( "revlo:  %08x\n", &base->revlo );
	printf ( "sblock: %08x\n", &base->sblock );

	p1 = (char *) &base->revlo;
	p2 = (char *) &base->sblock;

	printf ( "sblock offset: %x\n", p2 - p1 );
	*/

	printf ( "IRQ status R: %08x\n", base->irqstatus_raw );
	printf ( "IRQ status  : %08x\n", base->irqstatus );
	printf ( "Buf status  : %08x\n", base->bufstat );
	printf ( "sysc        : %08x\n", base->sysc );
	printf ( "we/iv       : %08x\n", base->we );
	printf ( "CON         : %08x\n", base->con );
}

/* Calling i2c_start() back to back fails.
 *  some delay is required.
 * Even with a 100 us delay we get "bus busy"
 * Waiting 200 us works fine and yields a nice scope pattern.
 * The start yields a burst of clock pulses, it looks like
 * 9 of them, at 100 kHz or whatever has been programmed.
 * Note that such a burst lasts for about 90 us.
 * The data line pulses low just once per burst,
 *  staying low for about 10 us.
 */

void
i2c_try1 ( void )
{
	int count = 0;

	printf ( "Testing!\n" );
	for ( ;; ) {
	    if ( i2c_start () )
		break;
	    delay_ns ( 200 * 1000 );
	    count++;
	}
	printf ( "Stopped after %d\n", count );
}

// #define BUFS	512
#define BUFS	8

void
i2c_try2 ( void )
{
	char buf[BUFS];
	int i;
	int count = 0;
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	for ( i=0; i<BUFS; i++ )
	    buf[i] = 0xaa;

	printf ( "Testing!\n" );
	for ( ;; ) {
	    if ( i2c_send ( buf, BUFS ) )
		break;
	    // delay_ns ( 200 * 1000 );
	    printf ( "IRQ status R: %08x\n", base->irqstatus_raw );
	    if ( i2c_irq_waitclr ( IRQ_BB ) )
		break;
	    count++;
	}
	printf ( "Stopped after %d\n", count );
}

/* This was to watch how the states transition after
 * a transfer finished.
 * First we see 0x10 (XRDY for a write).
 * The next time we sample we see 0x116.
 * The main thing is ARDY isn't set right away and
 * needs to be waited for.
 */
void
i2c_try3 ( void )
{
	char buf[BUFS];
	int i;
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int s1, s2;

	for ( i=0; i<BUFS; i++ )
	    buf[i] = 0xaa;

	printf ( "Start, IRQ status R: %08x\n", base->irqstatus_raw );

	if ( i2c_send ( buf, BUFS ) )
	    printf ( "Trouble\n" );

	i = 0;
	s1 = base->irqstatus_raw;
	printf ( "Done, IRQ status R: %08x (%d)\n", s1, i );

	for ( ;; ) {
	    i++;
	    s2 = base->irqstatus_raw;
	    if ( s2 != s1 )
		printf ( "Done, IRQ status R: %08x (%d)\n", s2, i );
	    s1 = s2;
	}
}

/* The first thing that ever actually worked */
void
i2c_try4 ( void )
{
	char buf[2];
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	printf ( "Start, IRQ status R: %08x\n", base->irqstatus_raw );

	buf[0] = 0xD0;

	if ( i2c_send ( buf, 1 ) )
	    printf ( "send Trouble\n" );

	if ( i2c_irq_waitset( IRQ_ARDY ) )
	    printf ( "send-ardy Trouble\n" );

	if ( i2c_recv ( buf, 1 ) )
	    printf ( "recv Trouble\n" );

	if ( i2c_irq_waitset( IRQ_ARDY ) )
	    printf ( "recv-ardy Trouble\n" );

	printf ( " Received: %02x\n", buf[0]&0xff );

	i2c_show ();
}

/* ---------------------- */
/* ---------------------- */
/* ---------------------- */

void
i2c_write ( int reg, int val )
{
	char buf[2];

	buf[0] = reg;
	buf[1] = val;

	if ( i2c_send ( buf, 2 ) )
	    printf ( "send Trouble\n" );

	if ( i2c_irq_waitset( IRQ_ARDY ) )
	    printf ( "send-ardy Trouble\n" );
}

static void
i2c_read_reg_n ( int reg, unsigned char *buf, int n )
{
	buf[0] = reg;

	if ( i2c_send ( buf, 1 ) )
	    printf ( "send Trouble\n" );

	if ( i2c_irq_waitset( IRQ_ARDY ) )
	    printf ( "send-ardy Trouble\n" );

	if ( i2c_recv ( buf, n ) )
	    printf ( "recv Trouble\n" );

	if ( i2c_irq_waitset( IRQ_ARDY ) )
	    printf ( "recv-ardy Trouble\n" );
}

void
i2c_read_16n ( int reg, unsigned short *sbuf, int n )
{
	unsigned char buf[64];	/* XXX */
	int i;
	int j;

	i2c_read_reg_n ( reg, buf, n * 2 );

	for ( i=0; i<n; i++ ) {
	    j = i + i;
	    sbuf[i] = buf[j]<<8 | buf[j+1];
	}
}

int
i2c_read_8 ( int reg )
{
	char buf[1];

	i2c_read_reg_n ( reg, buf, 1 );

	return buf[0] & 0xff;
}

int
i2c_read_16 ( int reg )
{
	int rv;
	unsigned char buf[2];

	buf[0] = reg;

	i2c_read_reg_n ( reg, buf, 2 );

        rv = buf[0] << 8;
        rv |= buf[1];

	return rv;
}

int
i2c_read_24 ( int reg )
{
	int rv;
	unsigned char buf[3];

	buf[0] = reg;

	i2c_read_reg_n ( reg, buf, 3 );

	rv =  buf[0] << 16;
        rv |= buf[1] << 8;
        rv |= buf[2];

	return rv;
}

/* BMP180 definitions */
#define BMP_ADDR	0x77
#define  REG_ID         0xD0

#define NTEST	5000

void
i2c_try ( void )
{
	struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;
	int i;
	int id;
	int st;
	int con;

	i2c_set_slave ( BMP_ADDR );

	for ( i=0; i<NTEST; i++ ) {
	    id = i2c_read_8 ( REG_ID );
	    st = base->irqstatus_raw;
	    con = base->con;
	    if ( id != 0x55 )
		printf ( "ID = %02x %08x %08x  %d\n", id, st, con, i );
	}

	printf ( "Done\n" );
}

/* Currently wired to "t 19" */
void
i2c_test ( void )
{
	// struct i2c_dev *base = (struct i2c_dev *) I2C1_BASE;

	// i2c_show ();
	i2c_try ();
}

/* THE END */
