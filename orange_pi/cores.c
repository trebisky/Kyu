/* Simple program to start a second core on the Orange Pi PC
 * Tom Trebisky  1-20-2017
 */

#ifdef ORIG
#define GPIO_A    0
#define GPIO_B    1
#define GPIO_C    2
#define GPIO_D    3
#define GPIO_E    4
#define GPIO_F    5
#define GPIO_G    6
#define GPIO_H    7
#define GPIO_I    8
#define GPIO_L    9	/* R_PIO */

struct h3_gpio {
	volatile unsigned long config[4];
	volatile unsigned long data;
	volatile unsigned long drive[2];
	volatile unsigned long pull[2];
};

/* In theory each gpio has 32 pins, but they are actually populated like so.
 */
// static int gpio_count[] = { 22, 0, 19, 18, 16, 7, 14, 0, 0, 12 };

static struct h3_gpio * gpio_base[] = {
    (struct h3_gpio *) 0x01C20800,		/* GPIO_A */
    (struct h3_gpio *) 0x01C20824,		/* GPIO_B */
    (struct h3_gpio *) 0x01C20848,		/* GPIO_C */
    (struct h3_gpio *) 0x01C2086C,		/* GPIO_D */
    (struct h3_gpio *) 0x01C20890,		/* GPIO_E */
    (struct h3_gpio *) 0x01C208B4,		/* GPIO_F */
    (struct h3_gpio *) 0x01C208D8,		/* GPIO_G */
    (struct h3_gpio *) 0x01C208FC,		/* GPIO_H */
    (struct h3_gpio *) 0x01C20920,		/* GPIO_I */

    (struct h3_gpio *) 0x01F02c00,		/* GPIO_L */
};

/* Only A, G, and R can interrupt */

/* GPIO pin function config (0-7) */
#define GPIO_INPUT        (0)
#define GPIO_OUTPUT       (1)
#define H3_GPA_UART0      (2)
#define H5_GPA_UART0      (2)
#define GPIO_DISABLE      (7)

/* GPIO pin pull-up/down config (0-3)*/
#define GPIO_PULL_DISABLE	(0)
#define GPIO_PULL_UP		(1)
#define GPIO_PULL_DOWN		(2)
#define GPIO_PULL_RESERVED	(3)

/* There are 4 config registers,
 * each with 8 fields of 4 bits.
 */
void
gpio_config ( int gpio, int pin, int val )
{
	struct h3_gpio *gp = gpio_base[gpio];
	int reg = pin / 8;
	int shift = (pin & 0x7) * 4;
	int tmp;

	tmp = gp->config[reg] & ~(0xf << shift);
	gp->config[reg] = tmp | (val << shift);
}

/* There are two pull registers,
 * each with 16 fields of 2 bits.
 */
void
gpio_pull ( int gpio, int pin, int val )
{
	struct h3_gpio *gp = gpio_base[gpio];
	int reg = pin / 16;
	int shift = (pin & 0xf) * 2;
	int tmp;

	tmp = gp->pull[reg] & ~(0x3 << shift);
	gp->pull[reg] = tmp | (val << shift);
}

void
gpio_output ( int gpio, int pin, int val )
{
	struct h3_gpio *gp = gpio_base[gpio];

	if ( val )
	    gp->data |= 1 << pin;
	else
	    gp->data &= ~(1 << pin);
}

int
gpio_input ( int gpio, int pin )
{
	struct h3_gpio *gp = gpio_base[gpio];

	return (gp->data >> pin) & 1;
}

/* These are registers in the CCM (clock control module)
 */
#define CCM_GATE	((unsigned long *) 0x01c2006c)
#define CCM_RESET4	((unsigned long *) 0x01c202d8)

#define GATE_UART0	0x0001000
#define GATE_UART1	0x0002000
#define GATE_UART2	0x0004000
#define GATE_UART3	0x0008000

#define RESET4_UART0	0x0001000
#define RESET4_UART1	0x0002000
#define RESET4_UART2	0x0004000
#define RESET4_UART3	0x0008000

/* This is probably set up for us by U-boot,
 * true bare metal would need this.
 */
void
uart_clock_init ( void )
{
	*CCM_GATE |= GATE_UART0;
	*CCM_RESET4 |= RESET4_UART0;
}

#define CHIP_H3

void
uart_gpio_init ( void )
{
#ifdef CHIP_H3
	gpio_config ( GPIO_A, 4, H3_GPA_UART0 );
	gpio_config ( GPIO_A, 5, H3_GPA_UART0 );
	gpio_pull ( GPIO_A, 5, GPIO_PULL_UP );

#else	/* H5 */
	gpio_config ( GPIO_A, 4, SUN50I_H5_GPA_UART0 );
	gpio_config ( GPIO_A, 5, SUN50I_H5_GPA_UART0 );
	gpio_pull ( GPIO_A, 5, GPIO_PULL_UP );
#endif
}

void
led_init ( void )
{
	gpio_config ( GPIO_L, 10, GPIO_OUTPUT );
	gpio_config ( GPIO_A, 15, GPIO_OUTPUT );
}

void
led_on ( void )
{
	gpio_output ( GPIO_L, 10, 1 );
}

void
led_off ( void )
{
	gpio_output ( GPIO_L, 10, 0 );
}

void
status_on ( void )
{
	gpio_output ( GPIO_A, 15, 1 );
}

