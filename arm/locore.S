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
 * temporary catch-all for assembly language
 *
 * Amazingly enough, ARM by default is little-endian.
 * It can switch, but watch out if you do!
 */

#include "board/board.h"

@ Apparently the at-sign is the ARM comment character.
@ on the x86 you use the pound sign.

#define PSR_M_SUPER	0x13
#define PSR_M_IRQ	0x12
#define PSR_M_FIQ	0x11

#define PSR_IRQ_DIS	0x80
#define PSR_FIQ_DIS	0x40
#define PSR_INT_DIS	0xc0

#define PSR_DEFAULT	PSR_M_SUPER

/* For the time being, this is the first thing linked
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
 * place to tell the linker to use as a start address in the ELF file.
 * The only purpose this serves is to keep the linker quiet, but
 * that is worth something.
 */
	.globl asm_startup
asm_startup:

	b primary_start

/* Secondary cores will start up here.
 */
.globl	secondary_start
secondary_start:
	b xinu_core

/* This is the old standard Kyu startup */
primary_start:
	mrs	r0, cpsr		/* disable interrupts */
	orr	r0, r0, #PSR_INT_DIS
	msr	cpsr, r0

	movw	r0, #:lower16:vectors	/* set VBAR */
	movt	r0, #:upper16:vectors
	mcr 	p15, 0, r0, c12, c0, 0

	bl	clear_bss
	dsb

@	bl	paging_init_XINU	// Xinu multicore
@	dsb

	bl	kern_startup
@	bl	test_core
xspin:	b	xspin
@	bx	lr

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

xinu_core:
start_core:
	// first switch to SVR mode and disable interrupts
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr cpsr, r0

	// set the spsr to a known value
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr spsr_cxsf, r0
	mrs r0, spsr

	// allow unaligned data access
	mrc      p15, 0, r0, c1, c0, 0
	bic      r0, r0, #2
	mcr      p15, 0, r0, c1, c0, 0

	// if enabled, flush D cache
	// (this needs to be done before disabling the MMU)
	mrc     p15, 0, r0, c1, c0, 0
	and     r0, r0, #(1 << 2)
	cmp     r0, #0
	beq     1f
		bl armv7_dcache_wbinv_all

1:
	// disable I cache, D cache and MMU
	// (we want to set up everything from scratch)
	mrc p15, 0, r0, c1, c0, 0
	bic r0, #0x00001000		// I cache
	bic r0, #0x00000004		// D cache
	bic r0, #0x00000001		// MMU
	mcr p15, 0, r0, c1, c0, 0
	isb

	// enable SMP
	// (this needs to be done before the MMU is enabled)
	mrc p15, 0, r0, c1, c0, 1
	bic r0, r0, #0x040
	mcr p15, 0, r0, c1, c0, 1

	// set up stacks for all modes of this core (Xinu method)
	// bl setup_stacks

	// setup stack for this core (Kyu method)
	// Read processor affinity register
        mrc     p15, 0, r0, c0, c0, 5
	and	r0, #3
	lsl	r0, #14
	ldr	r2, =core_stacks
	ldr	r2, [r2]
	add	sp, r2, r0

	// core 0 has to inititialize
	// some shared data
	mrc p15, 0, r11, c0, c0, 5
	and r11, r11, #0x03
	cmp r11, #0
	bne 2f
		/// zero BSS
@		ldr	r0, =edata
@		ldr	r1, =end
@		bl	bzero
@		dsb

		// create exception vector
@		bl initevec
@		dsb

		// create page table
@		bl paging_init
@		dsb

2:
	// set exception vector
@	ldr r0, =exp_vector
@	bl evec_set_addr

	// as per Kyu main core
	movw	r0, #:lower16:vectors	/* set VBAR */
	movt	r0, #:upper16:vectors
	mcr 	p15, 0, r0, c12, c0, 0

	// enable I cache
	// (it works without MMU enabled)
	mov r1, #0x0
	mcr p15, 0, r1, c7, c5, 0	// invalidate I cache
	mov  r1, #0x1000
	mcr p15, 0, r1, c1, c0, 0	// enable I cache
	isb

	// set page table address in TTBR0/TTBR1
	// ldr r0, =page_table
	// dsb
@	bl mmu_set_ttbr_XINU
@	isb

	// set DACR to manager level for all domains
