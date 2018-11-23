/* start.S
 * This is a minimal startup file for a
 * bare metal program that runs on the Fire3.
 *
 * Tom Trebisky 9-16-2018
 *
 * With the NSIH header configured, this
 * is intended to be loaded by the bl1 loader.
 * Without it (or with it for that matter), that is
 * is intended to be loaded by U-Boot tftp
 */

.globl  start
.globl  asm_startup
start:
asm_startup:

#ifdef CONFIG_NSIH_HEADER
        b       after

        .skip   60
        .word 0x00000000    // 0x040 : Device Read Address for next boot
        .word 0x00010000    // 0x044 : Load Size for this
        .word 0x40000000    // 0x048 : Load Address for this
        .word 0x40000000    // 0x04C : Launch Address for this

        .skip 428
        .word 0x4849534E    // 0x1FC        "NSIH"

        .align 3
after:
#endif

/* ----------------------------------------------------------- */

reset:
	msr DAIFSet, #0xf

	bl	remap_vectors

	bl      clear_bss
        dsb	sy

	/*
	ldr	x0, =0x400f4100
	ldr	x1, =0xdeadbeef
	str	x1, [x0]

	ldr	x0, =0x400f4104
	str	x1, [x0]
	ldr	x0, =0x400f4108
	str	x1, [x0]
	*/

        bl      board_mmu_init
        dsb	sy
	// bl	tjt_startup

        // bl      mmu_set_ttbr

	bl	kyu_startup

	b	.

/* ----------------------------------------------------------- */

clear_bss:
        ldr     x0, =__bss_start__
        ldr     x1, =__bss_end__

        mov     x2, #0
	mov	x3, #0
// 1:	str     x2, [x0]
1:	stp     x2, x3, [x0]
        add     x0, x0, #16
        cmp     x0, x1
        b.lo    1b
	ret

/* ----------------------------------------------------------- */

#define PSR_MODE	0x9	/* EL2h */
#define PSR_D_BIT	0x200
#define PSR_A_BIT	0x100
#define PSR_I_BIT	0x080
#define PSR_F_BIT	0x040

/* New threads fire up with interrupts enabled */
// #define INITIAL_PSR	(PSR_D_BIT | PSR_A_BIT | PSR_MODE)
#define INITIAL_PSR	0x309

// This needs to have DAIF added.
// #define BARE_PSR	(PSR_MODE)
#define BARE_PSR	0x009

/* resume_c ( cregs )
 *
 * This routine launches a thread from scratch.
 *  On the ARM it gets a pointer (in x0)
 *  to an array of 4 items:
 *      1 - an argument to pass
 *      2 - a place to start up at.
 *      3 - an initial stack pointer.
 *      4 - a place to return to (usually thr_exit)
 */
        .globl resume_cc
resume_cc:
	ldp	x1, x2, [x0]
	ldp	x3, x30, [x0,16]
	mov	sp, x3

        msr     elr_el2, x2
	// ldr	x2, =INITIAL_PSR
	mov	x2, #INITIAL_PSR
        msr     spsr_el2, x2

	mov	fp, #0		/* to end backtraces */
	mov	x0, x1
	eret

// for testing and experimentation.
	.globl get_regs
get_regs:
	stp	x0, x1, [x0]
        stp     x2, x3, [x0, 16]
        stp     x4, x5, [x0, 32]
        stp     x6, x7, [x0, 48]
        stp     x8, x9, [x0, 64]
        stp     x10, x11, [x0, 80]
        stp     x12, x13, [x0, 96]
        stp     x14, x15, [x0, 112]
        stp     x16, x17, [x0, 128]
        stp     x18, x19, [x0, 144]
        stp     x20, x21, [x0, 160]
        stp     x22, x23, [x0, 176]
        stp     x24, x25, [x0, 192]
        stp     x26, x27, [x0, 208]
        stp     x28, x29, [x0, 224]

	mov	x1, sp
        stp     x30, x1, [x0, 240]
        stp     xzr, xzr, [x0, 256]
	ret

// resume_i ( iregs )
// resume_j ( iregs )

        .globl resume_ii
        .globl resume_jj
