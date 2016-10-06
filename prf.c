/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* prf.c
 * $Id: prf.c,v 1.7 2002/12/22 01:31:16 tom Exp $
 *
 * Began as scaled down printf from the BSD code.
 * Taken from my Miniframe project (1991).
 * Beaten on heavily to support sprintf and such.
 * Tom Trebisky  9/23/2001
 * Tom 8/09/2002 added varargs: vsnprintf
 * Tom split the real printf off into console.c
 */
#include <stdarg.h>
#include <kyulib.h>

/* these hex functions added by tjt  3/2/91
 */
#define HEX(x)	((x)<10 ? '0'+(x) : 'A'+(x)-10)

/* ----------------------------------------------- */
/* Now start the vsnprintf/vsprintf/sprintf family */
/* ----------------------------------------------- */

#ifdef notdef
#define PUTCHAR(x)	*buf++ = x
#endif

#define PUTCHAR(x)	if ( buf <= end ) *buf++ = (x)

static char *
shex2( char *buf, char *end, int val )
{
	PUTCHAR( HEX((val>>4)&0xf) );
	PUTCHAR( HEX(val&0xf) );
	return buf;
}

static char *
shex3( char *buf, char *end, int val )
{
	PUTCHAR( HEX((val>>8)&0xf) );
	return shex2(buf,end,val);
}

static char *
shex4( char *buf, char *end, int val )
{
	buf = shex2(buf,end,val>>8);
	return shex2(buf,end,val);
}

static char *
shex8( char *buf, char *end, int val )
{
	buf = shex2(buf,end,val>>24);
	buf = shex2(buf,end,val>>16);
	buf = shex2(buf,end,val>>8);
	return shex2(buf,end,val);
}

#ifdef DESPERATE_TIMES
/* This subterfuge was used when we patched in a vsnprintf()
 * from the linux sources and it went berzerk due to a
 * faulty macro definition.
 * The idea here is to have a working fallback version
 * while we debug the defective one.
 */
#undef USE_LINUX_PRF
// #define VSNPRINTF	vsnprintf
#define VSNPRINTF	xsnprintf

#endif

/* We now use routines in linux/lib
 */
#ifndef USE_LINUX_PRF

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static char *
sprintn ( char *buf, char *end, int n, int b)
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && n < 0) {
	    PUTCHAR('-');
	    n = -n;
	}
	cp = prbuf;

	do {
	    *cp++ = "0123456789ABCDEF"[n%b];
	    n /= b;
	} while (n);

	do {
	    PUTCHAR(*--cp);
	} while (cp > prbuf);

	return buf;
}

/*
 * as above, but unsigned
 * presently always called with b = 10
 */
static char *
sprintu( char *buf, char *end, unsigned int n, int b)
{
	char prbuf[11];
	register char *cp;

	cp = prbuf;

	do {
	    *cp++ = "0123456789ABCDEF"[n%b];
	    n /= b;
	} while (n);

	do {
	    PUTCHAR(*--cp);
	} while (cp > prbuf);

	return buf;
}

/* -------------------------------------------------- */

static int
snprintf(char * buf, unsigned int size, const char *fmt, ...)
{
        va_list args;
        int rv;

        va_start(args, fmt);
        rv = vsnprintf(buf,size,fmt,args);
        va_end(args);
        return rv;
}

#ifdef notdef
static int
sprintf(char *buf, const char *fmt, ...)
{
        va_list args;
        int rv;

        va_start(args, fmt);
        rv = vsprintf(buf,fmt,args);
        va_end(args);
        return rv;
}
#endif

static int
vsprintf(char *buf, const char *fmt, va_list args)
{
	/*
        return vsnprintf(buf, 0xFFFFFFFFUL, fmt, args);
	*/
        return vsnprintf(buf, 0x0FFFFFFFUL, fmt, args);
}

/* XXX size should/could really be size_t
 * (which is unsigned long/int)
 */