@	ldr r0, =0xffffffff
@	bl mmu_set_dacr

	// invalidate TLB and enable the MMU
	mov r0, #1
	mcr p15, 0, r0, c8, c7, 0
	isb

	mrc p15,0,r0,c1,c0,0
	orr r0,r0,#1
	mcr p15,0,r0,c1,c0,0
	isb
	dsb

	// invalidate and enable the D cache
	// (it needs an enabled MMU to work)
	bl armv7_dcache_l1inv_all
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, #0x00000004
	mcr	p15, 0, r0, c1, c0, 0

	// jump to C code
	// find out what core is starting
	mrc p15, 0, r11, c0, c0, 5
	and r11, r11, #0x03
	cmp r11, #0
	bne 3f
		// start core0
		bl	kern_startup
3:
	// other cores
	/* pass core number in r0 as argument */
        mrc     p15, 0, r0, c0, c0, 5
	and	r0, #3

	bl	kyu_newcore

// should never return here
yspin:	b	yspin
	bx lr

#ifdef XINU_STARTUP

#include <armv7a.h>

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

.text

/*------------------------------------------------------------------------
 * start  -  Initial entry point for a Xinu image (ARM) for primary core
 *------------------------------------------------------------------------
 */
.globl start
start:
	b start_core_XINU

/*------------------------------------------------------------------------
 * secondary_start  -  entry point for secondary cores
 *------------------------------------------------------------------------
 */
.globl	secondary_start
secondary_start:
	b start_core_XINU

/*------------------------------------------------------------------------
 * start_core  -  core initialization
 *------------------------------------------------------------------------
 */
start_core_XINU:
	// first switch to SVR mode and disable interrupts
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr cpsr, r0

	// set the spsr to a known value
	mov r0, #(CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR)
	msr spsr_cxsf, r0
	mrs r0, spsr

	// allow unaligned data access
	mrc      p15, 0, r0, c1, c0, 0
	bic      r0, r0, #2
	mcr      p15, 0, r0, c1, c0, 0

	// if enabled, flush D cache
	// (this needs to be done before disabling the MMU)
	mrc     p15, 0, r0, c1, c0, 0
	and     r0, r0, #(1 << 2)
	cmp     r0, #0
	beq     1f
		bl armv7_dcache_wbinv_all

1:
	// disable I cache, D cache and MMU
	// (we want to set up everything from scratch)
	mrc p15, 0, r0, c1, c0, 0
	bic r0, #0x00001000		// I cache
	bic r0, #0x00000004		// D cache
	bic r0, #0x00000001		// MMU
	mcr p15, 0, r0, c1, c0, 0
	isb

	// enable SMP
	// (this needs to be done before the MMU is enabled)
	mrc p15, 0, r0, c1, c0, 1
	bic r0, r0, #0x040
	mcr p15, 0, r0, c1, c0, 1

	// set up stacks for all modes of this core
	bl setup_stacks

	// core 0 has to inititialize
	// some shared data
	mrc p15, 0, r11, c0, c0, 5
	and r11, r11, #0x03
	cmp r11, #0
	bne 2f
		/// zero BSS
		ldr	r0, =edata
		ldr	r1, =end
		bl	bzero
		dsb

		// create exception vector
		bl initevec
		dsb

		// create page table
		bl paging_init
		dsb

2:
	// set exception vector
	ldr r0, =exp_vector
	bl evec_set_addr

	// enable I cache
	// (it works without MMU enabled)
	mov r1, #0x0
	mcr p15, 0, r1, c7, c5, 0	// invalidate I cache
	mov  r1, #0x1000
	mcr p15, 0, r1, c1, c0, 0	// enable I cache
	isb

	// set page table address in TTBR0/TTBR1
	ldr r0, =page_table
	dsb
	bl mmu_set_ttbr
	isb

	// set DACR to manager level for all domains
	ldr r0, =0xffffffff
	bl mmu_set_dacr

	// invalidate TLB and enable the MMU
	mov r0, #1
	mcr p15, 0, r0, c8, c7, 0
	isb
	mrc p15,0,r0,c1,c0,0
	orr r0,r0,#1
	mcr p15,0,r0,c1,c0,0
	isb
	dsb

	// invalidate and enable the D cache
	// (it needs an enabled MMU to work)
	bl armv7_dcache_l1inv_all
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, #0x00000004
	mcr	p15, 0, r0, c1, c0, 0

	// jump to Xinu
	mrc p15, 0, r11, c0, c0, 5
	and r11, r11, #0x03
	cmp r11, #0
	bne 3f
		bl	nulluser	// core0