resume_ii:
resume_jj:
        ldp     x4, x5, [x0, 32]
        ldp     x6, x7, [x0, 48]
        ldp     x8, x9, [x0, 64]
        ldp     x10, x11, [x0, 80]
        ldp     x12, x13, [x0, 96]
        ldp     x14, x15, [x0, 112]
        ldp     x16, x17, [x0, 128]
        ldp     x18, x19, [x0, 144]
        ldp     x20, x21, [x0, 160]
        ldp     x22, x23, [x0, 176]
        ldp     x24, x25, [x0, 192]
        ldp     x26, x27, [x0, 208]
        ldp     x28, x29, [x0, 224]

        ldp     x30, x1, [x0, 240]
        mov     sp, x1

        ldp     x1, x2, [x0, 256]
        msr     elr_el2, x1
        msr     spsr_el2, x2

	ldp     x2, x3, [x0, 16]
	ldp     x0, x1, [x0]
	eret

// save_j ( jregs )
// This returns 0 on the setup call.
// This will return 1 when resume_j uses
// what we save (because resume_j restores
// x0 with the value of 1 we save below.

        .globl save_j
save_j:
	stp	x0, x1, [sp,-16]!	// push for now

	stp	x2, x3, [x0, 16]
	stp	x4, x5, [x0, 32]
	stp	x6, x7, [x0, 48]
	stp	x8, x9, [x0, 64]
	stp	x10, x11, [x0, 80]
	stp	x12, x13, [x0, 96]
	stp	x14, x15, [x0, 112]
	stp	x16, x17, [x0, 128]
	stp	x18, x19, [x0, 144]
	stp	x20, x21, [x0, 160]
	stp	x22, x23, [x0, 176]
	stp	x24, x25, [x0, 192]
	stp	x26, x27, [x0, 208]
	stp	x28, x29, [x0, 224]

	add	x1, sp, #16
	stp	x30, x1, [x0, 240]

	// Here x1 will become elr_el2
	// and x2 will become spsr_el2
	mov	x2, #INITIAL_PSR
	mrs	x1, DAIF
	orr	x2, x2, x1

	mov	x1, lr
	stp	x1, x2, [x0, 256]

	ldp	x1, x2, [sp], 16	// pop x0, x1
	mov	x1, #1			// resume_j will return 1
	stp	x1, x2, [x0]		// save x0, x1

	mov	x0, #0			// return 0 now.
	ret

/* ----------------------------------------------------------- */

remap_vectors:
        adr     x3, vectors_el2
        msr     VBAR_EL2, x3
        ret

.globl	cur_thread

// Comes here with vector offset in x1 that needs
// to end up in x0 as argument to fault_handler
new_fault:

	// stp	x0, x1, [sp,-16]!	// push for now
	ldr	x0, =cur_thread
	ldr	x0, [x0]

	stp	x2, x3, [x0, 16]
	stp	x4, x5, [x0, 32]
	stp	x6, x7, [x0, 48]
	stp	x8, x9, [x0, 64]
	stp	x10, x11, [x0, 80]
	stp	x12, x13, [x0, 96]
	stp	x14, x15, [x0, 112]
	stp	x16, x17, [x0, 128]
	stp	x18, x19, [x0, 144]
	stp	x20, x21, [x0, 160]
	stp	x22, x23, [x0, 176]
	stp	x24, x25, [x0, 192]
	stp	x26, x27, [x0, 208]
	stp	x28, x29, [x0, 224]

	mov	x2, sp
	stp	x30, x2, [x0, 240]

	mrs	x2, elr_el2
	mrs	x3, spsr_el2
	stp	x2, x3, [x0, 256]

	ldp	x2, x3, [sp], 16	// pop x0, x1
	stp	x2, x3, [x0]		// save x0, x1

	mov	x0, x1
	b	fault_handler