static int
VSNPRINTF (char *abuf, unsigned int size, const char *fmt, va_list args)
{
    char *buf, *end;
    int c;
    int i, b;
    char *s, *p;
    int prenum;		/* width prefix as in %8s */
    int prenum2;	/* precision as in %2.2X (ignored) */
    int zero_flag;	/* XXX - %08d and such, not yet */
    int long_flag;	/* XXX - %ld and such, not yet */
    int alt_flag;	/* XXX - %#x and such, not yet */

    buf = abuf;
    end = buf + size - 1;
    if (end < buf - 1) {
	end = ((void *) -1);
	size = end - buf + 1;
    }

    while ( c = *fmt++ ) {

	if ( c != '%' ) {
	    PUTCHAR(c);
	    continue;
	}

	zero_flag = 0;
	long_flag = 0;
	alt_flag = 0;

	/* for %#x, this means prepend 0x
	 * (we could just ignore it).
	 */
	if ( *fmt == '#' ) {
	    alt_flag = 1;
	    fmt++;
	}

	/* OK, now look for a digit sequence after the
	 * '%' and lump them into "prenum"
	 */
	prenum = 0;
	while ( *fmt >= '0' && *fmt <= '9' ) {
	    prenum = prenum*10 + ( *fmt - '0');
	    if ( prenum == 0 )
	    	zero_flag = 1;
	    fmt++;
	}

	/* Added 8/24/2002, if we see a period, then
	 * consume a second prenum (and we just ignore it).
	 * XXX - The first prenum should be called "width",
	 * and this second one "precision".
	 */
	if ( *fmt == '.' ) {
	    fmt++;
	    prenum2 = 0;
	    while ( *fmt >= '0' && *fmt <= '9' ) {
		prenum2 = prenum2*10 + ( *fmt - '0');
		fmt++;
	    }
	}

	/* Just ignore this,
	 * long and int are the same
	 */
	if ( *fmt == 'l' ) {
	    long_flag = 0;
	    fmt++;
	}

	/* Ignore this too,
	 * Z and z prefix are for size_t and
	 * tend to come as %Zd.
	 * size_t is unsigned int in linux.
	 */
	if ( *fmt == 'z' || *fmt == 'Z' ) {
	    long_flag = 0;
	    fmt++;
	}

	/* format ended early.
	 */
	if ( ! *fmt ) {
	    PUTCHAR ( '%' );
	    return buf-abuf;
	}

	c = *fmt++;

	switch (c) {

	case 'x': case 'X':
	    if ( prenum == 2 )
		buf = shex2(buf,end,va_arg(args,int));
	    else if ( prenum == 3 )
		buf = shex3(buf,end,va_arg(args,int));
	    else if ( prenum == 4 )
		buf = shex4(buf,end,va_arg(args,int));
	    else if ( prenum == 8 )
		buf = shex8(buf,end,va_arg(args,int));
	    else {
		buf = shex8(buf,end,va_arg(args,int));
		/*
		buf = sprintn(buf,end,va_arg(args,int), 16);
		*/
	    }
	    break;

	case 'd': case 'D':
	    buf = sprintn(buf,end,va_arg(args,int), 10);
	    break;

	case 'u':
	    buf = sprintu(buf,end,va_arg(args,int), 10);
	    break;

	case 'o': case 'O':
	    buf = sprintn(buf,end,va_arg(args,int), 8);
	    break;

	/* XXX - This %c may print more than one char.
	 */
	case 'c':
	    b = va_arg(args,int);
	    for (i = 24; i >= 0; i -= 8)
		if (c = (b >> i) & 0x7f)
		    PUTCHAR(c);
	    break;

	case 's':
	    p = s = va_arg(args,char *);
	    /*
	    printf ( "TJT sprintf: %08x", p );
	    printf ( "%s\n", p );
	    */
	    if ( prenum ) {
	    	i = 0;
		while ( *p++ )
		    i++;
		while ( i++ < prenum )
		    PUTCHAR ( ' ' );
	    }
	    while (c = *s++)
		PUTCHAR(c);
	    break;
	}
    }

    if (buf <= end)
	*buf = '\0';
    else if (size > 0)
	*end = '\0';

    return buf-abuf;
}
#endif	/* USE_LINUX_PRF */

#define PRINTF_BUF_SIZE 128

int
printf ( const char *fmt, ... )
{
	char buf[PRINTF_BUF_SIZE];
	va_list args;
	int rv;

	va_start ( args, fmt );
	rv = vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
	va_end ( args );

	console_puts ( buf );
	return rv;
}

#ifdef DESPERATE_TIMES
/* Alternative to printf while debugging new printf */
int
xprintf ( const char *fmt, ... )
{
	char buf[PRINTF_BUF_SIZE];
	va_list args;
	int rv;

	va_start ( args, fmt );
	rv = xsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
	va_end ( args );

	console_puts ( buf );
	return rv;
}
#endif

/* ------------------------------------------ */
/* ------------------------------------------ */

/* simple and quick version to get some linux
 * library routines online  6-11-2015
 *
 * Note that the real linux version does lots
 * of nice and fancy things.  It puts messages
 * into a circular buffer, perhaps sends them
 * to syslog, watches for ugly scenarios, all
 * desirable in a big system.
 *
 * see linux/printk.h and kernel/printk/prink.c
 *
 * Also note that linux messages may be prefixed
 * by a 3 character logging priority that looks like
 * <x> where x is 0-7 or "d" to get the current default.
 * XXX - We could at least strip this off.
 */

#ifdef notdef
int
printk ( const char *fmt, ... )
{
	char buf[PRINTF_BUF_SIZE];
	va_list args;
	int rv;

	va_start ( args, fmt );
	rv = vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
	va_end ( args );

	console_puts ( buf );
	return rv;
}
#endif

/* THE END */
