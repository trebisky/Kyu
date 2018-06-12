#include <armv7a.h>

#define UART0LSR 0x01C28014
#define UART0RBR 0x01C28000
#define TX_READY 0x40

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
	b start_core

/*------------------------------------------------------------------------
 * secondary_start  -  entry point for secondary cores
 *------------------------------------------------------------------------
 */
.globl	secondary_start
secondary_start:
	b start_core

/*------------------------------------------------------------------------
 * start_core  -  core initialization
 *------------------------------------------------------------------------
 */
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

