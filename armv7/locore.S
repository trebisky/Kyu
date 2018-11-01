/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * locore.S
 *
 * Kyu project  5-12-2015  Tom Trebisky
 *
 *  Multicore code by Marco Warga, Philip Michael Van Every, and myself.
 *
 * temporary catch-all for all kinds of assembly language
 *
 * Amazingly enough, ARM by default is little-endian.
 * It can switch, but watch out if you do!
 *
 * Transition to multicore  6-15-2018  Tom Trebisky
 *
 */

#include "board/board.h"

@ Apparently the at-sign is the ARM comment character.
@ on the x86 you use the pound sign.
@ You can also use C style comments including //

/* We always run in SVR (supervisor) mode */
#define PSR_M_SYS	0x1F	/* system */
#define PSR_M_UDI	0x1B	/* undefined instruction */
#define PSR_M_ABT	0x17	/* abort */
#define PSR_M_MON	0x16	/* monitor */
#define PSR_M_SVR	0x13	/* supervisor */
#define PSR_M_IRQ	0x12
#define PSR_M_FIQ	0x11
#define PSR_M_USER	0x10

#define PSR_IRQ_DIS	0x80
#define PSR_FIQ_DIS	0x40
#define PSR_INT_DIS	0xc0

#define PSR_DEFAULT	PSR_M_SVR

#define CPSR_MODE_USER			0x10
#define CPSR_MODE_FIQ			0x11
#define CPSR_MODE_IRQ			0x12
#define CPSR_MODE_SVR			0x13
#define CPSR_MODE_ABORT			0x17
#define CPSR_MODE_HYP			0x1A
#define CPSR_MODE_UNDEFINED		0x1B
#define CPSR_MODE_SYSTEM		0x1F

#define CPSR_IRQ_INHIBIT		0x80
#define CPSR_FIQ_INHIBIT		0x40

/* In an effort to make this code more readable,
 * I am introducing these get/set macros for the
 * various ARM control registers.
 */

/* ARM system control register */
#define read_SCTLR(reg)		MRC p15, 0, reg, c1, c0, 0
#define write_SCTLR(reg)	MCR p15, 0, reg, c1, c0, 0

/* apparently this is an alternate way of doing the above, not tested */
#define read_SCTLR_alt(reg)	MRS reg, SCTLR_EL1
#define write_SCTLR_alt(reg)	MSR SCTLR_EL1, reg

/* Auxiliary control register */
#define read_ACTLR(reg)		MRC p15, 0, reg, c1, c0, 1
#define write_ACTLR(reg)	MCR p15, 0, reg, c1, c0, 1

/* Coprocessor access control register */
#define read_CACR(reg)		MRC p15, 0, reg, c1, c0, 2
#define write_CACR(reg)		MCR p15, 0, reg, c1, c0, 2

/* vector base address register */
#define read_VBAR(reg)		MRC p15, 0, reg, c12, c0, 0
#define write_VBAR(reg)		MCR p15, 0, reg, c12, c0, 0

/* multiprocessor ID register (processor affinity register) */
#define read_MPID(reg)		MRC p15, 0, reg, c0, c0, 5

/* c7 is a bunch of cache specific operations */
// invalidate I cache
#define write_I_INV(reg)	MCR p15, 0, reg, c7, c5, 0

/* c8 is a bunch of tlb specific operations */
// invalidate both I and D cache
#define write_TLB_INV(reg)	MCR p15, 0, reg, c8, c7, 0

#define CTLR_MMU		1
#define CTLR_ALIGN		2
#define CTLR_D_CACHE		4
#define CTLR_I_CACHE		0x1000

/* ----------------------------------

/* For the time being, this file is the first thing linked
 * and U-boot simply branches to a hard address.
 *  0x80000000 on the BBB
 *  0x40000000 on the Orange Pi
 *
 * As long as locore.o is the first thing linked,
 * we should be OK.
 *
 * For some reason the compiler was reordering routines
 * compiled from main.c, this makes us immune to that.
 */

/* Providing this symbol here is really unnecessary, but it gives us
 * a way to tell the linker what to use as a start address in the ELF file.
 * The only purpose this serves is to keep the linker quiet, but
 * that is worth something, perhaps.
 */
	.globl asm_startup
asm_startup:

/* Secondary cores will start up here.
 */
.globl	secondary_start
secondary_start:
// Just fall through

