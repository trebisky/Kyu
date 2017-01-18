/* locore.S
 * $Id: locore.S,v 1.3 2002/10/26 05:10:35 tom Exp $
 * T. Trebisky  9/15/2001
 *
 * Once we get here, we are in 32 bit protected mode.
 *
 * "compile" using gas.
 * Works OK  9/16/2001
 */

#define LED_DEBUG

.code32
.text

# The concept, at least initially, is that this
# loads and runs at CS:IP of 1000:0000
# (linear address 0x10000)
#
# Nowadays, it loads to 0x10000, but we move it
# to linear address 0x1000, and run it there.

.global _begin

_begin:
        cld
        cli

# DS follows CS in GDT, load seg regs accordingly.

	movl	%cs,%eax
	addl	$0x08,%eax
        movl	%eax,%ds
        movl	%eax,%es
        movl	%eax,%fs
        movl	%eax,%gs

# Initialize stack.
# (trample any BIOS EBDA area).

	movl	%eax,(stack_seg)
	movl	$0x90000,%eax
	movl	%eax,(stack_off)
        lss	stack_init,%esp

# Check that A20 is enabled, loop if not.

        xorl	%eax,%eax
1:      incl	%eax
        movl	%eax,0x000000
        cmpl	%eax,0x100000
        je 1b

# Initialize flags.
# (notice that this does cli and cld)

        pushl $0
        popfl

# Zero bss

	xorl	%eax, %eax
	movl	$__bss_start, %edi
	movl	$_end, %ecx
	subl	%edi, %ecx
	rep
	stosb

/* Setup IDT (vector table)
 * First thing is to set up the entire
 * 256 entry vector table with valid
 * interrupt gates to our "bogus" handler.
 * (eventually, this will be used only for
 *  vectors arriving at 48 or higher, which
 *  we *never* expect to see!)
 */
	lea	bogus_int_h, %ebx
	movl	%cs, %eax
	shll	$16, %eax
	movw	%bx, %ax
	movw	$0x8E00, %bx	# valid interrupt gate.

	lea	vector_table, %edi
	movl	$256, %ecx
1:
	movl	%eax, (%edi)
	movl	%ebx, 4(%edi)
	addl	$8, %edi
	dec	%ecx
	jne	1b

/* Now, overwrite what we just did for the first
 * 48 entries to point to our second stage table.
 */
	lea	vector_jump_0, %edx
	lea	vector_table, %edi
	movl	$48, %ecx
1:
	movl	%edx, %ebx
	movl	%cs, %eax
	shll	$16, %eax
	movw	%bx, %ax
	movw	$0x8E00, %bx	# valid interrupt gate.

	movl	%eax, (%edi)
	movl	%ebx, 4(%edi)

	addl	$10, %edx	# XXX - see table below.
	addl	$8, %edi
	dec	%ecx
	jne	1b

# -----------------------------------

	lidt	vector_desc

	call	kern_startup
	/* NOTREACHED */

spin32:	jmp	spin32

# ----------------------------------------

# Delay is needed after doing I/O

delay:
        outb %al, $0x80
        ret

#.globl	user_init
#user_init:
#	pushl	$0xabab1212
#	call	user_initX
#	ret

# ----------------------------------------
# setjmp/longjmp
# kind of a warmup for context switching.
# depends on compiler register allocation.
# %eax %edx %ecx are scratch (these 3 registers).
# We can count on this because we are called
# "predictably", i.e. this is not a trap or
# interrupt handling kind of thing.

.globl	setjmp
setjmp:
	movl	4(%esp), %eax
	movl	%ebx,   (%eax)
	movl	%esp,  4(%eax)
	movl	%ebp,  8(%eax)
	movl	%esi, 12(%eax)
	movl	%edi, 16(%eax)
	movl	(%esp), %edx	# return addr
	movl	%edx, 20(%eax)
	xorl	%eax, %eax	# returns 0
	ret

.globl	longjmp
longjmp:
	movl	 4(%esp), %eax
	movl	  (%eax), %ebx
	movl	 4(%eax), %esp
	movl	 8(%eax), %ebp
	movl	12(%eax), %esi
	movl	16(%eax), %edi
	movl	20(%eax), %edx
	movl	%edx, (%esp)	# new return addr
	xorl	%eax, %eax
	incl	%eax		# returns 1
	ret

# ----------------------------------------
# save_j / resume_j
# basically, setjmp/longjmp on steroids.
# %eax %edx %ecx are scratch (these 3 registers).
# we do trash %eax and %ecx, relying on what the
# compiler does with these.
# XXX - I think we can do away with saving %eip,
# and just use the return address on the stack.
# (tricky, if we do, since save_j will do a ret
# and loose it before abandoning the stack.

.globl	save_j
save_j:
	movl	4(%esp), %eax
	movl	%ebx,  0(%eax)
	movl	%esp,  4(%eax)
	movl	%ebp,  8(%eax)
	movl	%esi, 12(%eax)
	movl	%edi, 16(%eax)
	movl	(%esp), %edx	# return addr
	movl	%edx, 20(%eax)
	xorl	%eax, %eax	# returns 0
	ret

# this version of resume_j is OK from interrupt level. 
# we need to put a 3 word iret frame on the stack.
# (I always seem to forget the order, so here it
#  is after looking it up again ...)
#	
#	esp   --> return address (new %eip)
#	esp+4 --> code segment selector
#	esp+8 --> flags

