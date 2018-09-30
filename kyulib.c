/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* kyulib.c
 * A library of odds and ends for skidoo
 *
 *	Tom Trebisky 12/1/2001 5/28/2015
 *
 * Right now we have:
 *	character circular buffer handling.
 *	memory dump routines.
 *	string routines
 */

#include "arch/types.h"
#include "kyu.h"
#include "thread.h"
#include "kyulib.h"
#include "malloc.h"

// #define LIBSTUBS
#ifdef LIBSTUBS
void * memcpy ( void *s1, char *s2, size_t count ) {}
int memcmp ( void *s1, void *s2, size_t count ) {}
void * memset ( void *buf, int val, size_t count ) {}
char * strcpy ( char *ds, const char *ss ) {}
int strcmp ( const char *s1, const char *s2 ) {}
int strncmp ( const char *s1, const char *s2, int n ) {}
int strlen ( const char *s ) {}
int sprintf(char *buf, const char *fmt, ...) {}
#include <stdarg.h>
int vsnprintf(char *buf, int size, const char *fmt, va_list args) {}
#endif

#ifdef notdef
/* Argument in microseconds.
 * silly to try to make this "fast"
 * XXX - this could be more clever.
 */
void
udelay ( int count )
{
	if ( count <= 0 )
	    return;

	if ( count >= 1000 )
	    thr_delay ( (count+999)/1000 );
	else
	    _udelay ( count );
}
#endif

int
hextoi ( char *s )
{
	int val = 0;
	int c;

	while ( c = *s++ ) {
	    if ( c >= '0' && c <= '9' )
		val = val*16 + (c - '0');
	    else if ( c >= 'a' && c <= 'f' )
		val = val*16 + 10 + (c - 'a');
	    else if ( c >= 'A' && c <= 'F' )
		val = val*16 + 10 + (c - 'A');
	    else
	    	break;
	}

	return val;
}

int
atoi ( char *s )
{
	int val = 0;

	if ( s[0] == '0' && s[1] == 'x' )
	    return hextoi ( &s[2] );

	while ( *s >= '0' && *s <= '9' )
	    val = val*10 + (*s++ - '0');

	return val;
}

/* If the last character of a string is
 * a newline, trim it off.
 */
void
chomp ( char *buf )
{
	char *lp;

	if ( ! buf || ! *buf )
	    return;

	while ( *buf )
	    lp = buf++;

	if ( *lp = '\n' )
	    *lp = '\0';

}

/* split a string in place.
 * tosses nulls into string, trashing it.
 */
int
split ( char *buf, char **bufp, int max )
{
	int i;
	char *p;

	p = buf;
	for ( i=0; i<max; ) {
	    while ( *p && *p == ' ' )
	    	p++;
	    if ( ! *p )
	    	break;
	    bufp[i++] = p;
	    while ( *p && *p != ' ' )
	    	p++;
	    if ( ! *p )
		break;
	    *p++ = '\0';
	}

	return i;
}

/* Take a string of the form 128.196.100.52
 * and convert it to a packed integer in network
 * byte order.
 * Note that at the present time, Kyu keeps IP addresses
 * in network byte order regardless of the host order
 * and compares them as opaque objects.
 * This may not be such a good idea and is likely
 * to be abandoned as we adopt the Xinu network code.
 */
int
net_dots ( char *buf, unsigned char *iaddr )
{
	char numstr[4];
	char *p, *np;
	int i;

	p = buf;
	np = numstr;

	for ( i=0; i<4; ) {
	    if ( *p < '0' || *p > '9' )
		return 0;
	    while ( *p && *p >= '0' && *p <= '9' ) {
		if ( np < &numstr[3] )
		    *np++ = *p++;
		else
		    return 0;
	    }
	    *np++ = '\0';

	    iaddr[i++] = atoi ( numstr );
	    np = numstr;

	    if ( ! *p )
	    	break;

	    /* skip the dot */
	    if ( *p != '.' )
		return 0;
	    p++;
	}

	if ( i != 4 )
	    return 0;

	return 1;
}

/* single line dump, byte by byte.
 *  count gives bytes.
 *  (gives true byte order).
 *  No newline at end.
 */
void
dump_v ( void *addr, int n )
{
	unsigned char *p;
	int i;

	p = (unsigned char *) addr;

	printf ( "%08x  ", (long) addr );

	for ( i=0; i<n; i++ )
	    printf ( "%02x ", *p++ );
}

/* multi line dump, byte by byte.
 *  count gives size of buffer
 */
