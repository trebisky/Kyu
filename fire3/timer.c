/* Timer for the s5p6818
 *
 * Tom Trebisky  9-17-2018
 */

#include "arch/types.h"
#include "arch/cpu.h"
#include "fire3_ints.h"

#define N_TIMERS	5

struct tmr {
	vu32	count;
	vu32	cmp;
	vu32	obs;
};

struct timer {
	vu32 config0;			/* 00 */
	vu32 config1;			/* 04 */
	vu32 control;			/* 08 */
	struct tmr timer[N_TIMERS-1];	/* 0C */
	vu32 t4_count;			/* 08 */
	vu32 t4_obs;			/* 08 */
	vu32	int_cstat;		/* 44 */
};

/* Stopwatch calibration shows that with load value of 0x80000
 * we get 0.525 seconds.  This is a 998644 Hz clock,
 * i.e. a 1 Mhz clock.
 */
#define TIMER_BASE	((struct timer *) 0xc0017000)
#define PWM_BASE	((struct timer *) 0xc0018000)

/* Timer is a down counter */

#define T0_AUTO		0x8
#define T0_INVERT	0x4
#define T0_LOAD		0x2
#define T0_RUN		0x1

#define T0_IENABLE		0x1
#define T1_IENABLE		0x2
#define T2_IENABLE		0x4
#define T3_IENABLE		0x8
#define T4_IENABLE		0x10

#define T0_ISTAT		0x20
#define T1_ISTAT		0x40
#define T2_ISTAT		0x80
#define T3_ISTAT		0x100
#define T4_ISTAT		0x200

#define TIMER_CLOCK	1000000

void
fire3_timer_rate_set ( int rate )
{
	struct timer *tp = TIMER_BASE;

	tp->control &= ~T0_RUN;

	// printf ( "BOGUS slow timer rate set\n" );
	// tp->timer[0].count = TIMER_CLOCK * 2;
	// tp->timer[0].count = TIMER_CLOCK / 100;

	tp->timer[0].count = TIMER_CLOCK / rate;
	tp->timer[0].cmp = 0;

	/* Pulse the load bit */
	tp->control |= T0_AUTO | T0_LOAD;
	tp->control &= ~T0_LOAD;

	tp->control |= T0_RUN;
}

static void
timer_start ( int rate )
{
	struct timer *tp = TIMER_BASE;

#ifdef notdef
	printf ( "Timer cstat = %08x\n", &tp->int_cstat );
	printf ( "Tc0 = %08x\n", tp->config0 );
	printf ( "Tc1 = %08x\n", tp->config1 );
	printf ( "Tctl = %08x\n", tp->control );
	printf ( "Cstat = %08x\n", tp->int_cstat );
	/* See if we can clear an interrupt without setting a bit.
	 * (we can!)
	 */
	tp->int_cstat = T0_ISTAT;
	printf ( "Cstat = %08x\n", tp->int_cstat );
	printf ( "T0 = %08x\n", tp->timer[0].obs );
	// printf ( "T1 = %08x\n", tp->timer[1].obs );
#endif

	/* XXX - this stops ALL timers */
	tp->control = 0;

	fire3_timer_rate_set ( rate );
	// printf ( "BOGUS slow timer rate set\n" );
	// fire3_timer_rate_set ( 1 );

#ifdef notdef
	tp->timer[0].count = TIMER_CLOCK / rate;
	tp->timer[0].cmp = 0;

	/* Pulse the load bit */
	tp->control = T0_AUTO | T0_LOAD;
	tp->control = T0_AUTO;
#endif

	// printf ( "T0 = %08x\n", tp->timer[0].obs );
	// printf ( "T0 = %08x\n", tp->timer[0].obs );
	// printf ( "--\n" );

	tp->int_cstat |= T0_IENABLE;

	intcon_ena ( IRQ_TIMER0 );
	tp->control |= T0_RUN;

	// printf ( "Go\n" );

	/*
	for ( ;; ) {
	    printf ( "T0 = %08x\n", tp->timer[0].obs );
	    delay ( 100000 );
	}
	*/

	/*
	for ( ;; ) {
	    if ( tp->int_cstat & T0_ISTAT ) {
		serial_putc ( '.' );
		tp->int_cstat = T0_ISTAT;
	    }
	}
	*/
}

static void
timer_ack ( void )
{
	struct timer *tp = TIMER_BASE;

	tp->int_cstat |= T0_ISTAT;
}

#ifdef notdef
static int tcount;

static void
timer_debug ( void )
{
	reg_t sp;

	tcount++;

	// get_SP ( sp );
	// printf ( "DING ---------------- %d %016lx\n", tcount, &tcount );
	// printf ( "DING ---------------- %d, sp = %016lx\n", tcount, sp);

	if ( tcount % 1000 == 0 ) {
	    get_SP ( sp );
	    printf ( "DING ---------------- %d, sp = %016lx\n", tcount, sp);
	}
}
#endif

/* Handle a timer interrupt */
/* Called at interrupt level */
static void
timer_handler ( int junk )
{
        timer_tick ();

	// timer_debug ();

        timer_ack ();
}

#ifdef notdef
static int last_count;

/* There are some problems with this.
 * This is really too early in initialization to
 * enable interrupts and some race conditions
 * will cause this to fail at the usual 1000 Hz rate.
 * It works fine with a 1 Hz rate.
 */
static void
timer_checkout ( void )
{
	last_count = -1;
	printf ( "Spinning in timer_init\n" );
	INT_unlock ();

	// for now Kyu startup will hang here
	// until we get interrupts working.
	for ( ;; ) {
	    // if ( tcount == 1 || (tcount % 1000) == 0 )
	    if ( tcount != last_count ) {
		printf (" *****\n" );
		printf (" ***** Tcount = %d\n", tcount );
		printf (" *****\n" );
		last_count = tcount;
	    }
	}

	printf ( "Done spinning in timer_init\n" );
}
#endif

/* Called during Kyu startup */
void
fire3_timer_init ( int rate )
{
        irq_hookup ( IRQ_TIMER0, timer_handler, 0 );

        timer_start ( rate );

	// timer_checkout ();

}

/* THE END */
