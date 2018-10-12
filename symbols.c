/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* shell.c
 * Tom Trebisky
 *
 * added to kyu 10/1/2015
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"

#include "board/board.h"

/* ------------- */

#define SYM_SIZE       200000

/*
static char sym_buf[SYM_SIZE];
*/
static char *sym_buf;
static char *sym_last;
static char *symp;

static char *sym_file = "kyu.sym";

/* ------------- */

struct symtab {
	unsigned long addr;
	char *name;
	struct symtab *next;
};

static struct symtab *sym_table;

static int
getc ( void )
{
	if ( symp < sym_last )
	    return *symp++;
	return 999;
}

static int
getl ( char *buf, int nbuf )
{
	int n = 0;
	int c;

	while ( n < nbuf - 1 ) {
	    c = getc();
	    /* printf ( "getc %d\n", c ); */
	    if ( c == 999 || c == '\n' ) {
		*buf++ = '\0';
		return n;
	    }
	    *buf++ = c;
	    n++;
	}

	*buf = '\0';
	for ( ;; ) {
	    c = getc();
	    if ( c == 999 || c == '\n' )
		break;
	}
	return n;
}

static char *
strhide ( char *x )
{
	char *p = malloc ( strlen(x) + 1 );
	strcpy ( p, x );
	return p;
}

static int nsym = 0;

void
add_symbol ( int addr, char *name )
{
	struct symtab *sp;

	/*
	printf ( "%08x %s\n", addr, name );
	*/

	sp = malloc ( sizeof(struct symtab) );
	sp->addr = addr;
	sp->name = strhide ( name );
	sp->next = sym_table;
	sym_table = sp;

	nsym++;
}

struct symtab *
sym_lookup ( char *x )
{
	struct symtab *sp;

	for ( sp = sym_table; sp; sp = sp->next ) {
	    /* printf ( "checking %s %08x\n", sp->name, sp->addr ); */
	    if ( strcmp ( x, sp->name ) == 0 )
		return sp;
	}
	return NULL;
}

/* Do a reverse lookup, given an address,
 *  find the entry less than, but closest to the address given.
 */
struct symtab *
sym_rlookup ( int addr )
{
	struct symtab *sp;
	struct symtab *best_sp;
	int best, gap;

	best = 0x10000000;
	best_sp = sym_table;

	for ( sp = sym_table; sp; sp = sp->next ) {
	    /* printf ( "checking %s %08x\n", sp->name, sp->addr ); */
	    gap = addr - sp->addr;
	    if ( gap < 0 )
		continue;
	    if ( gap < best ) {
		best = gap;
		best_sp = sp;
	    }
	}
	return best_sp;
}

static char no_sym[] = "-?-";

char *
mk_symaddr ( int addr )
{
	struct symtab *sp;
	int off;
	static char rbuf[32];	/* XXX */

	/* This can get called early in the game
	 *  before there is a symbol table loaded.
	 */
	if ( ! sym_table )
	    return no_sym;

	sp = sym_rlookup ( addr );
	off = addr - sp->addr;
	if ( off ) {
	    sprintf ( rbuf, "%s+%x", sp->name, off );
	    return rbuf;
	}

	/* every proper trace ends in thr_exit */
	return sp->name;
}

#define NBUF	128
#define MAXW	4

static void
parse_table ( int count )
{
	char line[NBUF];
	char *wp[MAXW];
	unsigned long addr;
	int nw;

	nsym = 0;
	sym_table = NULL;

	if ( count > 0 ) {

	    symp = sym_buf;
	    sym_last = &sym_buf[count];

	    while ( getl ( line, NBUF ) ) {
		nw = split ( line, wp, MAXW );
		if ( *wp[1] == 'T' || *wp[1] == 't'  )
		    add_symbol ( hextoi(wp[0]), wp[2] );
	    }
	}

	printf ( "%d symbols\n", nsym );
}

void
shell_init ( void )
{
	int count;

	if ( ! net_running() ) {
	    parse_table ( 0 );
	    return;
	}

#ifdef WANT_SYMBOLS
	sym_buf = malloc ( SYM_SIZE );
	count = tftp_fetch ( sym_file, sym_buf, SYM_SIZE );
	printf ( "fetched symbol table: %d bytes\n", count );
	parse_table (count);
	free ( sym_buf );
#endif
}

/* --------------- */

static int
is_number ( char *s )
{
	int c;

	while ( c = *s++ ) {
	    if ( c < '0' || c > '9' )
		return 0;
	}
	return 1;
}

/* This handler is the bottom line once things are all set up.
 *
 * A command "x fish" calls fish(0);
 * A command "x fish 12" calls fish(12);
 * A command "x fish rat" calls fish("rat");
 *
 * At some point I will change the parset so that the leading
 * "x" is not required (and make the current "test" fixture
 * an optional thing off to the side.
 */

void
shell_x ( char **arg, int narg )
{
	struct symtab *sp;
	void (*ifp) ( int );
	void (*sfp) ( char * );

	printf ( "shell_x, narg = %d\n" );

	if ( narg < 1 )
	    return;

	sp = sym_lookup ( arg[0] );
	if ( ! sp ) {
	    printf ( "%s not found\n", arg[0] );
	    return;
	}

	printf ( "Call to address: %08x\n", sp->addr );

	if ( narg == 1 ) {
	    ifp = (void (*)(int)) sp->addr;
	    (*ifp)(0);
	    return;
	}

	/* OK, we have an argument, what flavor? */
	if ( is_number ( arg[1] ) ) {
	    printf ( " arg num = %d\n", atoi ( arg[1] ) );
	    ifp = (void (*)(int)) sp->addr;
	    (*ifp)( atoi(arg[1]));
	} else {
	    printf ( " arg string = %s\n", arg[1] );
	    sfp = (void (*)(char *)) sp->addr;
	    (*sfp)(arg[1]);
	}
}

/* XXX very much ARM specific */
#include "hardware.h"
#include "arm/cpu.h"

extern char * stack_start;

/* Do a stack traceback -- ARM specific
 */
void
unroll_fp ( reg_t *fp )
{
	int limit;
	char *msg;

	/* could also check is fp ever moves to lower addresses on stack and stop */
	limit = 16;
	while ( limit > 0 && fp ) {
	    // if ( fp[0] < THR_STACK_BASE ) {
	    if ( (unsigned long) fp[0] < (unsigned long) stack_start ) {
		msg = mk_symaddr ( fp[0] );
		printf ( "Called from %s -- %08x\n", msg, fp[0] );
		fp = (reg_t *) fp[-1];
	    } else {
		printf ( "Leaf routine\n" );
		fp = (reg_t *) fp[0];
	    }
	    // printf ( "Called from %s -- %08x, (next fp = %08x)\n", msg, fp[0], fp[-1] );
	    limit--;
	}
}

/* Do a stack backtrace on current thread,
 * along with dumping a fair bit of the stack.
 * XXX - what about all registers ??
 * what if mode is not JMP for current thread ???
 */
void
unroll_cur ( void )
{
	reg_t *fp;
	unsigned int sp;
	char stbuf[16];
	struct thread *cp;

	get_SP ( sp );

	// fp = (int *) get_fp ();
	get_FP ( fp );

	cp = thr_self ();
	printf ( "Current thread is %s\n", cp->name );
	printf ( " SP = %08x,  FP = %08x\n", sp, fp );

	sprintf ( stbuf, "%08x", sp );
	mem_dumper ( 'l', stbuf, "16" );

	unroll_fp ( fp );
}

/* THE END */