void
dump_buf ( void *addr, int len )
{
	unsigned char *p;
	int i;

	p = (unsigned char *) addr;

	for ( i=0; i<len; i++ ) {
	    if ( (i % 16) == 0 )
		printf ( "%08x  ", (long) addr );

	    printf ( "%02x ", *p++ );

	    if ( i > 0 && ((i+1) % 16) == 0 ) {
		printf ( "\n" );
		addr += 16;
	    }
	}

	if ( (len % 16) != 0 )
	    printf ( "\n" );
}

/* multi line dump, byte by byte.
 *  count gives lines on screen.
 */
void
dump_b ( void *addr, int n )
{
	unsigned char *p;
	int i;

	p = (unsigned char *) addr;

	while ( n-- ) {
	    printf ( "%08x  ", (long) addr );

	    for ( i=0; i<16; i++ )
		printf ( "%02x ", *p++ );

	    printf ( "\n" );
	    addr += 16;
	}
}

/* multi line dump, word by word.
 *  count gives lines on screen.
 *  (may get peculiar byte swapping).
 */
void
dump_w ( void *addr, int n )
{
	unsigned short *p;
	int i;

	p = (unsigned short *) addr;

	while ( n-- ) {
	    printf ( "%08x  ", (long) addr );

	    for ( i=0; i<8; i++ )
		printf ( "%04x ", *p++ );

	    printf ( "\n" );
	    addr += 16;
	}
}

/* multi line dump, longword by longword.
 *  count gives lines on screen.
 *  (may get peculiar byte swapping).
 */
void
dump_l ( void *addr, int n )
{
	u32 *p;
	int i;

	p = (u32 *) addr;

	while ( n-- ) {
	    printf ( "%08x  ", (long) addr );

	    for ( i=0; i<4; i++ )
		printf ( "%08x ", *p++ );

	    printf ( "\n" );
	    addr += 16;
	}
}

/* For sanity when testing */
#define LIMITS

/* Dump this many words,
 * where a "word" is a 4 byte object.
 */
void
dump_ln ( void *addr, int nw )
{
#ifdef LIMITS
	if ( nw < 1 ) nw = 1;
	if ( nw > 1024 ) nw = 1024;
#endif

	dump_l ( addr, (nw+3) / 4 );
}

void
fill_l ( void *addr, u32 data, int n )
{
	u32 *p =
	    (u32 *) addr;

	while ( n-- )
	    *p++ = data;
}

#ifdef SIMPLE_SLAB
/* Provide trivial pass-through functions
 * for the linux memory allocators.
 */

void * __kmalloc ( size_t size, int prio )
{
	return malloc ( size );
}

void * kmalloc ( size_t size, int prio )
{
	return malloc ( size );
}

void * krealloc ( void *addr, size_t size, int prio )
{
	return realloc ( addr, size );
}

/* see mm/util.c */
void * kmemdup ( void *src, size_t len, int prio )
{
	void *p = malloc ( len );

	memcpy(p, src, len);
	return p;
}

void kfree ( void *addr )
{
	free ( addr );
}

kmem_cache_t * kmem_cache_create ( const char *name,
	size_t size, size_t offset, unsigned long flags,
	void (*constructor)(void *, kmem_cache_t *, unsigned long flags ),
	void (*destructor)(void *, kmem_cache_t *, unsigned long flags )
	)
{
	kmem_cache_t *cp;

	cp = malloc ( sizeof(*cp) );
	cp->size = size;
	return cp;
}

int kmem_cache_destroy ( kmem_cache_t *cp )
{
	free ( cp );
}

void *kmem_cache_alloc ( kmem_cache_t *cp, int flags )
{
	return malloc ( cp->size );
}

void *kmem_cache_free ( kmem_cache_t *cache, void * addr )
{
	free ( addr );
}
#endif

/* Allow 5 queues for now.
 * 1 for x86 keyboard
 * 2 for each of two x86 serial ports (in and out)
 * So far has only been used in x86 days.
 *
 * XXX - someday should add dynamic allocator both for
 * the control structure and the buffer itself.
 * We never expect to free these resources (maybe someday
 * if we should ever allow modular drivers, but even then
 * in a real embedded application, if you need a driver,
 * you always need it.)
 */
#define MAX_CQ		5

struct cqueue cq_table[MAX_CQ];
static int cq_next = 0;

struct cqueue *
cq_init ( int size )
{
	struct cqueue *qp;

	if ( cq_next >= MAX_CQ )
	    return (struct cqueue *) 0;

	qp = &cq_table[cq_next++];

	/* XXX - argument is ignored */
	qp->size = MAX_CQ_SIZE;
	qp->count = 0;
	qp->ip = qp->op = qp->bp = qp->buf;
	qp->limit = &qp->bp[qp->size];
	qp->toss = 0;

	return qp;
}

