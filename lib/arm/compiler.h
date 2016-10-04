#ifndef __ASM_ARM_COMPILER_H
#define __ASM_ARM_COMPILER_H

/* Kyu tjt 10-1-2016
 * From linux sources circa 4.0.5 or so.
 *    arch/arm/include/asm/compiler.h
 */

#ifdef KYU
#else
#endif

/*
 * This is used to ensure the compiler did actually allocate the register we
 * asked it for some inline assembly sequences.  Apparently we can't trust
 * the compiler from one version to another so a bit of paranoia won't hurt.
 * This string is meant to be concatenated with the inline asm string and
 * will cause compilation to stop on mismatch.
 * (for details, see gcc PR 15089)
 * For compatibility with clang, we have to specifically take the equivalence
 * of 'r11' <-> 'fp' and 'r12' <-> 'ip' into account as well.
 */
#define __asmeq(x, y)				\
	".ifnc " x "," y "; "			\
	  ".ifnc " x y ",fpr11; " 		\
	    ".ifnc " x y ",r11fp; "		\
	      ".ifnc " x y ",ipr12; " 		\
	        ".ifnc " x y ",r12ip; "		\
	          ".err; "			\
	        ".endif; "			\
	      ".endif; "			\
	    ".endif; "				\
	  ".endif; "				\
	".endif\n\t"


#endif /* __ASM_ARM_COMPILER_H */
