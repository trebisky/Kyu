/* timer.c
 *
 * Simple driver for the DM timer
 * see section 20 of the am3359 TRM
 * especially control register
 *   description on pg 4113
 * 
 */
#include <kyu.h>
#include <kyulib.h>
#include <thread.h>

#include <omap_ints.h>
#include <cpu.h>

void prcm_timer1_mux ( void );

extern struct thread *cur_thread;

void thread_tick ( void );

#define TIMER0_BASE      0x44E05000

#define TIMER1MS_BASE    0x44E31000	/* gets data abort */

#define TIMER2_BASE      0x48040000
#define TIMER3_BASE      0x48042000
#define TIMER4_BASE      0x48044000
#define TIMER5_BASE      0x48046000
#define TIMER6_BASE      0x48048000
#define TIMER7_BASE      0x4804A000

//#define TIMER_BASE	TIMER1MS_BASE

#define TIMER_BASE	TIMER0_BASE
#define TIMER_IRQ	IRQ_TIMER0

/* XXX - this doesn't belong here, but
 * I discovered when trying to use other than timer 0
 * that the MMU is not configured to let me address everything.
 * Apparently U-boot only sets up a window for timer 0 and 2.
 */
static int
peek ( unsigned long *addr )
{
	unsigned long val;

	printf ( "Peeking at %08x\n", addr );
	val = *addr;
	printf ( "Poke at %08x yielded %08x\n", addr, val );

}

static void
do_peeks ( void )
{
	peek ( (void *) 0x44E0B000 );	/* i2c */
	peek ( (void *) 0x4802A000 );	/* i2c */
	//peek ( (void *) 0x4819C000 );	/* i2c */
	peek ( (void *) TIMER0_BASE );
	// peek ( (void *) TIMER1MS_BASE );
	peek ( (void *) TIMER2_BASE );
	// peek ( (void *) TIMER3_BASE );
	// peek ( (void *) TIMER4_BASE );
	// peek ( (void *) TIMER5_BASE );
	// peek ( (void *) TIMER6_BASE );
	// peek ( (void *) TIMER7_BASE );
}

/* registers in a timer (except the special timer 1) */

struct dmtimer {
	volatile unsigned long id;		/* 00 */
	long _pad0[3];
	volatile unsigned long ocp;		/* 10 */
	long _pad1[3];
	volatile unsigned long eoi;		/* 20 */
	volatile unsigned long irq_stat_raw;	/* 24 */
	volatile unsigned long irq_stat;	/* 28 */
	volatile unsigned long ena;		/* 2c */
	volatile unsigned long dis;		/* 30 */
	volatile unsigned long wake;		/* 34 */
	volatile unsigned long ctrl;		/* 38 */
	volatile unsigned long count;		/* 3c */
	volatile unsigned long load;		/* 40 */
	volatile unsigned long trig;		/* 44 */
	volatile unsigned long pend;		/* 48 */
	volatile unsigned long match;		/* 4c */
	volatile unsigned long cap1;		/* 50 */
	volatile unsigned long sic;		/* 54 */
	volatile unsigned long cap2;		/* 58 */
};

/* Bits in the write pending register */
#define	POST_CTRL	0x1
#define	POST_COUNT	0x2
#define	POST_LOAD	0x4
#define	POST_TRIG	0x8
#define	POST_MATCH	0x10

#define SIC_POSTED	0x4	/* enable posted mode */
#define SIC_RESET	0x2	/* Soft Reset */