// identical to the above, but calls the IRQ handler
// This is what we actually use.
new_irq:

	stp	x0, x1, [sp,-16]!	// push for now
	ldr	x0, =cur_thread
	ldr	x0, [x0]
	stp	x2, x3, [x0, 16]
	stp	x4, x5, [x0, 32]
	stp	x6, x7, [x0, 48]
	stp	x8, x9, [x0, 64]
	stp	x10, x11, [x0, 80]
	stp	x12, x13, [x0, 96]
	stp	x14, x15, [x0, 112]
	stp	x16, x17, [x0, 128]
	stp	x18, x19, [x0, 144]
	stp	x20, x21, [x0, 160]
	stp	x22, x23, [x0, 176]
	stp	x24, x25, [x0, 192]
	stp	x26, x27, [x0, 208]
	stp	x28, x29, [x0, 224]

	// mov	x1, sp
	add	x1, sp, #16
	stp	x30, x1, [x0, 240]

	mrs	x1, elr_el2
	mrs	x2, spsr_el2
	stp	x1, x2, [x0, 256]

	ldp	x1, x2, [sp], 16	// pop x0, x1
	stp	x1, x2, [x0]		// save x0, x1

	b	do_irq

#ifdef notdef
.fault:
    stp     x27, x28, [sp, -16]!
    stp     x25, x26, [sp, -16]!
    stp     x23, x24, [sp, -16]!
    stp     x21, x22, [sp, -16]!
    stp     x19, x20, [sp, -16]!
    stp     x17, x18, [sp, -16]!
    stp     x15, x16, [sp, -16]!
    stp     x13, x14, [sp, -16]!
    stp     x11, x12, [sp, -16]!
    stp     x9,  x10, [sp, -16]!
    stp     x7,  x8,  [sp, -16]!
    stp     x5,  x6,  [sp, -16]!
    stp     x3,  x4,  [sp, -16]!
    stp     x1,  x2,  [sp, -16]!
    stp     xzr, x0,  [sp, -16]!

    mov     x0, x29
    // add     x1, sp, 8
    // add     x1, sp, 0
    mov     x1, sp
    bl      fault_handler

    ldp     xzr, x0,  [sp], 16
    ldp     x1,  x2,  [sp], 16
    ldp     x3,  x4,  [sp], 16
    ldp     x5,  x6,  [sp], 16
    ldp     x7,  x8,  [sp], 16
    ldp     x9,  x10, [sp], 16
    ldp     x11, x12, [sp], 16
    ldp     x13, x14, [sp], 16
    ldp     x15, x16, [sp], 16
    ldp     x17, x18, [sp], 16
    ldp     x19, x20, [sp], 16
    ldp     x21, x22, [sp], 16
    ldp     x23, x24, [sp], 16
    ldp     x25, x26, [sp], 16
    ldp     x27, x28, [sp], 16
    ldp     x29, x30, [sp], 16
    eret


.irq:
1:  stp     x29, x30, [sp, -16]!
    stp     x27, x28, [sp, -16]!
    stp     x25, x26, [sp, -16]!
    stp     x23, x24, [sp, -16]!
    stp     x21, x22, [sp, -16]!
    stp     x19, x20, [sp, -16]!
    stp     x17, x18, [sp, -16]!
    stp     x15, x16, [sp, -16]!
    stp     x13, x14, [sp, -16]!
    stp     x11, x12, [sp, -16]!
    stp     x9,  x10, [sp, -16]!
    stp     x7,  x8,  [sp, -16]!
    stp     x5,  x6,  [sp, -16]!
    stp     x3,  x4,  [sp, -16]!
    stp     x1,  x2,  [sp, -16]!
    stp     xzr, x0,  [sp, -16]!

    bl      do_irq

    ldp     xzr, x0,  [sp], 16
    ldp     x1,  x2,  [sp], 16
    ldp     x3,  x4,  [sp], 16
    ldp     x5,  x6,  [sp], 16
    ldp     x7,  x8,  [sp], 16
    ldp     x9,  x10, [sp], 16
    ldp     x11, x12, [sp], 16
    ldp     x13, x14, [sp], 16
    ldp     x15, x16, [sp], 16
    ldp     x17, x18, [sp], 16
    ldp     x19, x20, [sp], 16
    ldp     x21, x22, [sp], 16
    ldp     x23, x24, [sp], 16
    ldp     x25, x26, [sp], 16
    ldp     x27, x28, [sp], 16
    ldp     x29, x30, [sp], 16
    eret

.macro OLDpanic
        .align  7
1:  stp     x29, x30, [sp, -16]!
    mov     x29, 1b - vectors_el2
    // b .fault
    b	new_fault
.endm
#endif

.macro panic
        .align  7
