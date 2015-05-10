/* Taken from U-boot /arch/arm/lib/interrupts.c
 * and trimmed for use in Kyu
 * 4-30-2015  U-boot 2015.01
 */

/*
 * (C) Copyright 2003
 * Texas Instruments <www.ti.com>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002-2004
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * (C) Copyright 2004
 * Philippe Robin, ARM Ltd. <philippe.robin@arm.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifdef notdef
#include <common.h>
#include <asm/proc-armv/ptrace.h>
#include <asm/u-boot-arm.h>
#endif

#include <kyu.h>
#include <ptrace.h>

#define STACKSIZE_IRQ 4096

/* These were in arch/arm/include/asm/u-boot-arm.h
 * tjt
 */
extern unsigned long  IRQ_STACK_START;
extern unsigned long  IRQ_STACK_START_IN;
extern unsigned long  FIQ_STACK_START;

/* tjt for Kyu */
extern void *vectors;

void
interrupt_init ( void )
{
	unsigned long irq_sp = IRQ_STACK_BASE;	/* in kyu.h */

	IRQ_STACK_START = irq_sp - 4;
        IRQ_STACK_START_IN = irq_sp + 8;
        FIQ_STACK_START = IRQ_STACK_START - STACKSIZE_IRQ;

	/*
	printf ( "My vectors at: %08x\n", &vectors );
	*/

	set_vbar ( &vectors );
}

volatile int val;

void
interrupt_test ( void )
{
#ifdef notdef
	/* generate a data abort */
	int *p = (int *) 0;
	volatile int x = *p;
#endif
	int i;

	val = 9;
	for ( i=5; i > -10; i-- )
	    val /= i;
}

#ifdef notdef
void bad_mode (void)
{
	panic ("Resetting CPU ...\n");
	reset_cpu (0);
}
#endif

void bad_mode (void) {}

/* copied here by tjt */
#define pc_pointer(v) \
         ((v) & ~PCMASK)

#define instruction_pointer(regs) \
        (pc_pointer((regs)->ARM_pc))

void show_regs (struct pt_regs *regs)
{
	unsigned long flags;
	const char *processor_modes[] = {
	"USER_26",	"FIQ_26",	"IRQ_26",	"SVC_26",
	"UK4_26",	"UK5_26",	"UK6_26",	"UK7_26",
	"UK8_26",	"UK9_26",	"UK10_26",	"UK11_26",
	"UK12_26",	"UK13_26",	"UK14_26",	"UK15_26",
	"USER_32",	"FIQ_32",	"IRQ_32",	"SVC_32",
	"UK4_32",	"UK5_32",	"UK6_32",	"ABT_32",
	"UK8_32",	"UK9_32",	"HYP_32",	"UND_32",
	"UK12_32",	"UK13_32",	"UK14_32",	"SYS_32",
	};

	flags = condition_codes (regs);

	printf ("pc : [<%08lx>]	   lr : [<%08lx>]\n"
		"sp : %08lx  ip : %08lx	 fp : %08lx\n",
		instruction_pointer (regs),
		regs->ARM_lr, regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	printf ("r10: %08lx  r9 : %08lx	 r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9, regs->ARM_r8);
	printf ("r7 : %08lx  r6 : %08lx	 r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6, regs->ARM_r5, regs->ARM_r4);
	printf ("r3 : %08lx  r2 : %08lx	 r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2, regs->ARM_r1, regs->ARM_r0);
	printf ("Flags: %c%c%c%c",
		flags & CC_N_BIT ? 'N' : 'n',
		flags & CC_Z_BIT ? 'Z' : 'z',
		flags & CC_C_BIT ? 'C' : 'c', flags & CC_V_BIT ? 'V' : 'v');
	printf ("  IRQs %s  FIQs %s  Mode %s%s\n",
		interrupts_enabled (regs) ? "on" : "off",
		fast_interrupts_enabled (regs) ? "on" : "off",
		processor_modes[processor_mode (regs)],
		thumb_mode (regs) ? " (T)" : "");
}

void do_undefined_instruction (struct pt_regs *pt_regs)
{
	printf ("undefined instruction\n");
	show_regs (pt_regs);
	bad_mode ();
}

void do_software_interrupt (struct pt_regs *pt_regs)
{
	printf ("software interrupt\n");
	show_regs (pt_regs);
	bad_mode ();
}

void do_prefetch_abort (struct pt_regs *pt_regs)
{
	printf ("prefetch abort\n");
	show_regs (pt_regs);
	bad_mode ();
}

void do_data_abort (struct pt_regs *pt_regs)
{
	printf ("data abort\n");
	show_regs (pt_regs);
	bad_mode ();
}

void do_not_used (struct pt_regs *pt_regs)
{
	printf ("not used\n");
	show_regs (pt_regs);
	bad_mode ();
}

void do_fiq (struct pt_regs *pt_regs)
{
	printf ("fast interrupt request\n");
	show_regs (pt_regs);
	bad_mode ();
}

#include "omap_ints.h"

void do_irq ( void )
{
	int n = intcon_irqwho ();

	if ( n == NINT_TIMER0 ) {
	    timer_irqack ();
	    timer_int ();
	    return;
	}

	if ( n == NINT_UART0 ) {
	    serial_irqack ();
	    serial_int ();
	    return;
	}

	printf ("interrupt request: %d\n", n);
	/*
	show_regs (pt_regs);
	*/
	bad_mode ();
}

/* THE END */