/* This timer is unique and unlike all the others */
struct dmtimer1 {
	volatile unsigned long id;		/* 00 */
	long _pad0[3];
	volatile unsigned long ocp;		/* 10 */
	volatile unsigned long stat;		/* 14 */
	volatile unsigned long istat;		/* 18 */
	volatile unsigned long enable;		/* 1c */
	volatile unsigned long wake;		/* 20 */
	volatile unsigned long control;		/* 24 */
	volatile unsigned long count;		/* 28 */
	volatile unsigned long load;		/* 2c */
	volatile unsigned long reload;		/* 30 */
	volatile unsigned long post;		/* 34 */
	volatile unsigned long match;		/* 38 */
	volatile unsigned long cap1;		/* 3c */
	volatile unsigned long sic;		/* 40 */
	volatile unsigned long cap2;		/* 44 */
	volatile unsigned long pir;		/* 48 */
	volatile unsigned long nir;		/* 4c */
	volatile unsigned long tcvr;		/* 50 */
	volatile unsigned long tocr;		/* 54 */
	volatile unsigned long towr;		/* 58 */
};

/* bits in ENA/DIS registers and others */
#define TIMER_CAP	0x04
#define TIMER_OVF	0x02
#define TIMER_MAT	0x01

/* bits in the ctrl register */
#define CTRL_START	0x1		/* start the timer */
#define CTRL_AUTO	0x2		/* autoreload enable */

#define CTRL_PRE	0x20		/* prescaler enable */
#define PRE_SHIFT	2
#define PRE_MASK	0x1c		/* 3 bits */

/* With the prescaler disabled, we get the raw clock.
 * With the prescaler enabled, we get at least a divide by two
 *  so with 3 bits we can divide by 2 through 256.
 *  i.e. the divisor is 2^(N+1)
 *
 * The raw clock is selected in the PRCM (power, reset, and clock management).
 * This is chapter 8 of the TRM.  Especially pages 901 to 902
 */

/* XXX - The weird 76677 gives close to 100 Hz ticks */
/*
#define TIMER_CLOCK	32768
 */
//#define TIMER_CLOCK	76677
#define TIMER_CLOCK	38194

static volatile long timer_count_t;
static volatile long timer_count_s;

/* Needed by imported linux code.
 */
volatile unsigned long jiffies;

static vfptr timer_hook;
#ifdef NET_TIMER
static vfptr net_timer_hook;
#endif

static int timer_rate;

/* --------------------------------------------------- */

#define NXXX	20

static int xxx_first = NXXX;
static int xxx_start = 1;

static int xxx_data[20];
static int xxx_counts[20];
static unsigned long xxx_stat[20];
static int xxx_count = 0;

void
timer_xxx ( void )
{
	int i;

	for ( i=0; i<NXXX; i++ )
	    printf ( "XXX timer; %d %08x %08x\n", xxx_data[i], xxx_counts[i], xxx_stat[i] );
}

/* --------------------------------------------------- */
/* --------------------------------------------------- */

/* The following have been crudely calibrated
 * by guessing and using a stopwatch.
 */

/* Close to a 1 second delay */
void
delay1 ( void )
{
    volatile long x = 250000000;

    while ( x-- > 0 )
	;
}

/* About a 0.1 second delay */
void
delay10 ( void )
{
    volatile long x = 25000000;

    while ( x-- > 0 )
	;
}

/* Use for sub millisecond delays
 * argument is microseconds.
 */
void
_udelay ( int n )
{
    volatile long x = 250 * n;

    while ( x-- > 0 )
	;
}

/* --------------------------------------------------- */
/* --------------------------------------------------- */

void
timer_hookup ( vfptr new )
{
	timer_hook = new;
}

#ifdef NET_TIMER
void
net_timer_hookup ( vfptr new )
{
	net_timer_hook = new;
}
#endif

/* Count in ticks */
int
get_timer_count_t ( void )
{
	return timer_count_t;
}

/* Count in seconds */
int
get_timer_count_s ( void )
{
	return timer_count_s;
}

static void
post_spin ( int mask )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	while ( tmr->pend & mask )
	    ;
}

/* These routines with prefix "dmtimer_" handle the timers
 * with the structure other than timer1
 */

static void
dmtimer_rate_set_i ( int rate )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	unsigned long val;

	val = 0xffffffff;
	val -= TIMER_CLOCK / rate;
	printf ( "Loading: %08x\n", val );

	/* load the timer */
	post_spin ( POST_LOAD );
	tmr->load = val;
	post_spin ( POST_COUNT );
	tmr->count = val;

