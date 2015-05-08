/* locore.S
 * temporary catch-all for assembly language
 */

/* For the time being, this is the first thing linked
 * and U-boot simply branches to a hard address 80300000
 * which is what this had better be.
 * For some reason the compiler was reordering routines
 * compiled from main.c, this makes us immune to that.
 */
	.globl uboot_startup
uboot_startup:
	bl	kern_startup
	bx	lr

        .globl get_sp
get_sp:
        add     r0, sp, #0
	bx	lr

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
 */
	.globl resume_c
resume_c:
	ldmia	r0,{r0,sp,lr,pc}

	.globl resume_i
resume_i:
	bx	lr

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