.globl	resume_j
resume_j:
	movl	 4(%esp), %eax	# arg
	movl	  (%eax), %ebx
	movl	 4(%eax), %esp
	movl	 8(%eax), %ebp
	movl	12(%eax), %esi
	movl	16(%eax), %edi
	movl	20(%eax), %edx

	movl	$0x200, (%esp)	# flags
	movl	%cs, %eax
	pushl	%eax		# selector
	pushl	%edx		# return addr

	xorl	%eax, %eax
	incl	%eax		# returns 1
	iret

# ----------------------------------------
/* Called internally to handle saving registers
 * for interrupts.
 * This gets called with the address of the
 * C routine to call on top of the stack,
 * so we can just "return" to call the function.
 * However, we need to save %esp without this
 * on the stack, so resume_i can do an iret.
 */

.globl cur_thread

save_i:
/* fiddle with the following constant if things
 * in thread.h get changed.
 */
	pushl	%eax
	movl	cur_thread, %eax

	movl	%ebx,  4(%eax)
	popl	%ebx
	movl	%ebx,  0(%eax)	# %eax

	movl	%ecx,  8(%eax)
	movl	%edx, 12(%eax)

	movl	%esi, 16(%eax)
	movl	%edi, 20(%eax)
	movl	%ebp, 24(%eax)
	leal	4(%esp), %ebx
	movl	%ebx, 28(%eax)	# %esp
	ret

/* This is called by anybody who wants to
 * resume a thread frozen via save_i above.
 * This may or may not be called from
 * interrupt level, but we still have a valid
 * iret stack frame.
 */
.globl	resume_i
resume_i:
	movl	 4(%esp), %eax
	movl	 4(%eax), %ebx
	movl	 8(%eax), %ecx
	movl	12(%eax), %edx
	movl	16(%eax), %esi
	movl	20(%eax), %edi
	movl	24(%eax), %ebp
	movl	28(%eax), %esp
	movl	  (%eax), %eax
	iret

# ----------------------------------------
/* Here is the lightweight continuation resumer.
 * It can just abandon the current stack and regs.
 */
#.globl	resume_c
#resume_c:
#	movl	4(%esp), %esp	# new %esp
#	movl	12(%esp), %ebx	# func ptr
#	movl	%ebx, 0(%esp)
#	ret

/* This version is even better, and now we use it
 * all the time.  It's great virtue is that it
 * uses an iret and restores the flags, so
 * that interrupts come back on (and typically
 * it is getting called from within interrupt
 * locked code, so this is a good thing).
 *
 * It works from interrupt code, or regular.
 */

.globl	resume_c
resume_c:
	movl	$0x00001234, %ebp	# XXX
	movl	4(%esp), %esp	# new %esp
	movl	8(%esp), %ebx	# func ptr
	pushl	$0x200		# flags
	movl	%cs, %eax
	pushl	%eax		# selector
	pushl	%ebx		# return addr
	iret

# ----------------------------------------

.globl	getfl
getfl:	pushfl
	popl	%eax
	ret

.globl	getsp
getsp:	movl	%esp, %eax
	subl	$4, %eax
	ret

.globl	getbp
getbp:	movl	%ebp, %eax
	ret

.globl	getip
getip:	movl	(%esp), %eax
	ret

.globl	getgdt
getgdt:	movl	4(%esp), %eax
	sgdt	(%eax)
	ret

.globl	getidt
getidt:	movl	4(%esp), %eax
	sidt	(%eax)
	ret

# ----------------------------------------

stack_init:
stack_off:
	.long 0
stack_seg:
	.long 0

# ----------------------------------------
# ----------------------------------------
# ----------------------------------------
# ----------------------------------------
# ----------------------------------------

#ifdef LED_DEBUG

PIO = 0x378

.globl	flash32
flash32:

        movw $PIO,%dx
        movw $0x00,%ax
        outb %al,%dx

        call ldelay

        movw $PIO,%dx
        movw $0x01,%ax
        outb %al,%dx

        call ldelay

        jmp flash32

# -----------------------

ldelay:
        pushl %ecx
        movl $200000,%ecx

1:
        decl %ecx
        call delay
        jnz 1b

        popl %ecx
        ret
#endif

# ----------------------------------------

bogus_int_h:
	pushl	$bogus_int
	jmp	save_i

# ----------------------------------------

.global vector_jump_0
.global vector_jump_1
.global vector_jump_end

/* Here we have a table of 48 jump vectors
 * (the only ones of the 256 we care about).
 * There had BETTER be 48 of these or we are
 * going to make a terrible mess of things.
 * It turns out these entries are 10 bytes
 * each in length, but someday a clever
 * assembler might trip us up (like by making
 * a short encoding of a relative jump).
 * Code elsewhere modifies this table in place
 * to install interrupt handlers.
 * Right now the 10 bytes are:
 *	0x68	(single byte opcode for pushl)
 *	xxxx	(4 byte address to push)
 *	0xe9	(single byte opcode for jmp)
 *	yyyy	(4 byte jump offset)
 *
 * XXX - before this was at the end of the file,
 *	the assembler was getting clever and
 *	generating some shorter relative jumps
 *	at the end of the table.
 */

vector_jump_0:
	pushl	$unini_int
	jmp	save_i

vector_jump_1:
	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

	pushl	$unini_int
	jmp	save_i

vector_jump_end:

# ----------------------------------------

/*
 * The vector table here is a bunch of 8-byte gizmos
 * (segment descriptors, actually call gates
 * if you really want to get technical.)
 * Some old buggy Pentiums need this aligned in a fussy way.
 */

.data
/* Here is a 48 bit "pseudo descriptor"
 */

.align 2
.global vector_desc
vector_desc:
	.word	256*8-1
	.long	vector_table

/* This gets filled in up above.
 */
.align 8
.global vector_table
vector_table:
	.fill	256,8,0

# THE END