#ifdef notdef
	/* see what actually got in there */
	/* Always returns 0 */
	post_spin ( POST_LOAD );
	printf ( "Timer load: %08x\n", val = tmr->load );
#endif

}

void
dmtimer_irqena ( void )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	tmr->ena = TIMER_OVF;
}

void
dmtimer_irqdis ( void )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	tmr->dis = TIMER_OVF;
}

void
dmtimer_rate_set ( int hz )
{
	dmtimer_irqdis ();
	dmtimer_rate_set_i ( hz );
	dmtimer_irqena ();
}

// static int limit = 20;

/* Handle a timer interrupt */
void
dmtimer_int ( int xxx )
{
	static int subcount;
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	/* Abandon bogus interrupts */
	if ( ! (tmr->irq_stat & TIMER_OVF) )
	    return;
	    
	// printf ( "TIMER int !!\n" );
	// if ( limit > 0 ) {
	// post_spin ( POST_COUNT );
	//     printf ( "TIMER int %08x !!\n", tmr->count );
	//     limit--;
	// }

	if ( xxx_first > 0 ) {
	    if ( xxx_start ) {
		reset_ccnt ();
		xxx_start = 0;
	    }
	    // printf ( "Tick: %d\n", get_ccnt () );
	    // printf ( " regs 1 -- %08x %08x\n", tmr->irq_stat_raw, tmr->irq_stat );
	    xxx_data[xxx_count] = get_ccnt ();
	    post_spin ( POST_COUNT );
	    xxx_counts[xxx_count] = tmr->count;
	    xxx_stat[xxx_count] = tmr->irq_stat;
	    xxx_count++;
	}

	/* ACK the interrupt */
	// tmr->irq_stat = TIMER_OVF;

	if ( xxx_first > 0 ) {
	    // printf ( " regs 2 -- %08x %08x\n", tmr->irq_stat_raw, tmr->irq_stat );
	    --xxx_first;
	}

	++jiffies;

	/* These counts are somewhat bogus,
	 * but handy when first bringing up the timer
	 * and interrupt system.
	 *
	 * We used to make then available as global variables,
	 * but that was tacky.  Now we provide access to them
	 * via function calls.
	 */
	++timer_count_t;

	++subcount;
	if ( (subcount % timer_rate) == 0 ) {
	    ++timer_count_s;
	}

	if ( ! cur_thread )
	    panic ( "timer, cur_thread" );

	++cur_thread->prof;

	thread_tick ();

	if ( timer_hook ) {
	    (*timer_hook) ();
	}

#ifdef NET_TIMER
	if ( net_timer_hook ) {
	    (*net_timer_hook) ();
	}
#endif

	/* ACK the interrupt */
	tmr->irq_stat = TIMER_OVF;
}

/* This demonstrates that Timer 0 runs at 38194 Hz.
 * This timer runs off the sloppy RC 32768 oscillator,
 *  so we should not be altogether surprised.
 * This is the only possible source for Timer 0.
 *  we could run this calibration and use the determined value.
 */
void
dmtimer_checkrate ( void )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;
	unsigned long v1, v2;
	unsigned long c1, c2;

	printf ( "Timer control reg = %08x\n", tmr->ctrl );
	dmtimer_irqdis ();
	// tmr->ctrl = 0;

	post_spin ( POST_COUNT );
	tmr->count = 9;
	post_spin ( POST_LOAD );
	tmr->load = 9;

	post_spin ( POST_CTRL );
	tmr->ctrl = CTRL_START;

	post_spin ( POST_COUNT );
	printf ( "COUNT = %d\n", tmr->count );
	delay_ns ( 1000 );
	post_spin ( POST_COUNT );
	printf ( "COUNT = %d\n", tmr->count );
	delay_ns ( 1000 );
	post_spin ( POST_COUNT );
	printf ( "COUNT = %d\n", tmr->count );
	delay_ns ( 1000 );
	post_spin ( POST_COUNT );
	printf ( "COUNT = %d\n", tmr->count );

	v1 = tmr->count;
	    delay_ns ( 1000 * 1000 * 1000 );
	v2 = tmr->count;

	post_spin ( POST_CTRL );
	tmr->ctrl = 0;
	printf ( "CCNT = %d\n", get_ccnt() );

	printf ( "Checkrate: %d %d -- %d\n", v1, v2, v2-v1 );
}

