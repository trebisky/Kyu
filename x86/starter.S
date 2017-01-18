/* starter.S
 * $Id: starter.S,v 1.4 2005/03/30 21:14:36 tom Exp $
 * T. Trebisky  7/22/2001
 * T. Trebisky  9/15/2001
 *
 * Cars need a starter, so why not us.
 *
 * This runs in real mode, and does the gyrations to get
 * us into protected mode and running where we want it to.
 *
 * Here is the memory concept, as we now see it....
 *
 * We leave 0x00000 - 0x0ffff for the BIOS.
 * We leave 0x90000 - 0x9ffff for netboot.
 * We use   0x80000 - 0x8ffff for THIS (starter).
 * We use   0x10000 - 0x7ffff for the kernel.
 * We avoid 0xA0000 - 0xBffff (VGA memory and such).
 * We avoid 0xC0000 - 0xDffff (IO memory and such).
 * We avoid 0xE0000 - 0xfffff (rom BIOS and such).
 * 
 * The kernel starts its initial stack at
 *  0x90000, which will probably change someday.
 *
 * This limits the kernel to 7*64k in size (448k)
 *  which is just fine really.
 *
 * After netboot loads the kernel to 0x10000,
 *  this code relocates the kernel from
 *  0x10000 to 0x1000 to reclaim 64k.
 *
 * This will keep us nice and tight in the start of
 * memory, right up against the vector table.
 * and leaves just enough for the interrupt table.
 * (well, at least it leaves the bottom 4K alone).
 *
 *  (We cannot simply have netboot or whoever put us there
 *   right off, since the BIOS needs to live there during
 *   the loading process ... Except for the initial messages,
 *   this code can do without the BIOS)
 *
 * A lot of this code is taken (more or less) from linux setup.S
 * 
 * "compile" using gas.
 *
 * 	Works OK  9/15/2001
 */

# Must have the .code16 line, or she won't work!

.code16
.text
.org 0

/* Kernel CS indexes entry 2 in the GDT
 * Kernel DS indexes entry 3 in the GDT
 * (GDT entries 0 and 1 are null ).
 * the DS entry *must* follow the CS entry in the GDT
 * (we read the CS and tick it one to get the DS later
 *  when we are in 32 bit protected mode).
 */

KERNEL_CS = 0x10
KERNEL_DS = 0x18


#define LED_DEBUG

# This happens to be the parallel data out port on
# the machine I am testing with.
LED_PORT = 0x378
#define LED_ON		0x01
#define LED_OFF		0x00

# The concept, at least for now, is that this runs at
# CS:IP of 8020:0000 (linear address 0x80200)

# Heaven only knows about the stack.
# Experiment shows that SS:SP is
# typically 8645:0FF0 coming from Netboot.
# And this seems to do just fine for us
# (inheriting a stack), but maybe we should
#  explicitly put it somewhere ? ).

.global _start

_start:
	call	next
next:
	popw	%ax
	subw	$3,%ax
	pushw	%ax
	cli
	cld

# make DS and ES the same as CS
# (this is OK for how netboot transfers control to us).

	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es

#ifdef LED_DEBUG
# Turn out my test LED

	movw	$LED_PORT,%dx
	movw	$LED_OFF,%ax
	outb	%al,%dx
#endif

#ifdef WEIRD_DEBUG
	movw	$'A', %ax
        xorb    %bh, %bh
        movw    $0x01, %cx
        movb    $0x0e, %ah
        int     $0x10

	call	delay

	movw	$'B', %ax
        xorb    %bh, %bh
        movw    $0x01, %cx
        movb    $0x0e, %ah
        int     $0x10

	call	bdelay

	movw	$'C', %ax
        xorb    %bh, %bh
        movw    $0x01, %cx
        movb    $0x0e, %ah
        int     $0x10

	call	outnl
#endif

#ifdef STACK_DEBUG

        lea     mess0,%si
        call    outstr
	jmp	1f

mess0: .string "Real mode stack at: "

1:	movw	%ss,%ax
	call	outhex
	movw	$':', %ax
	call	outchr
	movw	%sp,%ax
	call	outhex
	call	outnl

        lea     mess1,%si
        call    outstr
	jmp	1f

mess1: .string " -- bdelay at: "