1:	stp	x0, x1, [sp,-16]!	// push for now
	mov     x1, 1b - vectors_el2
	b	new_fault
	.word	0xdeadbeef
.endm


/* Vector table entries are 128 bytes in size, i.e. 32 instructions.
 *  (armv7 had 4 bytes per entry (1 instruction)).
 * There are 16 entries in the table.
 * This is 4 types of exceptions from 4 sources.
 * The 4 sources are:
 *  Same exception level when source SP = SP_EL0
 *  Same exception level when source SP = SP_ELx
 *  Lower exception level running on AArch64
 *  Lower exception level running on AArch32
 */

.globl vectors

        .text
        .align 11
vectors:
vectors_el2:
	panic 		/* 0x000 Current EL w/ SP0 - Synchronous Thread */
	panic 		/* 0x080 Current EL w/ SP0 - IRQ Thread */
        panic 		/* 0x100 Current EL w/ SP0 - FIQ Thread */
        panic 		/* 0x180 Current EL w/ SP0 - Error Thread */

        panic 		/* 0x200 Current EL w/ SPx - Synchronous Handler */
        .align  7	/* 0x280 Current EL w/ SPx - IRQ Handler */
        b	new_irq
        // b	.irq
        panic 		/* 0x300 Current EL w/ SPx - FIQ Handler */
        panic 		/* 0x380 Current EL w/ SPx - Error Handler */

        panic 		/* 0x400 Lower EL 64 bit - Synch */
	panic 		/* 0x480 Lower EL 64 bit - IRQ */
	panic 		/* 0x500 Lower EL 64 bit - FIQ */
	panic 		/* 0x580 Lower EL 64 bit - Error */

	panic 		/* 0x600 Lower EL 32 bit - Synch */
	panic 		/* 0x680 Lower EL 32 bit - IRQ */
	panic 		/* 0x700 Lower EL 32 bit - FIQ */
	panic 		/* 0x780 Lower EL 32 bit - Error */

/* ----------------------------------------------------------- */

#ifdef notdef
// Get our cpu ID
// unsigned int GetCPUID(void);
.global GetCPUID
GetCPUID:
        mrs     x1, MPIDR_EL1
        and     x0, x1, #0x3
	tst     x1, 0xff00
	beq     1f
	orr     x0, x0, 4
1:	ret

// unsigned int GetCurrentSMode(void);
.global GetCurrentSMode
GetCurrentSMode:
        mrs             x0, CurrentEL
        lsr             x0, x0, #2
        ret
#endif

/* ----------------------------------------------------------- */
/* Here are some library routines.
 */

/*
 * void __asm_flush_dcache_range(start, end)
 * From U-Boot sources, u-boot-2018.09/arch/arm/cpu/armv8/cache.S
 *
 * clean & invalidate data cache over a range
 *
 * x0: start address
 * x1: end address
 */

.global __asm_flush_dcache_range
__asm_flush_dcache_range:

        mrs     x3, ctr_el0
        lsr     x3, x3, #16
        and     x3, x3, #0xf
        mov     x2, #4
        lsl     x2, x2, x3              /* cache line size */

        /* x2 <- minimal cache line size in cache system */
        sub     x3, x2, #1
        bic     x0, x0, x3
1:      dc      civac, x0       /* clean & invalidate data or unified cache */
        add     x0, x0, x2
        cmp     x0, x1
        b.lo    1b
        dsb     sy
        ret

/*
 * void __asm_invalidate_dcache_range(start, end)
 * From U-Boot sources, u-boot-2018.09/arch/arm/cpu/armv8/cache.S
 *
 * invalidate data cache in the range
 *
 * x0: start address
 * x1: end address
 */

.global  __asm_invalidate_dcache_range
__asm_invalidate_dcache_range:

        mrs     x3, ctr_el0
        ubfm    x3, x3, #16, #19
        mov     x2, #4
        lsl     x2, x2, x3              /* cache line size */

        /* x2 <- minimal cache line size in cache system */
        sub     x3, x2, #1
        bic     x0, x0, x3
1:      dc      ivac, x0        /* invalidate data or unified cache */
        add     x0, x0, x2
        cmp     x0, x1
        b.lo    1b
        dsb     sy
        ret

// THE END