void
dmtimer_init ( int rate )
{
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	dmtimer_checkrate ();

	/* This is 0x4 from U-boot (posted is set) */
	// printf ( " SIC == %08x\n", tmr->sic );
	tmr->sic = SIC_POSTED;

	/* Stop the timer */
	dmtimer_irqdis ();
	post_spin ( POST_CTRL );
	tmr->ctrl = 0;

	// prcm_timer1_mux ();

	timer_count_t = 0;
	timer_count_s = 0;

	timer_hook = (vfptr) 0;
#ifdef NET_TIMER
	net_timer_hook = (vfptr) 0;
#endif

	printf ( "Timer id: %08x\n", tmr->id );

	irq_hookup ( TIMER_IRQ, dmtimer_int, 0 );

	timer_rate = rate;
	dmtimer_rate_set_i ( rate );
	dmtimer_irqena ();

	/* start the timer with autoreload */
	post_spin ( POST_CTRL );
	tmr->ctrl = CTRL_START | CTRL_AUTO;
}

void
dmtimer_test ( void )
{
	int i;
	int val, st;
	struct dmtimer *tmr = (struct dmtimer *) TIMER_BASE;

	for ( i=0; i<20; i++ ) {
	    val = tmr->count;
	    st = tmr->irq_stat;
	    printf ( "Timer: %08x %08x\n", val, st );
	    if ( st )
		tmr->irq_stat = TIMER_OVF;
	    delay10 ();
	    delay10 ();
	}
}

/* Called during Kyu startup */
void
timer_init ( int rate )
{
	// struct dmtimer1 *tmr = (struct dmtimer1 *) TIMER_BASE;
	// printf ( "Timer ID = %08x\n", tmr->id );
	// do_peeks ();

	dmtimer_init ( rate );
}

/* Public entry point.
 */
int
timer_rate_get ( void )
{
	return timer_rate;
}

/* Public entry point.
 * Set a different timer rate
 */
void
timer_rate_set ( int rate )
{
	timer_rate = rate;
	dmtimer_rate_set ( rate );
}

/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */

/* The following is in the PRCM module and controls
 * the mux that feeds clocks to the timers (and other things).
 * These are the CM_DPLL registers.
 * "CM" stands for "Clock Module"
 * See TRM page 1048.
 */

#define CM_DPLL		0x44E00500

struct clksel {
	unsigned long _pad0;
	volatile unsigned long timer7;
	volatile unsigned long timer2;
	volatile unsigned long timer3;
	volatile unsigned long timer4;
	volatile unsigned long mac;
	volatile unsigned long timer5;
	volatile unsigned long timer6;
	volatile unsigned long rft;
	unsigned long _pad1;
	volatile unsigned long timer1ms;
	volatile unsigned long gfx;
	volatile unsigned long pru;
	volatile unsigned long pixel;
	volatile unsigned long wdt1;
	volatile unsigned long debounce;
};

/* Timer 0 always gets the sloppy RC 32 khz clock */

/* Timer 1 gets 5 choices */
#define CLK_1MS_MASTER	0
#define CLK_1MS_32PLL	1
#define CLK_1MS_PIN	2
#define CLK_1MS_32RC	3
#define CLK_1MS_32XL	4

/* Timers 2 through 8 get 3 choices */
#define CLK_PIN		0
#define CLK_MASTER	1
#define CLK_32PLL	2

void
prcm_timer1_mux ( void )
{
	struct clksel *sel = (struct clksel *) CM_DPLL;

	sel->timer1ms = CLK_1MS_32XL;
}

/* THE END */