1:	movw	%cs,%ax
	call	outhex
	movw	$':', %ax
	call	outchr
	lea	bdelay, %ax
	call	outhex
	call	outnl
#endif

# -----------------------------------
# Announce ourself.

##	lea     mess4,%si
##	call    outstr

##	movw	%cs,%ax
##	call	outhex

##	movw	$':',%ax
##	call	outchr

##	popw	%ax
##	call	outhex
##	call	outnl

# -----------------------------------
# Announce where we will jump to in 32 bit mode.

##	lea     mess5, %si
##	call    outstr
##	call	outsp

#ifdef PATCH_32_ADDR

# We don't do this anymore,
# (but this was once amusing during debug).
# Now, we jump to a hard address.

# ... patch together a linear address for
# when we go to full 32 bit mode.

	xorl	%eax,%eax
	movw	%cs,%ax
	shll	$4,%eax
	addl	$full32, %eax
	movl	%eax,(patch)

#endif

##	movw	(patch+2),%ax
##	call	outhex
##	movw	(patch),%ax
##	call	outhex
##	call	outsp

# seg handling very bogus in what follows ...
#	movl	(patch),%eax
#	shrl	$4,%eax

	movw	$0x100, %ax
	movw	%ax,%fs
##	movw	%fs:(0),%ax
##	call	outhex
##	call	outsp
##	call	outnl

# -----------------------------------
# Move the kernel.
# notice that once we do this, we cannot
# call the BIOS for i/o since we have pretty
# much demolished its data structures.
#
# Also, the kernel must be linked to run at the
# address it finally gets moved to (see the Makefile).

#define MOVE_KERNEL
#ifdef MOVE_KERNEL
# OK, lets move the kernel from
#  0x10000 to 0x1000

	movw	$0x1000, %bx	# get from here.
	movw	$0x8000, %bp	# stop getting here.
	movw	$0x0100, %ax	# put it here.

	pushw	%ds
	cld

# The following loop moves 4k at a whack

kmove:
	movw	%bx, %ds	# source.
	xorw	%si, %si
	movw	%ax, %es	# dest.
	xorw	%di, %di

	movw	$0x800, %cx	# move 0x1000 bytes.
	rep
	movsw

	incb	%ah
	incb	%bh

	cmpw	%bp, %bx
	jb	kmove

	popw	%ds
#endif

# -----------------------------------

#ifdef NICE_SEG_SETUP

# I don't use this, but the following is a
# full general way to get the segment regs
# consistent with the expectation that this
# was assembled with the org 0 above.

# Set up and do an iret to get the segment
# registers fixed.  This is general code
# that makes no assumption about how CS:IP
# is when we get called up.

	call	pic1
pic1:	popw	%ax
	subw	$pic1, %ax

	pushw	%dx
	movw	%cs, %bx
	movw	%bx, %dx
	andw	$0xf000, %dx
	movb	$4, %cl
	shl	%cl, %bx
	addw	%ax, %bx

	shr	%cl, %bx
	addw	%dx, %bx
	popw	%dx

# Now, do an iret
# first push flags, then seg, then offset.

	pushf
	pushw	%bx

	lea	pic2, %ax
	pushw   %ax

pic2:

#endif

# -----------------------------------
# Turn interrupts off,
#  in preparation to go to protected mode.

	cli

# -----------------------------------
# Disable NMI
# port 0x70 is the "CMOS address register port",
# which happens to have an extra bit
# to disable NMI when set.

	movb	$0x80, %al
	outb	%al, $0x70

# -----------------------------------
# Now we set up the descriptor tables.

	call	load_idt
	call	load_gdt
	call	a20on
	call	reset_math

# Now we fiddle with the PIC
#  to mask all interrupts (except IRQ2,
#  since IRQ2 is the cascade from the other PIC).
# 0xa0 and 0xa1 are PIC2 registers.
# 0x20 and 0x21 are PIC1 registers.

	movb	$0xFF, %al
	outb	%al, $0xA1
	call	delay
	
	movb	$0xFB, %al
	outb	%al, $0x21
	call	delay

# -----------------------------------
# OK, turn on the protected mode bit !!

	movw	$1, %ax
	lmsw	%ax

	jmp	flush
flush:

# Now wire together a jump to 32 bit code.
	.byte	0x66
	.byte	0xea
