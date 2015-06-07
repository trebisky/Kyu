/* timer.c
 *
 * Simple driver for the DM timer
 * see section 20 of the am3359 TRM
 * especially control register
 *   description on pg 4113
 * 
 */
#include <kyu.h>
#include <thread.h>

#include <omap_ints.h>
#include <cpu.h>

void timer_irqena ( void );
void timer_irqdis ( void );

extern struct thread *cur_thread;

void timer_int ( int );
void thread_tick ( void );

#define TIMER0_BASE      0x44E05000

#define TIMER1MS_BASE    0x44E31000

#define TIMER2_BASE      0x48040000
#define TIMER3_BASE      0x48042000
#define TIMER4_BASE      0x48044000
#define TIMER5_BASE      0x48046000
#define TIMER6_BASE      0x48048000
#define TIMER7_BASE      0x4804A000

#define TIMER_BASE	TIMER0_BASE

/* registers in a timer (except the special timer 1) */

struct dmtimer {
	volatile unsigned long id;
	long _pad0[3];
	volatile unsigned long ocp;
	long _pad1[3];
	volatile unsigned long eoi;
	volatile unsigned long irq_sr;
	volatile unsigned long irq_st;
	volatile unsigned long ena;
	volatile unsigned long dis;
	volatile unsigned long wake;
	volatile unsigned long ctrl;
	volatile unsigned long count;
	volatile unsigned long load;
	volatile unsigned long trig;
	volatile unsigned long wps;
	volatile unsigned long match;
	volatile unsigned long cap1;
	volatile unsigned long sic;
	volatile unsigned long cap2;
};

/* bits in ENA/DIS registers and others */
#define TIMER_CAP	0x04
#define TIMER_OVF	0x02
#define TIMER_MAT	0x01

/* XXX - The weird 76677 gives close to 100 Hz ticks */
/*
#define TIMER_CLOCK	32768
 */
#define TIMER_CLOCK	76677

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
timer_rate_set_i ( int rate )
{
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;
	unsigned long val;

	val = 0xffffffff;
	val -= TIMER_CLOCK / rate;
	/*
	printf ( "Loading: %08x\n", val );
	*/

	/* load the timer */
	base->load = val;
	base->count = val;

#ifdef notdef
	/* see what actually got in there */
	/* Always returns 0 */
	printf ( "Timer load: %08x\n", val = base->load );
#endif

}

void
timer_init ( int rate )
{
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;
	int i;

	timer_rate = rate;

	timer_count_t = 0;
	timer_count_s = 0;

	timer_hook = (vfptr) 0;
#ifdef NET_TIMER
	net_timer_hook = (vfptr) 0;
#endif

#ifdef notdef
	printf ( "Timer id: %08x\n", base->id );
#endif
	irq_hookup ( NINT_TIMER0, timer_int, 0 );

	timer_rate_set_i ( rate );
	timer_irqena ();

	/* start the timer with autoreload */
	base->ctrl = 3;
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
timer_rate_set ( int hz )
{
	timer_irqdis ();
	timer_rate_set_i ( hz );
	timer_irqena ();
}

void
timer_test ( void )
{
	int i;
	int val, st;
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;

	for ( i=0; i<20; i++ ) {
	    val = base->count;
	    st = base->irq_sr;
	    printf ( "Timer: %08x %08x\n", val, st );
	    if ( st )
		base->irq_st = TIMER_OVF;
	    delay10 ();
	    delay10 ();
	}
}

void
timer_irqena ( void )
{
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;
	base->ena = TIMER_OVF;
}

void
timer_irqdis ( void )
{
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;
	base->dis = TIMER_OVF;
}

/* acknowledge the interrupt */
/* XXX - make this inline */
void
timer_irqack ( void )
{
	struct dmtimer *base = (struct dmtimer *) TIMER_BASE;
	base->irq_st = TIMER_OVF;
}

/* Handle a timer interrupt */
void
timer_int ( int xxx )
{
	static int subcount;

	timer_irqack ();

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
}

/* THE END */