/* At first, was just going let folks examine
 * the count element of the structure, but
 * perhaps someday I will eliminate it and be
 * thankful for this accessor function.
 *
 * (Right now, I am keeping the count element,
 * it actually makes checks faster at interrupt
 * level unless I get a bit more clever and
 * sacrifice one element of storage.)
 * The key assertion is that *ip always points
 * to a valid place to dump a character.
 */
int
cq_count ( struct cqueue *qp )
{
	return qp->count;
#ifdef notdef
	/* works, but slower than just
	 * returning count
	 */
	if ( qp->ip < qp->op )
	    return qp->size - (qp->op - qp->ip);
	else
	    return qp->ip - qp->op;
#endif
}

/* return amount of available space in queue
 */
int
cq_space ( struct cqueue *qp )
{
	return qp->size - qp->count;
}

/* Almost certainly gets called from interrupt level.
 *	(must not block.)
 * Should surely have locks wrapped around it, in the
 * usual producer/consumer situation it is intended for.
 * (This will usually be called in an interrupt routine,
 *  where locking will be implicit.)
 */
void
cq_add ( struct cqueue *qp, int ch )
{
	if ( qp->count < qp->size ) {
	    qp->count++;
	    *(qp->ip)++ = ch;
	    if ( qp->ip >= qp->limit )
		qp->ip = qp->bp;
	} else {
	    qp->toss++;
	}
}

int
cq_toss ( struct cqueue *qp )
{
	return qp->toss;
}

/* Once upon a time, I had locking in here,
 * but now the onus is on the caller to do
 * any locking external to this facility.
 */
int
cq_remove ( struct cqueue *qp )
{
	int ch;

	if ( qp->count < 1 )
	    return -1;
	else {
	    ch = *(qp->op)++;
	    if ( qp->op >= qp->limit )
		qp->op = qp->bp;
	    qp->count--;
	}
	return ch;
}

#ifndef USE_LINUX_STR
/* gcc didn't like this prototype until the const
 *  got added to the arguments.
 *
 * (it has some idea this is a builtin function).
 */

int
strcmp ( const char *s1, const char *s2 )
{
	int c1, c2;

	while ( c1 = *s1++ ) {
	    if ( c2 = *s2++ ) {
	    	if ( c1 < c2 )
		    return 1;
	    	if ( c1 > c2 )
		    return -1;
	    } else {
	    	return 1;
	    }
	}

	if ( c2 = *s2 )
	    return -1;
	return 0;
}

int
strlen ( const char *s )
{
	int len = 0;

	while ( *s++ )
	    len++;

	return len;
}

char *
strcpy ( char *ds, const char *ss )
{
	char cc;
	char *dp = ds;

	do {
	    *dp++ = cc = *ss++;
	} while ( cc );
	return ds;
}

char *
strncpy ( char *ds, const char *ss, int n )
{
	char cc;
	char *dp = ds;

	if ( n <= 0 ) {
	    *ds = '\0';
	    return ds;
	}

	do {
	    *dp++ = cc = *ss++;
	    n --;
	} while ( n && cc );
	return ds;
}
#endif

#ifndef USE_LINUX_MEM
void *
memcpy ( void *s1, char *s2, size_t count )
{
	char *p, *end;

	p=(char *)s1;
	end = &p[count];

	while ( p<end ) 
	    *p++ = *s2++;

	return s1;
}

/* Usually just used to detect match or not */
int
memcmp ( void *s1, void *s2, size_t count )
{
	char *p, *q, *end;

	p=(char *)s1;
	q=(char *)s2;
	end = &p[count];

	while ( p < end ) {
	    if ( *p++ < *q++ )
		return -1;
	    if ( *p++ > *q++ )
		return 1;
	}

	return 0;
}


void *
memset ( void *buf, int val, size_t count )
{
	char *p, *end;

	p=(char *)buf;
	end = &p[count];

	while ( p<end ) 
	    *p++ = val;

	return buf;
}
#endif

/* Wrapper function to catch troubles when making new threads.
 */
struct thread *
safe_thr_new ( char *nm, tfptr func, void *arg, int pri, int flags )
{
	struct thread *rv;

	rv = thr_new ( nm, func, arg, pri, flags );
	if ( ! rv ) {
	    printf ( "Cannot create new thread: %s\n", nm );
	    panic ( "user thread new" );
	}

	return rv;
}

/* XXX Move this */
#ifdef ARCH_X86
#ifdef LED_DEBUG
/* ... or just call flash32() in locore.S
 */

#include "intel.h"

void
flash ( void )
{
	for ( ;; ) {
	    outb ( 0x00, 0x378 );
	    big_delay ();
	    outb ( 0x01, 0x378 );
	    big_delay ();
	}
}

void
big_delay ( void )
{
	int n = 250000;

	while ( n-- ) {
	    outb ( 0, 0x80 );
	}
}
#endif
#endif

/* THE END */
