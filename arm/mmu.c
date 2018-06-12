/* mmu.c - mmu_enable, mmu_disable, mmu_set_ttbr */

/*------------------------------------------------------------------------
 * mmu_set_ttbr  -  Set the Translation Table Base Register
 *------------------------------------------------------------------------
 */
void
mmu_set_ttbr ( void *base )
{
	asm volatile (

			/* We want to use TTBR0 only, 16KB page table */

			"ldr	r0, =0x00000000\n"

			/* Write the value into TTBCR */

			"mcr	p15, 0, r0, c2, c0, 2\n"

			/* Load the base address in r0 */

			"mov	r0, %0\n"

			/* Set table walk attributes */

			"orr	r0, #0x00000002\n" /* shareable cacheable */
			"orr	r0, #0x00000008\n" /* outer write-back write-allocate */
			"orr	r0, #0x00000001\n" /* inner write-back write-allocate */

			/* Write the new TTBR0 */

			"mcr	p15, 0, r0, c2, c0, 0\n"

			/* Write the new TTBR1 (just in case) */

			"mcr	p15, 0, r0, c2, c0, 1\n"

			/* Perform memory synchronization */
			"isb\n"
			"dsb\n"
			"dmb\n"

			:		/* Output	*/
			: "r" (base)	/* Input	*/
			: "r0"		/* Clobber	*/
		);
}

/*------------------------------------------------------------------------
 * mmu_set_dacr -  Set the Domain Access Control Register
 *------------------------------------------------------------------------
 */
void
mmu_set_dacr ( unsigned int dacr )
{
	asm volatile (

			/* Load the dacr setting into r0 */

			"mov	r0, %0\n"

			/* Write the new DACR */

			"mcr	p15, 0, r0, c3, c0, 0\n"

			/* Perform memory synchronization */
			"isb\n"
			"dsb\n"
			"dmb\n"

			:		/* Output	*/
			: "r" (dacr)	/* Input	*/
			: "r0"		/* Clobber	*/
		);
}