/* This is the 6-14-2018 hack on the standard Kyu startup */
/* Working 6-15-2018 for main and extra cores */

primary_start:

#ifdef BOARD_BBB

	b primary_start_bbb

#endif


	// switch to SVR mode and disable interrupts
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr cpsr, r0

	// set the spsr to a known value
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr spsr_cxsf, r0
	mrs r0, spsr

	// clear this bit to
	// allow unaligned data access
	read_SCTLR ( r0 )
	bic      r0, r0, #CTLR_ALIGN
	write_SCTLR ( r0 )

	/* core zero gets standard vectors */
        read_MPID ( r11 )
        and     r11, r11, #0x03
        cmp     r11, #0
        bne     1f
	    movw	r0, #:lower16:vectors
	    movt	r0, #:upper16:vectors
	    write_VBAR ( r0 )
	    b		2f
1:
	    movw	r0, #:lower16:alt_vectors
	    movt	r0, #:upper16:alt_vectors
	    write_VBAR ( r0 )
2:

@	movw	r0, #:lower16:vectors
@	movt	r0, #:upper16:vectors
@	write_VBAR ( r0 )

	bl	setup_stacks_kyu

	/* Only core zero should zero BSS */
        read_MPID ( r11 )
        and     r11, r11, #0x03
        cmp     r11, #0
        bne     1f
	    bl	clear_bss
	    dsb
1:

        // enable SMP -- near as I can tell this sets
	// the IBE bit, which enables invalidate all
	// instructions.
	//
        // (this needs to be done before the MMU is enabled)
	read_ACTLR ( r0 )
        bic     r0, r0, #0x040
	write_ACTLR ( r0 )

	// if enabled, flush D cache
	// (this needs to be done before disabling the MMU)
	read_SCTLR ( r0 )
	and     r0, r0, #CTLR_D_CACHE
	cmp     r0, #0
	beq     1f
	    bl armv7_dcache_wbinv_all
1:

	// disable I cache, D cache and MMU
        // (we want to set up everything from scratch)
	// May as well set the whole register to zero.
@	read_SCTLR ( r0 )
@       bic     r0, #CTLR_I_CACHE
@       bic     r0, #CTLR_D_CACHE
@       bic     r0, #CTLR_MMU
	mov	r0, #0x0
	write_SCTLR ( r0 )
        isb

	/* Only core zero should setup page tables */
        read_MPID ( r11 )
        and     r11, r11, #0x03
        cmp     r11, #0
        bne     1f
	    bl	board_mmu_init
1:

	// invalidate I cache
	mov	r1, #0x0
	write_I_INV ( r1 )

	// Reenable I cache
	// (it works without MMU enabled)
	mov	r1, #CTLR_I_CACHE
	write_SCTLR ( r1 )
	isb

	bl	mmu_set_ttbr

	// invalidate TLB for both I and D
	mov r0, #1
	write_TLB_INV ( r0 )
	isb
	dsb

	// enable MMU
	read_SCTLR ( r0 )
        orr     r0, #CTLR_MMU
	write_SCTLR ( r0 )
	isb
	dsb

	// invalidate and enable D cache
	// MMU must be enabled for this to work
	bl armv7_dcache_l1inv_all

	read_SCTLR ( r0 )
        orr     r0, #CTLR_D_CACHE
	write_SCTLR ( r0 )

        read_MPID ( r0 )
        and     r0, r0, #0x03
        cmp     r0, #0
        beq     2f
	    // r0 holds core number,
	    // which becomes argument to kyu_newcore
	    // bl	spin_alt
	    bl	kyu_newcore
1:	    b	1b
2:
	// start Kyu
	bl	kyu_startup

// spin if we return
1:	b	1b

clear_bss:
	movw	r3, #:lower16:__bss_start__
	movt	r3, #:upper16:__bss_start__
	movw	r2, #:lower16:__bss_end__
	movt	r2, #:upper16:__bss_end__
	mov	r1, #0
loop:	cmp	r3, r2
	strcc	r1, [r3]
	addcc	r3, r3, #4
	bcc	loop
	bx	lr

/* We have 6 modes -- and each needs a stack.
 * (we ignore USER and MONITOR)
 * See also board.h
 * They are in this order,
 *  these are offsets from the end of the core region.
 */