patch:
#ifdef MOVE_KERNEL
	.long	0x1000
#else
	.long	0x10000
#endif
	.word	KERNEL_CS

/* NOTREACHED */

# The following caused me a fair bit of grief.
# At one time I did not have the trailing code16
# (but this was at the end of the file).
# Then I added some 16 bit routines after this
# and when I called them, we crashed ....
# This was very confusing for a while, but in
# prayer meeting the morning of 9/20/2001 God
# reminded me of that .code32 lurking here.

.code32
full32:
	jmp	full32
.code16

# -----------------------------------

mess4: .string "Real mode startup running at "
mess5: .string "Jumping to second stage at "

# -----------------------------------
# -----------------------------------

.align 2

# Descriptor tables
gdt:
	.word	0, 0, 0, 0			# dummy
	.word	0, 0, 0, 0			# unused

	.word	0xFFFF				# 4Gb - (0x100000*0x1000 = 4Gb)
	.word	0				# base address = 0
	.word	0x9A00				# code read/exec
	.word	0x00CF				# granularity = 4096, 386
						#  (+5th nibble of limit)

	.word	0xFFFF				# 4Gb - (0x100000*0x1000 = 4Gb)
	.word	0				# base address = 0
	.word	0x9200				# data read/write
	.word	0x00CF				# granularity = 4096, 386
						#  (+5th nibble of limit)
# Here is a "null" interrupt descriptor table pointer
idt_48:
	.word	0			# limit = 0
	.word	0, 0			# base = 0L

# Here is a our prototype global descriptor table pointer
gdt_48:
	.word	0x8000			# limit=2048 (256 entries)
	.word	0, 0			# base (filled in dynamically)

# -----------------------------------

# load the IDT pointer.

load_idt:
	lidt	idt_48
	ret

# -----------------------------------

load_gdt:
# now build the GDT pointer,
# we need a linear pointer, so we calculate it here.

	xorl	%eax,%eax
	movw	%ds,%ax
	shll	$4, %eax
	addl	$gdt, %eax
	movl	%eax, (gdt_48+2)
	lgdt	gdt_48		
	ret

# -----------------------------------

# that was painless, now we enable a20.
# Use the fast gate A20 in the 8042 keyboard controller,
# but need to fuss around a lot to clear the 8042.

a20on:
	call	drain_8042

	movb	$0xD1, %al		# command write
	outb	%al, $0x64
	call	drain_8042

	movb	$0xDF, %al		# A20 on
	outb	%al, $0x60
	call	drain_8042

# OK, gate on A20, another way.
# You must preserve the other bits here.
# Otherwise embarrasing things like laptops powering off on boot happen.
# Corrected version by Kira Brown from Linux 2.2

	inb	$0x92, %al
	orb	$02, %al		# "fast A20" version
	outb	%al, $0x92		# some chips have only this

# wait until a20 really *is* enabled;
# it can take a fair amount of time on certain systems;
# Toshiba Tecras are known to have this problem.
# The memory location used here (0x200) is the int 0x80
# vector, which should be safe to use.

	xorw	%ax, %ax		# segment 0x0000
	movw	%ax, %fs
	decw	%ax			# segment 0xffff (HMA)
	movw	%ax, %gs

a20_wait:
	incw	%ax			# unused memory location <0xfff0
	movw	%ax, %fs:(0x200)	# we use the "int 0x80" vector
	cmpw	%gs:(0x210), %ax	# and its corresponding HMA addr
	je	a20_wait		# loop until no longer aliased
	ret

# -----------------------------------

# reset any possible math coprocessor.

reset_math:
	xorw	%ax, %ax
	outb	%al, $0xf0
	call	delay

	outb	%al, $0xf1
	call	delay
	ret

# ----------------------------------------------------

#ifdef notdef
kb_wait:
	pushl	%ecx
	movl	$100000, %ecx

1:	decl	%ecx
	jz	2f

	call	delay
	inb	$0x64, %al			# 8042 status port
	testb	$2, %al				# output buffer?
	jnz	1b

2:	popl	%ecx
	ret
#endif
	
# ----------------------------------------------------

