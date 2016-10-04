#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H

/* Kyu tjt 10-1-2016
 * From linux sources circa 4.0.5 or so.
 *    arch/arm/include/asm/linkage.h
 */

#ifdef KYU
/* renamed asmlinkage.h to avoid collision with include/linux/linkage.h */
#else
#endif


#define __ALIGN .align 0
#define __ALIGN_STR ".align 0"

#define ENDPROC(name) \
  .type name, %function; \
  END(name)

#endif
