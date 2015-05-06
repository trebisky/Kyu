/* timer.c
 *
 * Simple driver for the DM timer
 * see section 20 of the am3359 TRM
 * especially control register
 *   description on pg 4113
 * 
 */

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

/* May not need these, but here they are */
#define getb(a)          (*(volatile unsigned char *)(a))
#define getw(a)          (*(volatile unsigned short *)(a))
#define getl(a)          (*(volatile unsigned int *)(a))

#define putb(v,a)        (*(volatile unsigned char *)(a) = (v))
#define putw(v,a)        (*(volatile unsigned short *)(a) = (v))
#define putl(v,a)        (*(volatile unsigned int *)(a) = (v))

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
timer_init ( int rate )
{
	int i;
	unsigned long val;
	char *base = (char *) TIMER_BASE;

#ifdef notdef
	val = getl ( base + TIMER_ID );
	printf ( "Timer id: %08x\n", val );
#endif

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

	/* start the timer with autoreload */
	putl ( 3, base + TIMER_CTRL );

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

/* THE END */