# keyboard i/o is via an 8042 chip (or gates emulating it).
# port 0x60 is the data port (r/w)
# port 0x64 is status and command.
# status bit 0 is input buffer full.
# status bit 1 is output buffer full.

# command 0xd1 says next data written goes to port latches,
#  one bit of which is another way to gate A20.
# command 0xed says next data goes to LED's
# command 0xf4 says enable scanning
# command 0xff says reset

# This routine checks that the keyboard command queue is empty
# (after emptying the output buffers)

# Some machines have delusions that the keyboard buffer is always full
# with no keyboard attached...

# If there is no keyboard controller, we will usually get 0xff
# to all the reads.  With each IO taking a microsecond and
# a timeout of 100,000 iterations, this can take about half a
# second ("delay" == outb to port 0x80). That should be ok,
# and should also be plenty of time for a real keyboard controller
# to empty.

drain_8042:
	pushl	%ecx
	movl	$100000, %ecx		# timeout

1:	decl	%ecx
	jz	3f

	call	delay

	inb	$0x64, %al		# status port
	testb	$1, %al			# OBF ?
	jz	2f

	call	delay
	inb	$0x60, %al		# read it
	jmp	1b

2:	testb	$2, %al			# IBF ?
	jnz	1b

3:	popl	%ecx
	ret

# ----------------------------------------------------

spin:	
	jmp	spin

# ----------------------------------------------------
#
# outchr - print the character in %al
# (also outsp, prints space, with fall through).

outsp:  movb    $0x20, %al
outchr:
        xorb    %bh, %bh
        movw    $0x01, %cx
        movb    $0x0e, %ah
        int     $0x10
	ret

# ----------------------------------------------------

beep:   movb    $0x07, %al
        jmp     outchr

outnl:	
	movb    $0x0D, %al
	call	outchr
	movb    $0x0A, %al
	call	outchr
	ret

# ----------------------------------------------------
#
# outhex - print %ax in hex.

outhex:
	movw	%ax,%dx

	movb	$12,%cl
	shr	%cl,%ax
	andw	$0xf,%ax
	cmp	$10,%ax
	jc	ohd1
	addw	$0x07,%ax
ohd1:	addw	$0x30,%ax
	call	outchr

	movw	%dx,%ax
	movb	$8,%cl
	shr	%cl,%ax
	andw	$0xf,%ax
	cmp	$10,%ax
	jc	ohd2
	addw	$0x07,%ax
ohd2:	addw	$0x30,%ax
	call	outchr

	movw	%dx,%ax
	movb	$4,%cl
	shr	%cl,%ax
	andw	$0xf,%ax
	cmp	$10,%ax
	jc	ohd3
	addw	$0x07,%ax
ohd3:	addw	$0x30,%ax
	call	outchr

	movw	%dx,%ax
	andw	$0xf,%ax
	cmp	$10,%ax
	jc	ohd4
	addw	$0x07,%ax
ohd4:	addw	$0x30,%ax
	call	outchr
	ret

# ----------------------------------------------------

# Routine to print asciiz string at ds:si
outstr:
	cld

outs1:  lodsb
        andb    %al, %al
        jnz     outs2
	ret

outs2:  call    outchr
        jmp     outstr

# ----------------------------------------------------

# Delay is needed after doing I/O
delay:
	outb	%al,$0x80
	ret

# ----------------------------------------------------

#ifdef LED_DEBUG

# go into an endless loop flashing an LED on the
# printer port.  This flashed at about 1 Hz running
# on a 233 Mhz Pentium MMX.
# the LED was from pin 2 to pin 18 with a 750 ohm
# current limiting resistor.

flash:
	movw	$LED_PORT,%dx
	movw	$LED_OFF,%ax
	outb	%al,%dx
	call	bdelay

	movw	$LED_PORT,%dx
	movw	$LED_ON,%ax
	outb	%al,%dx
	call	bdelay

	jmp	flash

# ------------
# big delay for the flash above.

bdelay:
	pushl	%ecx
	movl	$200000, %ecx

1:
	outb	%al,$0x80
	decl	%ecx
	jnz	1b

	popl	%ecx
	ret

#endif /* LED_DEBUG */

# This induces the linker (or whatever)
# to pad the executable out to a nice even
# boundary, but now that we have a nicer
# image builder, this is not necessary.

/*
.org	1024
*/

# THE END