3:
	bl secondary_run	// the other cores

	bx lr

/*------------------------------------------------------------------------
 * Set up stacks for all modes for the current core
 *------------------------------------------------------------------------
 */
#define STACK_ADDR        MAXADDR
#define STACK_PER_MODE    0x8000
#define STACK_PER_CORE    STACK_PER_MODE * 6
#define STACK_TOTAL       STACK_PER_CORE * 4
#define SVR_STACK         STACK_PER_MODE
#define IRQ_STACK         STACK_PER_MODE * 1
#define FIQ_STACK         STACK_PER_MODE * 2
#define SYSTEM_STACK      STACK_PER_MODE * 3
#define ABORT_STACK       STACK_PER_MODE * 4
#define UNDEFINED_STACK   STACK_PER_MODE * 5

setup_stacks:
	// fetch core number into r11
	mrc p15, 0, r11, c0, c0, 5
	and r11, r11, #0x03

	// calc stack offset for this core
	ldr r1,=STACK_PER_CORE
	mul r0, r1, r11
	ldr r1,=STACK_ADDR
	sub r4, r1, r0

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

	// switch to SVC mode, set sp
	mov r0, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
	msr cpsr_c, r0
	sub sp, r4, #SVR_STACK

	bx lr

/*------------------------------------------------------------------------
 * Return the total size of all stacks
 *------------------------------------------------------------------------
 */
.globl get_stack_size
get_stack_size:
	mov r0, #STACK_TOTAL
	bx	lr

/*------------------------------------------------------------------------
 * Function to zero memory (r0 is lowest addr; r1 is highest)
 *------------------------------------------------------------------------
 */
bzero:
	mov	r2, #0					/* Round address to multiple	*/
	add	r0, r0, #3				/*   of four by adding 3 and	*/
	and	r0, r0, #0xFFFFFFFC		/*   taking the result module 4	*/
1:	cmp	r0, r1					/* Loop until last address	*/
	bhs	2f						/*   has been reached		*/
	str	r2, [r0]				/* Zero four-byte word of memory*/
	add	r0, r0, #4				/* Move to next word		*/
	b	1b						/* Continue to iterate		*/
2:	mov	pc, lr					/* Return to caller		*/
#endif /* XINU_STARTUP */

/*------------------------------------------------------------------------ */
/*------------------------------------------------------------------------ */
/*------------------------------------------------------------------------ */
/*------------------------------------------------------------------------ */

#ifdef XINU_EXTRAS
.globl get_ccnt
get_ccnt:
	mrc     p15, 0, r0, c9, c13, 0
	bx	lr

.globl set_ccnt
set_ccnt:
	mcr     p15, 0, r0, c9, c13, 0
	bx	lr

.globl get_pmcr
get_pmcr:
	mrc     p15, 0, r0, c9, c12, 0
	bx	lr

.globl set_pmcr
set_pmcr:
	mcr     p15, 0, r0, c9, c12, 0
	bx	lr

.globl get_prrr
get_prrr:
	mrc     p15, 0, r0, c10, c2, 0
	bx	lr

.globl get_nmrr
get_nmrr:
	mrc     p15, 0, r0, c10, c2, 1
	bx	lr

.globl get_idsar
get_idsar:
	mrc p15, 0, r0, c0, c2, 3
	bx lr

.globl get_cena
get_cena:
	mrc     p15, 0, r0, c9, c12, 1
	bx	lr

.globl set_cena
set_cena:
	mcr     p15, 0, r0, c9, c12, 1
	bx	lr

.globl get_cdis
get_cdis:
	mrc     p15, 0, r0, c9, c12, 2
	bx	lr

.globl get_sctlr
get_sctlr:
	mrc     p15, 0, r0, c1, c0, 0
	bx	lr

.globl get_actlr
get_actlr:
	mrc     p15, 0, r0, c1, c0, 1
	bx	lr

.globl get_ttbr0
get_ttbr0:
	mrc     p15, 0, r0, c2, c0, 0
	bx	lr

.globl get_ttbcr
get_ttbcr:
	mrc     p15, 0, r0, c2, c0, 2
	bx	lr

.globl get_scr
get_scr:
	mrc p15, 0, r0, c1, c1, 0
	bx	lr

/*------------------------------------------------------------------------
 * Page Table
 *------------------------------------------------------------------------
 */
.data
/* Page table base address must be aligned at 16 KB boundary */
	.align	14
	.globl page_table
page_table:
	.space	(0x1000 * 4)