#define SVR_STACK         MODE_STACK_SIZE * 5
#define IRQ_STACK         MODE_STACK_SIZE * 4
#define FIQ_STACK         MODE_STACK_SIZE * 3
#define SYSTEM_STACK      MODE_STACK_SIZE * 2
#define ABORT_STACK       MODE_STACK_SIZE * 1
#define UNDEFINED_STACK   MODE_STACK_SIZE

setup_stacks_kyu:
	// fetch core number into r11
	read_MPID ( r11 )
	and r11, r11, #0x03
	
	// calc offset for this core
	ldr r2,=STACK_PER_CORE
	mul r0, r2, r11

	// ldr r1,=STACK_ADDR
	// sub r4, r1, r0
	ldr	r1, =core_stacks
	ldr	r1, [r1]
	add	r4, r1, r0

	// stacks start at end of region
	add r4, r4, r2

	// switch to interrupt mode, set sp
	mov r0, #(CPSR_MODE_IRQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #IRQ_STACK

	// switch to fiq mode, set sp
	mov r0, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #FIQ_STACK

	// switch to undefined exception mode, set sp
	mov r0, #(CPSR_MODE_UNDEFINED | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #UNDEFINED_STACK

	// switch to data abort exception mode, set sp
	mov r0, #(CPSR_MODE_ABORT | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #ABORT_STACK

	// switch to user mode, set sp
	mov r0, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #SYSTEM_STACK

	// switch to SVR mode, set sp
	mov r0, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #SVR_STACK

	bx lr

/* This is the old standard Kyu startup,
 *  with some tweaks so we can use it for the BBB,
 *  yet with the multicore framework.
 * This works with the BBB (tested 6-15-2018)
 * XXX - the above multicore startup also works, BUT ...
 *  the BBB runs slow (this startup is fine).
 *  Both spin timed delays and timer timed delays run slow with
 *  the multicore startup.
 */
primary_start_bbb:
	mrs	r0, cpsr		/* disable interrupts */
	orr	r0, r0, #PSR_INT_DIS	/* both IRQ and FIQ */
	msr	cpsr, r0

	// bug fix 11-1-2018
	// -- somehow this got left out of the BBB initialization.
	//
	// clear this bit to
	// allow unaligned data access
	read_SCTLR ( r0 )
	bic      r0, r0, #CTLR_ALIGN
	write_SCTLR ( r0 )

	movw	r0, #:lower16:vectors
	movt	r0, #:upper16:vectors
	write_VBAR ( r0 )

	bl	clear_bss
	dsb

	bl	board_mmu_init
	dsb

	bl	mmu_set_ttbr

	bl	kyu_startup

// spin if we return
1:	b	1b

#ifdef notdef
/*------------------------------------------------------------------------
 * spin_lock / spin_unlock  -  spinlocks using ARM exclusive monitors
 * From Xinu code by Phil Van Every
 * 1 is locked, 0 is unlocked
 *------------------------------------------------------------------------ */

.text
.globl spin_lock
.globl spin_unlock

spin_lock:

1:	ldrex	r2, [r0]	/* spin until unlocked */
        cmp	r2, #0
        beq	1b

	mov	r1, #1
        strexne	r2, r1, [r0]	/* Attempt to lock it */
        cmpne	r2, #1
        beq	1b		/* Failed - retry */

        /* Lock acquired */
        dmb
        bx lr

spin_unlock:
	mov	r1, #0
        dmb

        str r1, [r0]
        bx lr
#endif

/* ----------------------------------------------------------- */
/*  Exception and interrupt vectors */
/* ----------------------------------------------------------- */

/* on the BBB, the reset vector will never come here, it will start the
 * firmware ROM code running, which will transfer to the SPL, which will
 * transfer to U-boot, which will finally transfer to us.
 *
 * Someday we could eliminate U-boot, although I don't know why,
 * and just let the SPL load us directly.
 * At any rate, the reset vector here won't get any use
 *  given this code is launched from U-Boot, or in the
 *  case of a second core, the reset vector goes to the
 *  bootrom, not here in the H3.
 */

/* Note on ".align".  On some processors (like the x86, not the arm)
 * this is the multiple of bytes to align on.  On the ARM it is the
 * power of 2.  So on the ARM .align 3 is like .align 8 on the x86.
 * On the ARM .align 5 is aligning on 32 byte boundaries.
 *
 * I am probably being overly fussy about alignment,
 *  but maybe I will thank myself when I move to 64 bit ARM processors.
 */

	.align  5
