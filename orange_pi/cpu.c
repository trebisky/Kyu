#define CPUCFG_BASE     0x01f01c00
#define PRCM_BASE       0x01f01400

#define ROM_START       ((unsigned long *) 0x01f01da4)
#define GEN_CTRL        ((unsigned long *) (CPUCFG_BASE + 0x184))
#define DBG_CTRL1       ((unsigned long *) (CPUCFG_BASE + 0x1e4))

#define POWER_OFF       ((unsigned long *) (PRCM_BASE + 0x100))

typedef void (*vfptr) ( void );

void
launch ( vfptr who )
{
        (*who) ();
}

static vfptr bounce;

void
bounce_core ( void )
{
        (*bounce) ();
}

#ifdef notyet
extern void new_core ( void );

void
launch_core ( int cpu, vfptr who )
{
        unsigned long *reset;
        unsigned long *clamp;
        unsigned long *enab;
        unsigned long mask = 1 << cpu;
        int i;

        reset = (unsigned long *) ( CPUCFG_BASE + 0x40 + cpu * 0x40);   /* reset */
        clamp = (unsigned long *) ( PRCM_BASE + 0x140 + cpu * 4);       /* power clamp */
        enab = (unsigned long *) ( PRCM_BASE + 0x10 + cpu * 4);         /* power clamp */

        bounce = who;

        // *ROM_START = (unsigned long) new_core;  /* in start.S */

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */
        *DBG_CTRL1 &= ~mask;            /* sun6i - disable external debug */
        // *enab = 0xffffffff;
        *enab = 0;

        for (i = 0; i <= 8; i++)        /* sun6i - power clamp */
            *clamp = 0xff >> i;

        *POWER_OFF &= ~mask;            /* power on */
        delay_ms ( 2 );                 /* delay at least 1 ms */

        *reset = 3;
        *DBG_CTRL1 |= mask;             /* sun6i - reenable external debug */
}
#endif

void
main_reset ( void )
{
	int cpu = 0;
        unsigned long *reset;

        reset = (unsigned long *) ( CPUCFG_BASE + 0x40 + cpu * 0x40);   /* reset */
	*reset = 0;
}

/* THE END */
