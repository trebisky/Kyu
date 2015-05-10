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

void timer_irqena ( void );
void timer_irqdis ( void );

extern struct thread *cur_thread;
extern int thread_debug;

extern long in_interrupt;
extern struct thread *in_newtp;

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

#define TIMER_ID	0x00
#define TIMER_OCP	0x10
#define TIMER_EOI	0x20
#define TIMER_IRQ_SR	0x24
#define TIMER_IRQ_ST	0x28
#define TIMER_ENA	0x2C
#define TIMER_DIS	0x30
#define TIMER_WAKE	0x34
#define TIMER_CTRL	0x38
#define TIMER_COUNT	0x3C
#define TIMER_LOAD	0x40
#define TIMER_TRIG	0x44
#define TIMER_WPS	0x48
#define TIMER_MATCH	0x4C
#define TIMER_CAP1	0x50
#define TIMER_SIC	0x54
#define TIMER_CAP2	0x58

/* bits in ENA/DIS registers and others */
#define TIMER_CAP	0x04
#define TIMER_OVF	0x02
#define TIMER_MAT	0x01

#define TIMER_CLOCK	32768

/* XXX XXX - eradicate this stuff */
/* May not need these, but here they are */
#define getb(a)          (*(volatile unsigned char *)(a))
#define getw(a)          (*(volatile unsigned short *)(a))
#define getl(a)          (*(volatile unsigned int *)(a))

#define putb(v,a)        (*(volatile unsigned char *)(a) = (v))
#define putw(v,a)        (*(volatile unsigned short *)(a) = (v))
#define putl(v,a)        (*(volatile unsigned int *)(a) = (v))

long timer_count_t = 0;
long timer_count_s = 0;

/* a kindness to the linux emulator.
 */
volatile unsigned long jiffies;

static vfptr timer_hook;
static vfptr net_timer_hook;

int timer_rate;

struct thread *timer_wait;

/* Close to a 1 second delay */
void
delay1 ( void )
{
    volatile long x = 200000000;

    while ( x-- > 0 )
	;
}

/* About a 0.1 second delay */
void
delay10 ( void )
{
    volatile long x = 20000000;

    while ( x-- > 0 )
	;
}

void
tmr_hookup ( vfptr new )
{
	timer_hook = new;
}

void
net_tmr_hookup ( vfptr new )
{
	net_timer_hook = new;
}


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
timer_rate_set ( int rate )
{
	char *base = (char *) TIMER_BASE;
	unsigned long val;

	val = 0xffffffff;
	val -= TIMER_CLOCK / rate;
	printf ( "Loading: %08x\n", val );

	/* load the timer */
	putl ( val, base + TIMER_LOAD );
	putl ( val, base + TIMER_COUNT );

#ifdef notdef
	/* see what actually got in there */
	/* Always returns 0 */
	val = getl ( base + TIMER_LOAD );
	printf ( "Timer load: %08x\n", val );
#endif

}

void
timer_init ( int rate )
{
	char *base = (char *) TIMER_BASE;
	int i;

	timer_rate = rate;

	timer_hook = (vfptr) 0;
	net_timer_hook = (vfptr) 0;

	timer_wait = (struct thread *) 0;

#ifdef notdef
	val = getl ( base + TIMER_ID );
	printf ( "Timer id: %08x\n", val );
#endif

	timer_rate_set ( rate );

	/* start the timer with autoreload */
	putl ( 3, base + TIMER_CTRL );
}

/* Public entry point.
 */
int
tmr_rate_get ( void )
{
	return timer_rate;
}

/* Public entry point.
 * Set a different timer rate
 */
void
tmr_rate_set ( int hz )
{
	timer_irqdis ();
	timer_rate_set ( hz );
	timer_irqena ();
}

void
timer_test ( void )
{
	int i;
	int val, st;
	char *base = (char *) TIMER_BASE;

	for ( i=0; i<20; i++ ) {
	    val = getl ( base + TIMER_COUNT );
	    st = getl ( base + TIMER_IRQ_SR );
	    printf ( "Timer: %08x %08x\n", val, st );
	    if ( st )
		putl ( TIMER_OVF, base + TIMER_IRQ_ST );
	    delay10 ();
	    delay10 ();
	}
}

