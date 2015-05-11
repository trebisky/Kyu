/* locore.S
 * temporary catch-all for assembly language
 */

/* For the time being, this is the first thing linked
 * and U-boot simply branches to a hard address 80300000.
 * So it had better find this code there !!
 * For some reason the compiler was reordering routines
 * compiled from main.c, this makes us immune to that.
 */
	.globl uboot_startup
uboot_startup:
	bl	clear_bss
	bl	kern_startup
	bx	lr

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

/* on the BBB, the reset vector will never come here, it will start the
 * firmware ROM code running, which will transfer to the SPL, which will
 * transfer to U-boot, which will finally transfer to us.
 *
 * Someday we could eliminate U-boot, although I don't know why,
 * and just let the SPL load us directly.
 * At any rate, the reset vector won't get any use on this architecture.
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
.globl _start
.globl vectors
_start:
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
 * we simply use sp as a scratch register (and lr as well later on).
 * The ldmia including PC with trailing "^" will copy spsr_irq to cpsr
 */
	.macro kyu_prolog
	movw	sp, #:lower16:cur_thread
	movt	sp, #:upper16:cur_thread
	ldr	sp, [sp, #0]
	stmia	sp, {r0-r12}
	add	sp, sp, #S_PC
	stmdb	sp, {sp, lr}^
	str	lr, [sp,#0]
	mrs	lr, spsr
	str	lr, [sp,#4]	/* saved SR */
	ldr	sp, [sp,#-8]	/* thread stack */
	.endm

	.macro kyu_epilog
	movw	sp, #:lower16:cur_thread
	movt	sp, #:upper16:cur_thread
	ldr	sp, [sp, #0]
	ldr	r1, [sp,#S_PSR]
	msr	spsr, r1
	ldmia	sp, {r0-lr,pc}^
	.endm

	/* Sort of like the epilog, but we get
	 * a pointer to a thread in r0
	 */
	.globl resume_i
resume_i:
	ldr	r1, [r0,#S_PSR]
	msr	spsr, r1
	ldmia	r0, {r0-lr,pc}^
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

        .globl get_regs
get_regs:
	stmia	r0, {r0-pc}
	bx	lr

/* For an experiment from interrupt code */
        .globl get_ssp
get_ssp:
	mrs	ip, cpsr
	bic	r0, ip, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr_c, r0

	add	r0, sp, #0

	msr	cpsr_c, ip
	bx	lr

/* Never called, demo of concept */
        .globl synch_stack
synch_stack:
	mrs	ip, cpsr
	bic	r0, ip, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr_c, r0

	add	r0, sp, #0
	msr	cpsr_c, ip
	add	sp, r0, #0
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

	.globl get_sctrl
get_sctrl:
	mrc     p15, 0, r0, c1, c0, 0
	bx	lr

/* enable_irq (void); */
	.globl enable_irq
enable_irq:
	mrs	r0, cpsr
	bic	r0, r0, #0x80
	msr	cpsr_c, r0
	bx	lr

/* enable_fiq (void); */
	.globl enable_fiq
enable_fiq:
	mrs	r0, cpsr
	bic	r0, r0, #0x40
	msr	cpsr_c, r0
	bx	lr

/* enter critical region
 * i.e. disable both irq and fiq
 */
	.globl cpu_enter
cpu_enter:
	mrs	r0, cpsr
	orr	r0, r0, #0xc0
	msr	cpsr_c, r0
	bx	lr

/* leave critical region
 * i.e. enable both irq and fiq
 * XXX - note that perhaps we just want
 *  to enable whatever was previously enabled
 */
	.globl cpu_leave
cpu_leave:
	mrs	r0, cpsr
	bic	r0, r0, #0xc0
	msr	cpsr_c, r0
	bx	lr

/* This routine launches a thread from scratch.
 * It receives one argument, the future stack pointer
 * On the stack is:
 *	a place to return to (usually thr_exit)
 *	an argument
 *	a place to start up at.
 *	? what about SR ?
 */
	.globl resume_c
resume_c:
	ldmia	r0,{r0,sp,lr,pc}

/* resume_j and save_j don't need to save all registers
 * since these are called "synchronously", the compiler
 * conventions should apply, and r0-r3 should not need
 * to be saved.  Also ip and maybe others.
 * But it is simple to save them all .....
 */

	.globl resume_j
resume_j:
	ldmia	r0, {r0-pc}

	.globl save_j
save_j:
	stmia	r0, {r0-lr}
	mov	ip, #1		/* resume will return 1 */
	str	ip, [r0]
	str	lr, [r0,#60]	/* resume at this pc */
	mov	r0, #0		/* we return 0 */
	bx	lr

/* THE END */