void
status_off ( void )
{
	gpio_output ( GPIO_A, 15, 0 );
}
#endif /* ORIG */

#define BAUD_115200    (0xD) /* 24 * 1000 * 1000 / 16 / 115200 = 13 */

#define NO_PARITY      (0)
#define ONE_STOP_BIT   (0)
#define DAT_LEN_8_BITS (3)
#define LC_8_N_1       (NO_PARITY << 3 | ONE_STOP_BIT << 2 | DAT_LEN_8_BITS)

struct h3_uart {
	volatile unsigned int data;	/* 00 */
	volatile unsigned int ier;	/* 04 */
	volatile unsigned int iir;	/* 08 */
	volatile unsigned int lcr;	/* 0c */
	int _pad;
	volatile unsigned int lsr;	/* 04 */
};

#define dlh	ier
#define dll	data

#define UART0_BASE	0x01C28000
#define UART_BASE	((struct h3_uart *) UART0_BASE)

#define TX_READY	0x40

static void
uart_init ( void )
{
	struct h3_uart *up = UART_BASE;

	uart_gpio_init();
	uart_clock_init();

	up->lcr = 0x80;		/* select dll dlh */

	up->dlh = 0;
	up->dll = BAUD_115200;

	up->lcr = LC_8_N_1;
}

static void
uart_putc ( char c )
{
	struct h3_uart *up = UART_BASE;

	while ( !(up->lsr & TX_READY) )
	    ;
	up->data = c;
}

static void
uart_puts ( char *s )
{
	while ( *s ) {
	    if (*s == '\n')
		uart_putc('\r');
	    uart_putc(*s++);
	}
}

/* ========================================= */

#ifdef notdef
/* A reasonable delay for blinking an LED */
void
delay_x ( void )
{
	volatile int count = 50000000;

	while ( count-- )
	    ;
}

/* Blink red status light */
void
blink_red ( void )
{
	for ( ;; ) {
	    status_on ();
	    delay_x ();

	    status_off ();
	    delay_x ();
	}
}

void
blink_green ( void )
{
	for ( ;; ) {
	    led_off ();
	    delay_x ();

	    led_on ();
	    delay_x ();
	}
}
#endif

/* ========================================= */

static void
delay_ms ( int msecs )
{
	volatile int count = 100000 * msecs;

	while ( count-- )
	    ;
}

#define CPUCFG_BASE 	0x01f01c00
#define PRCM_BASE   	0x01f01400

// #define ROM_START	((volatile unsigned long *) 0x01f01da4)
#define ROM_START        ((volatile unsigned long *) (CPUCFG_BASE + 0x1A4))

#define GEN_CTRL	((volatile unsigned long *) (CPUCFG_BASE + 0x184))
#define POWER_OFF	((volatile unsigned long *) (PRCM_BASE + 0x100))

/* In start.S */
extern void newcore ( void );
extern void oldcore ( void );

static unsigned long mycore;

/* Proven to work */
static void
launch_core2 ( int cpu )
{
	volatile unsigned long *reset; 
	unsigned long mask = 1 << cpu;

	reset = (volatile unsigned long *) ( CPUCFG_BASE + (cpu+1) * 0x40);

	// *ROM_START = (unsigned long) newcore;	/* in start.S */
        mycore =  (unsigned long) oldcore;  /* in locore.S */
        *ROM_START = mycore;

	*reset = 0;			/* put core into reset */

	*GEN_CTRL &= ~mask;		/* reset L1 cache */
	*POWER_OFF &= ~mask;		/* power on */
	delay_ms ( 10 );		/* delay at least 1 ms */

	*reset = 3;
}

static void
launch_core ( int cpu )
{
        volatile unsigned long *reset;
        unsigned long mask = 1 << cpu;

        reset = (volatile unsigned long *) ( CPUCFG_BASE + (cpu+1) * 0x40);
        // printf ( "-- reset = %08x\n", reset );

        mycore =  (unsigned long) oldcore;  /* in locore.S */
        *ROM_START = mycore;

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */
        *POWER_OFF &= ~mask;            /* power on */
        // thr_delay ( 2 );
	delay_ms ( 10 );		/* delay at least 1 ms */

        *reset = 3;                     /* take out of reset */
}

// #define uart_puts printf

/* Proven to work */
void
check_core_orig ( void )
{
	int count;

	launch_core2 ( 1 );

	count = 10;
	while ( count-- ) {
	    delay_ms ( 1000 );
	    if ( *ROM_START != mycore )
		break;
	    uart_puts ( "No\n" );
	}

	if ( *ROM_START != mycore )
	    uart_puts ( "Yes!\n" );
	else
	    uart_puts ( "Can't do it !!\n" );
}

#define SCTRL_A         0x0002
static void
enable_unaligned ( void )
{
        int scr;

        asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (scr) : : "cc");

        scr &= ~SCTRL_A;

        asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (scr) : "cc");
}

/* Works when called off Kyu test menu */
void
core_main ( void )
{
	enable_unaligned ();
	uart_init();

	// led_init ();

	uart_puts("\n" );
	uart_puts("Try to start second core:\n");

	check_core_orig ();

	/* spin */
	for ( ;; )
	    ;
}

/* Called from Kyu test menu */
void
check_core ( void )
{
	// check_core_orig ();
	core_main ();
}

/* THE END */
