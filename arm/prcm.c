/* prcm.c
 *
 * Tom Trebisky  Kyu project  5-11-2015
 * Tom Trebisky  Kyu project  8-23-2016
 *
 * Driver for PRCM stuff on the am3359
 */

/*
 * PRCM is "power, reset, and clock management
 */

#define PER_BASE      0x44E00000
#define WKUP_BASE     0x44E00400
#define PRM_BASE      0x44E00F00

/* A module on the am3359 is something like a single uart,
 *  i2c controller, or the PRU.
 * Each module has a "clock control register".
 * In general, after reset, every device is in standby
 *  and disabled (it reads out 0x70000).
 * Attempting to access its registers when
 *  it is in this state will yield data abort exceptions.
 */

#define	MODULE_STBY_STATUS	0x00040000
#define	MODULE_IDLE_STATUS	0x00030000
#define	MODULE_MODE		0x00000003

#define MODULE_MODE_DISABLE	0x0
#define MODULE_MODE_ENABLE	0x2

#define MODULE_IDLE_RUN		0x00000000
#define MODULE_IDLE_CHANGE	0x00010000	/* in transition */
#define MODULE_IDLE_IDLE	0x00020000
#define MODULE_IDLE_DISABLED	0x00030000

#define	MODULE_STBY_RUN		0x00000000
#define	MODULE_STBY_STBY	0x00040000

/* The following registers are fairly homogenous, but ...
 *  The registers with names ending in "_st" are quite different.
 *  Not all registers have the standby status bit.
 */

struct cm_perpll {
        volatile unsigned int l4ls_st;    /* offset 0x00 */
        volatile unsigned int l3s_st;     /* offset 0x04 */
        volatile unsigned int l4fw_st;    /* XXX - offset 0x08 */
        volatile unsigned int l3_st;      /* offset 0x0c */
        unsigned int _pad1;
        volatile unsigned int cpgmac0;    /* offset 0x14 */
        volatile unsigned int lcd;        /* offset 0x18 */
        volatile unsigned int usb0;       /* offset 0x1C */
        unsigned int _pad2;			/* offset 0x20 */
        volatile unsigned int tptc0;      /* offset 0x24 */
        volatile unsigned int emif;       /* offset 0x28 */
        volatile unsigned int ocmcram;    /* offset 0x2c */
        volatile unsigned int gpmc;       /* offset 0x30 */
        volatile unsigned int mcasp0;     /* offset 0x34 */
        volatile unsigned int uart5;      /* offset 0x38 */
        volatile unsigned int mmc0;       /* offset 0x3C */
        volatile unsigned int elm;        /* offset 0x40 */
        volatile unsigned int i2c2;       /* offset 0x44 */
        volatile unsigned int i2c1;       /* offset 0x48 */
        volatile unsigned int spi0;       /* offset 0x4C */
        volatile unsigned int spi1;       /* offset 0x50 */
        unsigned int _pad3[3];
        volatile unsigned int l4ls;       /* offset 0x60 */
        volatile unsigned int l4fw;       /* offset 0x64 */
        volatile unsigned int mcasp1;     /* offset 0x68 */
        volatile unsigned int uart1;      /* offset 0x6C */
        volatile unsigned int uart2;      /* offset 0x70 */
        volatile unsigned int uart3;      /* offset 0x74 */
        volatile unsigned int uart4;      /* offset 0x78 */
        volatile unsigned int timer7;     /* offset 0x7C */
        volatile unsigned int timer2;     /* offset 0x80 */
        volatile unsigned int timer3;     /* offset 0x84 */
        volatile unsigned int timer4;     /* offset 0x88 */
        unsigned int _pad4[8];
        volatile unsigned int gpio1;      /* offset 0xAC */
        volatile unsigned int gpio2;      /* offset 0xB0 */
        volatile unsigned int gpio3;      /* offset 0xB4 */
        unsigned int _pad5;
        volatile unsigned int tpcc;       /* offset 0xBC */
        volatile unsigned int dcan0;      /* offset 0xC0 */
        volatile unsigned int dcan1;      /* offset 0xC4 */
        unsigned int _pad6;
        volatile unsigned int epwmss1;    /* offset 0xCC */
        volatile unsigned int emiffw;     /* offset 0xD0 */
        volatile unsigned int epwmss0;    /* offset 0xD4 */
        volatile unsigned int epwmss2;    /* offset 0xD8 */
        volatile unsigned int l3instr;    /* offset 0xDC */
        volatile unsigned int l3;         /* Offset 0xE0 */
        volatile unsigned int ieee5000;   /* * Offset 0xE4 */
        volatile unsigned int pru;        /* * Offset 0xE8 */
        volatile unsigned int timer5;     /* offset 0xEC */
        volatile unsigned int timer6;     /* offset 0xF0 */
        volatile unsigned int mmc1;       /* offset 0xF4 */
        volatile unsigned int mmc2;       /* offset 0xF8 */
        volatile unsigned int tptc1;      /* * offset 0xFC */
        volatile unsigned int tptc2;      /* * offset 0x100 */
        unsigned int _pad9[2];
        volatile unsigned int spinlock;   /* * offset 0x10C */
        volatile unsigned int mailbox0;   /* * offset 0x110 */
        unsigned int _pad10[2];
        volatile unsigned int l4hs_st;    /* offset 0x11C */
        volatile unsigned int l4hs;       /* offset 0x120 */
        unsigned int _pad11[2];
        volatile unsigned int ocpwp_l3_st; /* offset 0x12c */
        volatile unsigned int ocpwp;      /* offset 0x130 */
        unsigned int _pad12[3];
        volatile unsigned int pru_st;     /* offset 0x140 */
        volatile unsigned int cpsw_st;    /* offset 0x144 */
        volatile unsigned int lcdc_st;    /* offset 0x148 */
        volatile unsigned int clkdiv32k;  /* offset 0x14c */
        volatile unsigned int clk_24m_st; /* offset 0x150 */
};


