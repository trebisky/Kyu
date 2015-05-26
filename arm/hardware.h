/* hardware.h
 * T. Trebisky  5/26/2015
 */

#ifdef ARCH_ARM
#define THR_STACK_BASE	0x98000000
#define THR_STACK_LIMIT	4096 * 128

#define EVIL_STACK_BASE 0x9a000000

#define MALLOC_BASE	0x90000000
#define MALLOC_SIZE	4 * 1024 * 1024
#endif

#ifdef ARCH_X86
#define THR_STACK_BASE	0x70000
#define THR_STACK_LIMIT	4096 * 32 /* (0x20000) */
#endif

/* THE END */