.globl vectors
vectors:
	b	reset
	ldr	pc, _undef
	ldr	pc, _swi
	ldr	pc, _iabt
	ldr	pc, _dabt
	ldr	pc, _unused
	ldr	pc, _irq
	ldr	pc, _fiq

/* An experiment, let other cores have a separate table, for some testing.
 */
	.align  5
.globl vectors
alt_vectors:
	b	reset
	ldr	pc, _undef
	ldr	pc, _swi
	ldr	pc, _iabt
	ldr	pc, _dabt
	ldr	pc, _unused
	ldr	pc, _irq_alt
	ldr	pc, _fiq


	/* This branch table lets the handlers be anywhere */
	.globl	_undef
	.globl	_swi
	.globl	_iabt
	.globl	_dabt
	.globl	_unused
	.globl	_irq
	.globl	_fiq

	.globl	_irq_alt

_undef:		.word undefined_instruction
_swi:		.word software_interrupt
_iabt:		.word prefetch_abort
_dabt:		.word data_abort
_unused:	.word not_used
_irq:		.word irq
_fiq:		.word fiq

_irq_alt:	.word irq_alt

/* XXX - spin and hang */
reset:
        b      reset

#define S_R0		0
#define S_R1		4
#define S_R2		8
#define S_R3		12
#define S_R4		16
#define S_R5		20
#define S_R6		24
#define S_R7		28
#define S_R8		32
#define S_R9		36
#define S_R10		40
#define S_R11		44	/* fp */
#define S_R12		48	/* ip */
#define S_SP		52	/* r13 */
#define S_LR		56	/* r14 */
#define S_PC		60	/* r15 */

#define S_PSR		64

#define FRAME_SIZE	68

/* ----------------------------------------------------------- */
/*  Kyu context switching and such */
/* ----------------------------------------------------------- */

	.globl	cur_thread
	.globl	static_thread

/* When an interrupt happens we have our own private r13 and r14 (sp and lr)
 * r14 has the address to return to (plus some adjustment),
 *  we can do as we please with sp (as long as we don't care about
 *  the interrupt stack it now points to).
 *
 * So, we start the prolog with an adjusted PC in the lr register.
 * we simply use sp as a scratch register (and lr as well later on).
 *
 * Using the "^" to get the "non-interrupt" registers would be a mistake.
 *  this gets the user mode registers, which we have no interest in.
 *
 * The MPID register reads all zeros on a Cortex-A8 (as per the BBB).
 */

cthread:	.word	0

	.macro kyu_prolog
