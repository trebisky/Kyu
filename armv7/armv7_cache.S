/*-
 * Copyright (c) 2010 Per Odlund <per.odlund@armagedon.se>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* ARMv7 assembly functions for manipulating caches and other core functions.
 * Based on cpufuncs for v6 and xscale.
 */

/* Added to Kyu 6-12-2018
 * Part of my integration of Marco's multicore work in Xinu
 * for the Orange Pi.
 * It looks to me like generic ARM v7 code.
 */

.text

.globl	armv7_dcache_wbinv_all
armv7_dcache_wbinv_all:
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	tst	r0, #0x07000000
	bxeq	lr
	mov	r3, #0			@ start with L1

start_wbinv:
	add	r2, r3, r3, lsr #1	@ r2 = level * 3 / 2
	mov	r1, r0, lsr r2		@ r1 = cache type
	tst	r1, #6			@ is it unified or data?
	beq	next_level_wbinv	@ nope, skip level

	mcr	p15, 2, r3, c0, c0, 0	@ select cache level
	isb
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	ip, r0, #0, #3		@ get linesize from CCSIDR
	add	ip, ip, #4		@ apply bias
	ubfx	r2, r0, #13, #15	@ get numsets - 1 from CCSIDR
	lsl	r2, r2, ip		@ shift to set position
	orr	r3, r3, r2		@ merge set into way/set/level
	mov	r1, #1
	lsl	r1, r1, ip		@ r1 = set decr

	ubfx	ip, r0, #3, #10		@ get numways - 1 from [to be discarded] CCSIDR
	clz	r2, ip			@ number of bits to MSB of way
	lsl	ip, ip, r2		@ shift by that into way position
	mov	r0, #1			@
	lsl	r2, r0, r2		@ r2 now contains the way decr
	mov	r0, r3 			@ get sets/level (no way yet)
	orr	r3, r3, ip		@ merge way into way/set/level
	bfc	r0, #0, #4		@ clear low 4 bits (level) to get numset - 1
	sub	r2, r2, r0		@ subtract from way decr

	/* r3 = ways/sets/level, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c14, 2	@ writeback and invalidate line
	cmp	r3, #15			@ are we done with this level (way/set == 0)
	bls	next_level_wbinv	@ yes, go to next level
	ubfx	r0, r3, #4, #18		@ extract set bits
	cmp	r0, #0			@ compare
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

next_level_wbinv:
	dsb
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	ubfx	ip, r0, #24, #3		@ narrow to LoC
	add	r3, r3, #2		@ go to next level
	cmp	r3, ip, lsl #1		@ compare
	blt	start_wbinv		@ not done, next level (r0 == CLIDR)

done_wbinv:
	mov	r0, #0			@ default back to cache level 0
	mcr	p15, 2, r0, c0, c0, 0	@ select cache level
	dsb
	isb
	bx	lr

.globl	armv7_dcache_l1inv_all
armv7_dcache_l1inv_all:
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	and	r0, r0, #0x7		@ check L1
	bxeq	lr			@ return if no L1 cache
	mov	r3, #0			@ start with L1
	mcr	p15, 2, r3, c0, c0, 0	@ select cache level
	isb
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	ip, r0, #0, #3		@ get linesize from CCSIDR
	add	ip, ip, #4		@ apply bias
	ubfx	r2, r0, #13, #15	@ get numsets - 1 from CCSIDR
	lsl	r2, r2, ip		@ shift to set position
	orr	r3, r3, r2		@ merge set into way/set/level
	mov	r1, #1
	lsl	r1, r1, ip		@ r1 = set decr

	ubfx	ip, r0, #3, #10		@ get numways - 1 from [to be discarded] CCSIDR
	clz	r2, ip			@ number of bits to MSB of way
	lsl	ip, ip, r2		@ shift by that into way position
	mov	r0, #1			@
	lsl	r2, r0, r2		@ r2 now contains the way decr
	mov	r0, r3 			@ get sets/level (no way yet)
	orr	r3, r3, ip		@ merge way into way/set/level
	bfc	r0, #0, #4		@ clear low 4 bits (level) to get numset - 1
	sub	r2, r2, r0		@ subtract from way decr

	/* r3 = ways/sets/level, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c6, 2	@ invalidate line
	cmp	r3, #15			@ are we done with this level (way/set == 0)
	bls	done_l1inv		@ yes, we've finished
	ubfx	r0, r3, #4, #18		@ extract set bits
	cmp	r0, #0			@ compare
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

done_l1inv:
	dsb
	mov	r0, #0			@ default back to cache level 0
	mcr	p15, 2, r0, c0, c0, 0	@ select cache level
	dsb
	isb
	bx	lr

#ifdef notdef

.globl	armv7_icache_inv_all
armv7_icache_inv_all:
	mov	r0, #0
	mcr	p15, 2, r0, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	r2, r0, #13, #15	@ get num sets - 1 from CCSIDR
	ubfx	r3, r0, #3, #10		@ get numways - 1 from CCSIDR
	clz	r1, r3			@ number of bits to MSB of way
	lsl	r3, r3, r1		@ shift into position
	mov	ip, #1			@
	lsl	ip, ip, r1		@ ip now contains the way decr

	ubfx	r0, r0, #0, #3		@ get linesize from CCSIDR
	add	r0, r0, #4		@ apply bias
	lsl	r2, r2, r0		@ shift sets by log2(linesize)
	add	r3, r3, r2		@ merge numsets - 1 with numways - 1
	sub	ip, ip, r2		@ subtract numsets - 1 from way decr
	mov	r1, #1
	lsl	r1, r1, r0		@ r1 now contains the set decr
	mov	r2, ip			@ r2 now contains set way decr

	/* r3 = ways/sets, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c6, 2	@ invalidate line
	movs	r0, r3			@ get current way/set
	beq	2f			@ at 0 means we are done.
	lsls	r0, r0, #10		@ clear way bits leaving only set bits
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

2:	dsb				@ wait for stores to finish
	mov	r0, #0			@ and ...
	mcr	p15, 0, r0, c7, c5, 0	@ invalidate L1 cache
	isb				@ instruction sync barrier
	bx	lr			@ return
	
#endif

// THE END