void
timer_irqena ( void )
{
	char *base = (char *) TIMER_BASE;
	putl ( TIMER_OVF, base + TIMER_ENA );
}

void
timer_irqdis ( void )
{
	char *base = (char *) TIMER_BASE;
	putl ( TIMER_OVF, base + TIMER_DIS );
}

void
timer_irqack ( void )
{
	char *base = (char *) TIMER_BASE;
	putl ( TIMER_OVF, base + TIMER_IRQ_ST );
}

void
timer_int ( void )
{
	struct thread *tp;
	static int subcount;

#ifdef SPECIAL
#define SPECIAL
	printf ( "timer_int sp = %08x\n", get_sp () );
	printf ( "timer_int sr = %08x\n", get_cpsr () );
	printf ( "timer_int ssp = %08x\n", get_ssp () );
	printf ( "timer_int sr = %08x\n", get_cpsr () );
	printf ( "timer_int sp = %08x\n", get_sp () );
	spin ();
#endif

	++jiffies;

	/* old baloney - really the count
	 * would do (if we chose to export it).
	 * Accessing these externally is tacky,
	 *  and folks should make method calls
	 *  for timer facilities....
	 * XXX maybe ditch these counts
	 */
	++timer_count_t;
	++subcount;
	if ( (subcount % timer_rate) == 0 ) {
	    ++timer_count_s;
	}

	if ( ! cur_thread )
	    panic ( "timer, cur_thread" );

	++cur_thread->prof;

	in_interrupt = 1;
	if ( timer_wait ) {
	    --timer_wait->delay;
	    while ( timer_wait->delay == 0 ) {
		tp = timer_wait;
		timer_wait = timer_wait->wnext;
		if ( thread_debug ) {
		    printf ( "Remove wait: %s\n", tp->name );
		}
	    	thr_unblock ( tp );
	    }
	}

	if ( timer_hook ) {
	    (*timer_hook) ();
	}

	if ( net_timer_hook ) {
	    (*net_timer_hook) ();
	}

	/* -------------------------------------------
	 * From here down should become the epilog
	 * of every interrupt routine that could
	 * possibly do a thr_unblock()
	 *
	 * XXX - should this just get moved into
	 * the wrapper that calls this then?
	 * Hard to do if we call resume_i when done.
	 */

	/* By this time a couple of things could
	 * make us want to return to a different
	 * thread than what we were running when
	 * the interrupt happened.
	 *
	 * First, when we call the timer hook, the
	 * user routine could unblock a semaphore,
	 * or unblock a thread, or some such thing.
	 *
	 * Second, when we unblock a thread on the
	 * delay list, we may have a hot new candidate.
	 */

	if ( in_newtp ) {
	    tp = in_newtp;
	    in_newtp = (struct thread *) 0;
	    in_interrupt = 0;
	    change_thread_int ( tp );
	    /* NOTREACHED */

	    panic ( "timer, change_thread" );
	}

	in_interrupt = 0;
	resume_i ( &cur_thread->iregs );
	/* NOTREACHED */

	/* XXX - just disabled during ARM port */
	panic ( "timer, resume" );
}

/* maintain a linked list of folks waiting on
 * timer delay activations.
 * In time-honored fashion, the list is kept in
 * sorted order, with the soon to be scheduled
 * entries at the front.  Each tick then just
 * needs to decrement the leading entry, and
 * when it becomes zero, one or more entries
 * get launched.
 *
 * Important - this must be called with
 * interrupts disabled !
 */
void
timer_add_wait ( struct thread *tp, int delay )
{
	struct thread *p, *lp;

	p = timer_wait;

	while ( p && p->delay <= delay ) {
	    delay -= p->delay;
	    lp = p;
	    p = p->wnext;
	}

	if ( p )
	    p->delay -= delay;

	tp->delay = delay;
	tp->wnext = p;

	if ( p == timer_wait )
	    timer_wait = tp;
	else
	    lp->wnext = tp;
	/*
	printf ( "Add wait: %d\n", delay );
	*/
}

/* THE END */
