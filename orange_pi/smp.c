/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * cpu.c for the Orange Pi PC and PC Plus
 *
 * Tom Trebisky  1/19/2017
 *
 */

#define CPUCFG_BASE     0x01f01c00
#define PRCM_BASE       0x01f01400

/* The CPUCFG registers are in a 1K section starting at 0x01f01c00
 * The ROM_START register is in here and is undocumented in the H3 datasheet.
 * See page 143 in the datasheet.
 * The overall address map is on page 84
 */

// #define ROM_START       ((volatile unsigned long *) 0x01f01da4)
#define ROM_START        ((volatile unsigned long *) (CPUCFG_BASE + 0x1A4))

#define GEN_CTRL        ((volatile unsigned long *) (CPUCFG_BASE + 0x184))

// #define DBG_CTRL1       ((unsigned long *) (CPUCFG_BASE + 0x1e4))

/* These two are suggestive */

/* This has one bit, set low to assert it */
#define CPU_SYS_RESET      ((volatile unsigned long *) (CPUCFG_BASE + 0x140))

/* This comes up set to 0x10f after reset they say,
 * and that is exactly how I find it.
 * I can set it to 0x101 and everything still runs fine
 * However, if I set it to 0x102, the system hangs,
 *  so I think I understand (and can ignore) this register.
 */
#define CPU_CLK_GATE       ((volatile unsigned long *) (CPUCFG_BASE + 0x144))
#define GATE_L2		0x100
#define GATE_CPU0	0x01
#define GATE_CPU1	0x02
#define GATE_CPU2	0x04
#define GATE_CPU3	0x08

#define POWER_OFF       ((unsigned long *) (PRCM_BASE + 0x100))

/* When any ARM cpu starts running, it will set the PC to 0xffff0000
 * This is the location of the H3 bootrom code.
 * This code does clever things for a second core.
 * In particular, it reads the processor affinity register and
 *  discovers that the low 2 bits are non-zero (the core number).
 * Once it discovers this, it loads the PC from the value at 0x01f01da4
 *
 * Also as a side note on the bootrom.  The data sheet says various
 *  contradictory things about it (such as it being 32K, 64K, and 96K)
 * What I discover is that it is in fact a 32K image at ffff0000
 * There is an exact second copy immediately following at ffff8000
 * This is probably a redundant address decode, but who knows.
 */

#ifdef notyet
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
#endif

/* These are in locore.S */
extern void newcore ( void );
extern long statcore;

#ifdef notdef
void
// launch_core ( int cpu, vfptr who )
launch_core ( int cpu )
{
        unsigned long *reset;
        unsigned long *clamp;
        unsigned long *enab;
        unsigned long mask = 1 << cpu;
        int i;
	unsigned long val;

        reset = (volatile unsigned long *) ( CPUCFG_BASE + 0x40 + cpu * 0x40);   /* reset */
        clamp = (volatile unsigned long *) ( PRCM_BASE + 0x140 + cpu * 4);       /* power clamp */
        enab = (volatile unsigned long *) ( PRCM_BASE + 0x10 + cpu * 4);         /* power clamp */

        // bounce = who;

        *ROM_START = (unsigned long) newcore;  /* in locore.S */

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */

        // *DBG_CTRL1 &= ~mask;            /* sun6i - disable external debug */
        // *enab = 0xffffffff;
        *enab = 0;

        for (i = 0; i <= 8; i++)        /* sun6i - power clamp */
            *clamp = 0xff >> i;

        *POWER_OFF &= ~mask;            /* power on */

        // delay_ms ( 2 );                 /* delay at least 1 ms */
	thr_delay ( 2 );

        *reset = 3;			/* take out of reset */
        // *DBG_CTRL1 |= mask;             /* sun6i - reenable external debug */
}
#endif

void
launch_core ( int cpu )
{
        volatile unsigned long *reset;
        unsigned long mask = 1 << cpu;
	// unsigned long val;

        reset = (volatile unsigned long *) ( CPUCFG_BASE + (cpu+1) * 0x40);
	// printf ( "-- reset = %08x\n", reset );

	// printf ( "-- ROM = %08x\n", *ROM_START );
        *ROM_START = (unsigned long) newcore;  /* in locore.S */
	// printf ( "-- ROM = %08x\n", *ROM_START );

        *reset = 0;                     /* put core into reset */

        *GEN_CTRL &= ~mask;             /* reset L1 cache */
        *POWER_OFF &= ~mask;            /* power on */
	thr_delay ( 2 );

        *reset = 3;			/* take out of reset */
}

// #define SENTINEL	ROM_START
#define SENTINEL	(volatile unsigned long *) 4

void
watch_core ( void )
{
	volatile unsigned long *sent;
	int i;

	sent = SENTINEL;

	printf ( "Sentinel addr: %08x\n", sent );

	for ( i=0; i<11; i++ ) {
	    thr_delay ( 1000 );
	    printf ( "Core status: %08x\n", *sent );
	}
}

void
test_core ( void )
{
	unsigned long val;

#ifdef notdef
	val = *CPU_CLK_GATE;
	printf ( "Gate register: %08x\n", val );
	*CPU_CLK_GATE = 0x101;
	val = *CPU_CLK_GATE;
	printf ( "Gate register: %08x\n", val );
#endif

	launch_core ( 1 );

	watch_core ();
}

/*
 * So far doing this just hangs the processor ...
 */
void
main_reset ( void )
{
	int cpu = 0;
        volatile unsigned long *reset;

#ifdef notdef
	// There are two bits in this register, both asserted by writing zeros.
	// This does not work
        // reset = (unsigned long *) ( CPUCFG_BASE + 0x40 + cpu * 0x40);   /* reset */
	// *reset = 0;

	// This does not work either
	reset = CPU_SYS_RESET;
	*reset = 0;
#endif
	printf ( "Sorry ...\n");

	test_core ();

}

/* If all goes well, we will be running here */

void
kyu_newcore ()
{
	volatile unsigned long *sent;
	int val = 0;

	sent = SENTINEL;

	for ( ;; ) {
	    *sent = val++;
	}
}

/* THE END */