void
module_enable ( volatile unsigned int *mp )
{
	int timeout = 1000000;

	printf ( "Module &clk = %08x\n", mp );
	printf ( "Module clk = %08x\n", *mp );
	*mp = ((*mp) & ~MODULE_MODE) | MODULE_MODE_ENABLE;
	printf ( "Module clk = %08x\n", *mp );

	/* Now wait for results */
	while ( timeout-- ) {
	    if ( (*mp & MODULE_IDLE_STATUS) == MODULE_IDLE_RUN )
		break;
	    delay_ns ( 1000 );	/* XXX */
	}

	printf ( "Module clk = %08x\n", *mp );

	if ( timeout <= 0 )
	    printf ("Cannot enable module\n" );
	    // panic ("Cannot enable module" );
}

/* The following does NOT work.
 * You take the PRU out of standby from code
 * running in the PRU (this is often the first
 * thing a piece of PRU code does, via:
 * LBCO r0, C4, 4, 4
 * CLR r0, r0, 4
 * SBCO r0, C4, 4, 4
 * The standby status bit should then reflect
 * that this has been done.
 */

#ifdef notdef
/* Take a module out of standby */
void
module_run ( volatile unsigned int *mp )
{
	int timeout = 1000000;

	printf ( "Standby clk = %08x\n", *mp );

	*mp &= ~MODULE_STBY_STBY;

	/* Now wait for results */
	while ( timeout-- ) {
	    if ( (*mp & MODULE_STBY_STATUS) == 0 )
		break;
	    delay_ns ( 1000 );	/* XXX */
	}

	printf ( "STandby clk = %08x\n", *mp );
	if ( timeout <= 0 )
	    printf ("Cannot take module out of standby\n" );
}
#endif

/* XXX - where does this really belong ? */
static void
pru_module_init ( void )
{
	struct cm_perpll *pp;

	pru_reset_module ();
	pp = (struct cm_perpll *) PER_BASE;
	module_enable ( &pp->pru );
}

void
modules_init ( void )
{
	struct cm_perpll *pp;

	pp = (struct cm_perpll *) PER_BASE;

	printf ( "Enable i2c1\n" );
	module_enable ( &pp->i2c1 );
	printf ( "Enable i2c2\n" );
	module_enable ( &pp->i2c2 );

	printf ( "Enable PRU\n" );
	pru_module_init ();
}

void
clocks_init ( void )
{
}

/* Provide a way for software to reset the processor
 */
struct prm {
	volatile unsigned long rstctl;
	volatile unsigned long rsttime;
	volatile unsigned long rstst;
	volatile unsigned long sram_count;
	volatile unsigned long ldo_sram_core_setup;
	volatile unsigned long ldo_sram_core_ctrl;
	volatile unsigned long ldo_sram_mpu_setup;
	volatile unsigned long ldo_sram_mpu_ctrl;
};

/* The difference between warm and cold is unclear.
 * The TRM says that if you do a cold reset, you
 *  "must ensure the SDRAM is properly put in sef-refresh
 *   mode before applying this reset"
 */
#define	WARM_RESET	0x01
#define	COLD_RESET	0x02

void
reset_cpu ( void )
{
	struct prm *base = (struct prm *) PRM_BASE;

	base->rstctl = WARM_RESET;
}

/* THE END */