@	movw	sp, #:lower16:cur_thread
@	movt	sp, #:upper16:cur_thread
// This works in lieu of the above.
	ldr     sp, =cur_thread
	ldr	sp, [sp, #0]

2:
	stmia	sp, {r0-r12}

	mrs	ip, spsr	/* save before forcing to super */
	str	ip, [sp,#S_PSR]	/* saved SR */

	add	r0, sp, #0	/* our cur_thread */
	add	r1, lr, #0	/* shuffle this (becomes PC on resume) */

@	mrs     ip, cpsr	/* get current mode */
@       bic     ip, ip, #0x1f
@       orr     ip, ip, #PSR_M_SVR
@       msr     cpsr, ip	/* force it to super */
	// this replaces the above 4 instructions
        cps     #PSR_M_SVR

	str	sp, [r0,#S_SP]	/* save this */
	str	lr, [r0,#S_LR]	/* this too */

	str	r1, [r0,#S_PC]	/* PC when we resume */

	.endm

	/* This is the tried and true prolog from days before
	 * we added code to handle interrupts in any core.
	 * 6-18-2018
	 */
	.macro kyu_prolog_OLD
	movw	sp, #:lower16:cur_thread
	movt	sp, #:upper16:cur_thread
	ldr	sp, [sp, #0]

	stmia	sp, {r0-r12}

	mrs	ip, spsr	/* save before forcing to super */
	str	ip, [sp,#S_PSR]	/* saved SR */

	add	r0, sp, #0	/* our cur_thread */
	add	r1, lr, #0	/* shuffle this (becomes PC on resume) */

	mrs     ip, cpsr	/* get current mode */
        bic     ip, ip, #0x1f
        orr     ip, ip, #PSR_M_SVR
        msr     cpsr, ip	/* force it to super */

	str	sp, [r0,#S_SP]	/* save this */
	str	lr, [r0,#S_LR]	/* this too */

	str	r1, [r0,#S_PC]	/* PC when we resume */

	.endm

	/* The way things are currently coded, we never use the epilog.
	 * We always return from interrupts using resume_i()
	 *  or some other specific resume method.
	 */
	.macro kyu_epilog
	b	spin
	.endm

	/* We get a pointer to a thread in r0
	 * XXX XXX - Note that turning on both IRQ and FIQ is wrong
	 *  and will yields bugs when we distinguish FIQ.
	 */
	.globl resume_i
resume_i:
	/* keep r0 till bitter end */
	ldr	ip, [r0, #S_PSR]
	msr	spsr, ip

	ldmia	r0, {r0-pc}^

/* This routine launches a thread from scratch.
 *  on the ARM it gets a pointer to an array of 4 items:
 *	an argument to pass
 *	a place to start up at.
 *	an initial stack pointer.
 *	a place to return to (usually thr_exit)
 * XXX danger if this is used from interrupt as we
 *  are not doing sp and lr properly
 */
	.globl resume_c
resume_c:
	ldmia	r0,{r0,ip,sp,lr}
	mrs     r1, cpsr
	bic	r1, r1, #0xff
	orr	r1, r1, #PSR_M_SVR	/* enable both IRQ and FIQ */
	msr	cpsr, r1
	mov	fp, #0		/* added 8-13-2016 to end backtraces */
	mov	pc, ip

/* resume_j and save_j don't need to save all registers
 * since these are called "synchronously", the compiler
 * conventions should apply, and r0-r3 should not need
 * to be saved.  Also ip and maybe others.
 * But it is simple to save them all .....
 * 
 * *** resume_j must be capable of being called from
 *      interrupt level.
 */

	/* This is always called from supervisor level,
	 * so we don't need to fuss with sp and lr.
	 */
	.globl save_j
save_j:
	stmia	r0, {r0-lr}
	mrs     ip, cpsr
	str	ip, [r0, #S_PSR]
	mov	ip, #1		/* resume will return 1 */
	str	ip, [r0]	/* coincidence this points to r0 */
	str	lr, [r0,#S_PC]	/* resume at this pc */
	mov	r0, #0		/* we return 0 */
	bx	lr

/* Now identical to resume_i */
	.globl resume_j
resume_j:
	/* keep r0 till bitter end */
	ldr	ip, [r0, #S_PSR]
	msr	spsr, ip

	ldmia	r0, {r0-pc}^

/*
 * exception handlers
 * XXX - do these really need to be align 5,
 *  or just the vector table itself??
 */
	.align  5
undefined_instruction:
	kyu_prolog
	bl	do_undefined_instruction
	kyu_epilog

	.align	5
software_interrupt:
	kyu_prolog
	bl	do_software_interrupt
	kyu_epilog

	.align	5
prefetch_abort:
	sub	lr, lr, #4
	kyu_prolog
	bl	do_prefetch_abort
	kyu_epilog

	.align	5
data_abort:
	sub	lr, lr, #8
	kyu_prolog
	bl	do_data_abort
	kyu_epilog

	.align	5
not_used:
	kyu_prolog
	bl	do_not_used
	kyu_epilog

	.align	5
irq:
	sub	lr, lr, #4
	kyu_prolog
	bl	do_irq
	kyu_epilog

	.align	5
fiq:
	sub	lr, lr, #4
	kyu_prolog
	bl	do_fiq
	kyu_epilog

/* This is the Xinu way of entering an exception handler */
/* This fails if we do not do the cps to SVR mode, why? */
/* It works fine, but does not save the state Kyu needs
 * to allow thread switching during interrupts */
	.align	5
irq_alt_XINU:
	sub	lr, lr, #4
	srsdb	sp!, #PSR_M_SVR
	cps	#PSR_M_SVR
	push	{r0-r12,lr}
	bl	do_irq_alt
	pop	{r0-r12,lr}
	rfeia	sp!

# This is "trusting" and does not allow nested interrupts.
# It builds a 68 byte "frame" on the interrupt stack and
# passes it as a parameter, but does not maintain the
# stack pointer pointing to this frame, so any other
# activity on the IRQ stack will trash it.
# --
# The alternative to this is to use cps to jump back to
# IRQ mode and free the space on the stack after the
# call to C code returns.
	.align	5
irq_alt:
	sub	lr, lr, #4	@ enter in IRQ mode
	sub	sp, sp, #FRAME_SIZE
	stmia	sp, {r0-r12}
	mrs	r0, spsr
	str	r0, [sp,#S_PSR]

	add	r0, sp, #0	@ pointer to frame we are building
	add	sp, sp, #FRAME_SIZE
	add	r2, lr, #0	@ pc

	cps	#PSR_M_SVR	@ permanent transition to SVR mode
	str	sp, [r0, #S_SP]
	str	lr, [r0, #S_LR]
	str	r2, [r0, #S_PC]
	push	{r0}

	bl	do_irq_alt

	pop	{r0}
	ldr	r1, [r0, #S_PSR]
	msr	spsr, r1
	ldmia	r0, {r0-pc}^
	// the ending ^ (caret) in the above says that if the register
	// list includes the PC (as it does), then the spsr is also
	// copied into the cpsr, which is just the thing for exception
	// handlers.
	// Once we make the marked transition to SVR mode,
	// we never return to IRQ mode.

/* ----------------------------------------------------------- */
/* Various handy things */
/* ----------------------------------------------------------- */

        .globl get_regs
get_regs:
	stmia	r0, {r0-pc}
	mrs     ip, cpsr
	str	ip, [r0,#S_PSR]
	bx	lr

#define FLOATING_POINT
#ifdef FLOATING_POINT
	.globl fp_enable
fp_enable:
	read_CACR ( r0 )
	orr	r0, r0, #0x300000 @ single precision
	orr	r0, r0, #0xC00000 @ double precision
	write_CACR ( r0 )
	isb
	mov	r0, #0x40000000
	fmxr	fpexc,r0
	mov	pc, lr
#endif

/* Call this to hang the processor in a spin loop */
	.globl spin
spin:
	b	spin

/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */

/* This is how Xinu does it, returns a mask to later
 * be passed to restore(m);
 * This pair is used in the Xinu TCP code.
 */
	.globl disable
disable:
        mrs     r0, cpsr        /* Copy the CPSR into r0                */
        cpsid   i               /* Disable interrupts                   */
        mov     pc, lr          /* Return the CPSR                      */

	.globl restore
restore:
        push    {r1, r2}        /* Save r1, r2 on stack                 */
        mrs     r1, cpsr        /* Copy CPSR into r1                    */
        ldr     r2, =0x01F00220
        and     r1, r1, r2      /* Extract flags and other important    */
        bic     r0, r0, r2      /*    bits from the mask                */
        orr     r1, r1, r0
        msr     cpsr_cfsx, r1   /* Restore the CPSR                     */
        pop     {r1, r2}        /* Restore r1, r2                       */
        mov     pc, lr          /* Return to caller                     */

/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */

#ifdef notdef
/* For an experiment from interrupt code */
        .globl get_ssp
get_ssp:
	mrs	ip, cpsr
	bic	r0, ip, #0x1f
	orr	r0, r0, #PSR_M_SVR
	msr	cpsr, r0

	add	r0, sp, #0

	msr	cpsr, ip
	bx	lr

xdelay:
	mov	r0, #0xFFFFFF
dloop:	sub	r0, #1
	cmp	r0, #0
	bgt	dloop
	bx	lr

/* spit out endless characters -- sanity check */
	.globl sanity
sanity:
	movw	r0, #'b'
	bl	serial_putc
	bl	xdelay
	b	sanity
#endif

#ifdef BOARD_ORANGE_PI

.globl go_red
.globl spin_red
.globl spin_alt

// Turn on RED led on Orange Pi
go_red:
	ldr	r12,=0x01c20810
	ldr	r11,=0xffffffff
	str	r11, [r12]
	bx	lr

// turn on RED led and spin
spin_red:
	ldr	r12,=0x01c20810
	ldr	r11,=0xffffffff
	str	r11, [r12]
1:	b	1b

// return unless alt core
// if so, turn on RED led and spin
spin_alt:
        read_MPID ( r11 )
        and     r11, r11, #0x03
        cmp     r11, #0
        bne     1f
	bx	lr

1:	ldr	r12,=0x01c20810
	ldr	r11,=0xffffffff
	str	r11, [r12]
1:	b	1b

#endif /* BOARD_ORANGE_PI */

/* THE END */