/*------------------------------------------------------------------------
 * bputc	-	simple polled output, prints C to screen for assembly
				debugging when kputc won't work
 *------------------------------------------------------------------------
 */
#define UART0LSR 0x01C28014
#define UART0RBR 0x01C28000
#define TX_READY 0x40

.text
.globl bputc
bputc:
	ldr r0, =UART0LSR		/* point to uart line status reg */
	ldr r1, [r0]			/* get value of line status reg */
	and r1, r1, #TX_READY	/* mask tx ready bit bit */
	cmp r1, #TX_READY		/* check for bit set */
	bne bputc				/* keep poling until bit is set */
	ldr r0, =UART0RBR		/* point to TX register */
	mov r1, #67				/* decimal 67 is capitol C */
	str r1, [r0]
	isb
	dsb
	dmb
	mov pc, lr
#endif /* XINU_EXTRAS */

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

/* on the BBB, the reset vector will never come here, it will start the
 * firmware ROM code running, which will transfer to the SPL, which will
 * transfer to U-boot, which will finally transfer to us.
 *
 * Someday we could eliminate U-boot, although I don't know why,
 * and just let the SPL load us directly.
 * At any rate, the reset vector here won't get any use on the BBB.
 */

/* Note on ".align".  On some processors (like the x86, not the arm)
 * this is the multiple of bytes to align on.  On the ARM it is the
 * power of 2.  So on the ARM .align 3 is like .align 8 on the x86.
 * And so on the ARM .align 5 is aligning on 32 byte boundaries.
 *
 * I am probably being overly fussy about alignment,
 *  but maybe I will thank myself if I move to 64 bit ARM processors.
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


	/* This branch table lets the handlers be anywhere */
	.globl	_undef
	.globl	_swi
	.globl	_iabt
	.globl	_dabt
	.globl	_unused
	.globl	_irq
	.globl	_fiq

_undef:		.word undefined_instruction
_swi:		.word software_interrupt
_iabt:		.word prefetch_abort
_dabt:		.word data_abort
_unused:	.word not_used
_irq:		.word irq
_fiq:		.word fiq

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
#define S_R11		44
#define S_R12		48
#define S_SP		52	/* r13 */
#define S_LR		56	/* r14 */
#define S_PC		60	/* r15 */

#define S_PSR		64

#define FRAME_SIZE	68

	.globl	cur_thread

/* When an interrupt happens we have our own private r13 and r14 (sp and lr)
 * r14 has the address to return to (plus some adjustment),
 *  but sp is entirely free to be used.
 *
 * So, we start the prolog with an adjusted PC in the lr register.
 * we simply use sp as a scratch register (and lr as well later on).
 *
 * Using the "^" to get the "non-interrupt" registers would be a mistake.
 *  this gets the user mode registers, which we have no interest in.
 */
	.macro kyu_prolog
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
        orr     ip, ip, #PSR_M_SUPER
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

