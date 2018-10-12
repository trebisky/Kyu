/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* kyulib.h
 *	Tom Trebisky 12/1/2001
 *	Tom Trebisky 9/23/2015
 */

/* This whole gcc builtin thing is a pain in the butt.
 * We would like to use the compiler builtins, but
 * getting it all straight is a big headache right now.
 * When we link with the linux subsystem, we must turn this off.
 * tjt 5-39-2015
 */

/* OLD and bogus now (sorta)
#define USE_GCC_BUILTINS
*/

#define USE_LINUX_STR
#define USE_LINUX_MEM
/* in prf.c */
#define USE_LINUX_PRF

void dump_w ( void *, int );
void dump_l ( void *, int );

/* from linux/compiler.h */
/* I don't use these, but they look interesting */

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

// Making this long rather than int cleans up the compile
// of dlmalloc.c on 64 bit armv8
typedef unsigned long size_t;

#ifdef USE_GCC_BUILTINS
#define memcpy(a,b,c)   __builtin_memcpy((a),(b),(c))
#define memcmp(a,b,c)   __builtin_memcmp((a),(b),(c))
#define memset(a,b,c)   __builtin_memset((a),(b),(c))
#define strcmp(a,b)     __builtin_strcmp((a),(b))
#define strcpy(a,b)     __builtin_strcpy((a),(b))
#define strncpy(a,b,c)  __builtin_strncpy((a),(b),(c))
#define strlen(a)       __builtin_strlen((a))
#endif

/* We provide these (at least thus far */
int printf(const char *, ...);
int sprintf(char *, const char *, ...);

void * memcpy ( void *, char *, size_t );

/* Defined in console.c */
void putchar ( int );
void puts ( char * );

/*
int strcmp ( const char *, const char * );
char *strncpy( char *, const char *, size_t );
char *strcpy ( char *, const char * );
void *memset( void *s, int c, size_t n );
int snprintf(char *str, size_t size, const char *format, ...);
*/

#ifdef LED_DEBUG
void flash ( void );
void big_delay ( void );
#endif

#define SIMPLE_SLAB
#ifdef SIMPLE_SLAB

struct kmem_cache {
	size_t size;
};

typedef struct kmem_cache kmem_cache_t;
#endif

/* XXX - the following fixed size should really
 * be an argument to cq_init() and be dynamically
 * allocated.
 */

#define MAX_CQ_SIZE	1024

struct cqueue {
	char	buf[MAX_CQ_SIZE];
	char	*bp;
	char	*ip;
	char	*op;
	char	*limit;
	int	size;
	int	count;
	int	toss;
};

struct cqueue * cq_init ( int );
void cq_add ( struct cqueue *, int );
int cq_remove ( struct cqueue * );
int cq_count ( struct cqueue * );

#ifndef NULL
#define NULL	(0)
#endif

#ifdef ARCH_X86
extern inline void cpu_enter ( void )
{
    __asm__ __volatile__ ( "cli" );
}

extern inline void cpu_leave ( void )
{
    __asm__ __volatile__ ( "sti" );
}
#endif

/* THE END */
