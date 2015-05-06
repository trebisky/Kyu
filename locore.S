/* locore.S
 * temporary catch-all for assembly language
 */

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

/* disable both IRQ and FIQ */
	.globl disable_interrupts
disable_interrupts:
	mrs	r0, cpsr
	orr	r0, r0, #0xc0
	msr	cpsr_c, r0
	bx	lr

/* THE END */