#ifdef notdef
OLDresume_i:
	/* keep r0 till bitter end */
	ldr	r1, [r0, #S_PSR]
	msr	spsr, r1
	ldr	r1, [r0, #S_SP]
	ldr	r2, [r0, #S_LR]

	mrs     ip, cpsr	/* save mode */
        bic     r3, ip, #0x1f
        orr     r3, r3, #PSR_M_SUPER	/* enable interrupts */
        msr     cpsr, r3	/* switch back to super */

	add	sp, r1, #0	/* setup SP and LR */
	add	lr, r2, #0
	msr	cpsr, ip

	ldr	lr, [r0, #S_PC]
	ldmia	r0, {r0-r12}
	movs	pc, lr
#endif

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
	orr	r1, r1, #PSR_M_SUPER	/* enable both IRQ and FIQ */
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

	.globl resume_j
resume_j:
	/* keep r0 till bitter end */
	ldr	ip, [r0, #S_PSR]
	msr	spsr, ip

	ldmia	r0, {r0-pc}^

#ifdef notdef
OLDresume_j:
	/* keep r0 till the bitter end */
	mrs     r1, cpsr
	bic	r1, r1, #0xff
	orr	r1, r1, #PSR_M_SUPER	/* enable interrupts */
	msr	spsr, r1

	ldr	r1, [r0, #S_SP]
	ldr	r2, [r0, #S_LR]

	mrs     ip, cpsr	/* save mode */
        bic     r3, ip, #0x1f
        orr     r3, r3, #PSR_M_SUPER	
        msr     cpsr, r3	/* switch back to super */

	add	sp, r1, #0	/* setup SP and LR */
	add	lr, r2, #0
	msr	cpsr, ip

	ldr	lr, [r0, #S_PC]
	ldmia	r0, {r0-r12}
	movs	pc, lr

VERYOLDresume_j:
	ldmia	r0, {r0-pc}
#endif

/*
 * exception handlers
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

/* ----------------------------------------- */
/* ----------------------------------------- */

        .globl get_sp
get_sp:
        add     r0, sp, #0
	bx	lr

        .globl get_fp
get_fp:
        add     r0, fp, #0
	bx	lr

/* Use only in dire situations */
        .globl set_sp
set_sp:
        add     sp, r0, #0
	bx	lr

        .globl get_regs
get_regs:
	stmia	r0, {r0-pc}
	mrs     ip, cpsr
	str	ip, [r0,#S_PSR]
	bx	lr

/* For an experiment from interrupt code */
        .globl get_ssp
get_ssp:
	mrs	ip, cpsr
	bic	r0, ip, #0x1f
	orr	r0, r0, #PSR_M_SUPER
	msr	cpsr, r0

	add	r0, sp, #0

	msr	cpsr, ip
	bx	lr

	.globl spin
spin:
	b	spin

        .globl get_pc
get_pc:
        add     r0, pc, #0
	bx	lr

        .globl get_cpsr
get_cpsr:
	mrs     r0, cpsr
	bx	lr

/* Read VBAR Register */

	.globl get_vbar
get_vbar:
	mrc 	p15, 0, r0, c12, c0, 0
	bx	lr

	.globl set_vbar
set_vbar:
	mcr 	p15, 0, r0, c12, c0, 0
	bx	lr

#ifdef notdef
/* SCTRL - System Control*/
	.globl get_sctrl
get_sctrl:
	mrc     p15, 0, r0, c1, c0, 0
	bx	lr

	.globl set_sctrl
set_sctrl:
	mcr 	p15, 0, r0, c1, c0, 0
	bx	lr
#endif

/* ASCTRL - Auxiliary System Control*/
	.globl get_asctrl
get_asctrl:
	mrc     p15, 0, r0, c1, c0, 1
	bx	lr

	.globl set_asctrl
set_asctrl:
	mcr 	p15, 0, r0, c1, c0, 1
	bx	lr

#ifdef notdef
/* MMU table base */
	.globl get_mmu
get_mmu:
	mrc     p15, 0, r0, c2, c0, 0
	bx	lr

	.globl set_mmu
set_mmu:
	mcr 	p15, 0, r0, c2, c0, 0
	bx	lr

/* Protection Unit base */
	.globl get_prot
get_prot:
	mrc     p15, 0, r0, c6, c0, 0
	bx	lr

	.globl set_prot
set_prot:
	mcr 	p15, 0, r0, c6, c0, 0
	bx	lr
#endif


/* The cycle count register is a 32 bit register
 * This is for a Cortex A8 arm chip.
 */
	.globl get_ccnt
get_ccnt:
	mrc     p15, 0, r0, c9, c13, 0
	bx	lr

	.globl set_ccnt
set_ccnt:
	mcr     p15, 0, r0, c9, c13, 0
	bx	lr

/* The performance monitor control register controls
 * the behavior of the CCNT register.
 */
	.globl get_pmcr
get_pmcr:
	mrc     p15, 0, r0, c9, c12, 0
	bx	lr

	.globl set_pmcr
set_pmcr:
	mcr     p15, 0, r0, c9, c12, 0
	bx	lr

/* Counter enable register.
 */
	.globl get_cena
get_cena:
	mrc     p15, 0, r0, c9, c12, 1
	bx	lr

	.globl set_cena
set_cena:
	mcr     p15, 0, r0, c9, c12, 1
	bx	lr

/* Counter disable register.
 */
	.globl get_cdis
get_cdis:
	mrc     p15, 0, r0, c9, c12, 2
	bx	lr

	.globl set_cdis
set_cdis:
	mcr     p15, 0, r0, c9, c12, 2
	bx	lr

/* Counter overflow register.
 */
	.globl get_covr
get_covr:
	mrc     p15, 0, r0, c9, c12, 3
	bx	lr

	.globl set_covr
set_covr:
	mcr     p15, 0, r0, c9, c12, 3
	bx	lr

/* XXX XXX - clean up this mess !!
 * we have way too many ways to disable/enable
 * processor interrupts.
 */

/* enable_irq (void); */
	.globl enable_irq
enable_irq:
	mrs	r0, cpsr
	@ bic	r0, r0, #0x80
	bic	r0, #0x80
	msr	cpsr, r0
	bx	lr

/* enable_fiq (void); */
	.globl enable_fiq
enable_fiq:
	mrs	r0, cpsr
	bic	r0, r0, #0x40
	msr	cpsr, r0
	bx	lr

/* enter critical region
 * i.e. disable both irq and fiq
 */
	.globl cpu_enter
cpu_enter:
	mrs	r0, cpsr
	orr	r0, r0, #0xc0
	msr	cpsr, r0
	bx	lr

/* leave critical region
 * i.e. enable both irq and fiq
 * XXX - do we really want to enable both?
 */
	.globl cpu_leave
cpu_leave:
	mrs	r0, cpsr
	bic	r0, r0, #0xc0
	msr	cpsr, r0
	bx	lr

/* This is how Xinu does it, returns a mask to later
 * be passed to restore(m);
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

#define FLOATING_POINT
#ifdef FLOATING_POINT
	.globl fp_enable
fp_enable:
	mrc	p15, 0, r0, c1, c0, 2
	orr	r0, r0, #0x300000 @ single precision
	orr	r0, r0, #0xC00000 @ double precision
	mcr	p15, 0, r0, c1, c0, 2
	isb
	mov	r0, #0x40000000
	fmxr	fpexc,r0
	mov	pc, lr
#endif

#ifdef notdef
/*------------------------------------------------------------------------
 * As per Xinu and Marco -- we declare the page_table
 *  here so we can conveniently force alignment.
 *------------------------------------------------------------------------
 */
.data
/* Page table base address must be aligned at 16 KB boundary */
        .align  14
        .globl page_table
page_table:
        .space  (0x1000 * 4)
#endif

/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */

/* Experimental section for Orange Pi multiple cores */

#ifdef BOARD_ORANGE_PI

#ifdef notdef
@ fails
core_startup:
        mrs     r0, cpsr                /* disable interrupts */
        orr     r0, r0, #PSR_INT_DIS
        msr     cpsr, r0

        movw    sp, #0
        movt    sp, #0x4fff
        bl      core_main
#endif

#ifdef notdef
/* We tried using this as a sentinel,
 * but caching hides the results.
 */
@	.globl statcore
@ statcore:
@ 	.word	0xdeadbeef

	.globl zcore
zcore:
	mov	r0, #3
	ldr	r1, =0x01f01da4
        str     r0, [r1]
	bx	lr
#endif

#define SENTINEL	0x4

	.globl newcore
newcore:
	/* Read processor affinity register */
        mrc     p15, 0, r0, c0, c0, 5
	and	r0, #3
	lsl	r0, #14
	ldr	r2, =core_stacks
	ldr	r2, [r2]
	add	sp, r2, r0

	/* pass core number in r0 as argument */
        mrc     p15, 0, r0, c0, c0, 5
	and	r0, #3

	bl	kyu_newcore

@ If we return, we will spin here rapidly
@ incrementing the sentinel location.
@ note that there is special static ram at addr 0

	mov	r0, #0
	ldr	r1, =SENTINEL
loopcore:
	add	r0, #1
        str     r0, [r1]
        b      loopcore

/* We realize that each core needs its own stack ...
 *  1-28-2017
 * Putting a stack at 0x7f00800 makes bad things happen
 * Start each cpu with a 16K stack (very generous).
 */
	@ .align 3
	.globl newcoreX
newcoreX:
	/* Read processor affinity register */
        mrc     p15, 0, r1, c0, c0, 5
	and	r1, #3
	lsl	r1, #14
	ldr	r0, =0x6f004000
	add	r0, r0, r1
	add	sp, r0, #0

	@ldr	sp, =0x6f008000
	bl	kyu_newcore

	@ mov	r0, #0
        @ str     r0, statcore
	@ ldr	r0, =0x1234abcd
	mov	r0, #0
	@ ldr	r1, =0x01f01da4
	ldr	r1, =SENTINEL
loopcoreX:
	add	r0, #1
        str     r0, [r1]
        b      loopcoreX

        .globl oldcore
oldcore:
        mov     r0, #0
        ldr     r1, =0x01f01da4
loopcore2:
        str     r0, [r1]
        b      loopcore2

#endif /* BOARD_ORANGE_PI */

/* THE END */
