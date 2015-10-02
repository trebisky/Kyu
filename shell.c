/* shell.c
 * Tom Trebisky
 *
 * added to kyu 10/1/2015
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"

/* ------------- */

#define SYM_SIZE       200000
static char sym_buf[SYM_SIZE];
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

#define NBUF	128
#define MAXW	4

static void
parse_table ( int count )
{
	char line[NBUF];
	char *wp[MAXW];
	unsigned long addr;
	int nw;

	symp = sym_buf;
	sym_last = &sym_buf[count];

	nsym = 0;
	sym_table = NULL;

	while ( getl ( line, NBUF ) ) {
	    nw = split ( line, wp, MAXW );
	    if ( *wp[1] != 'T' )
		continue;
	    add_symbol ( hextoi(wp[0]), wp[2] );
	}
	printf ( "%d symbols\n", nsym );
}

void
shell_init ( void )
{
	int count;

	count = tftp_fetch ( sym_file, sym_buf, SYM_SIZE );
	printf ( "fetched symbol table: %d bytes\n", count );
	parse_table (count);
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

void
shell_x ( char **arg, int narg )
{
	struct symtab *sp;

	printf ( "shell_x, narg = %d\n" );

	if ( narg < 1 )
	    return;

	sp = sym_lookup ( arg[0] );
	if ( ! sp ) {
	    printf ( "%s not found\n", arg[0] );
	    return;
	}

	printf ( "Call to address: %08x\n", sp->addr );

	if ( narg == 1 )
	    return;

	if ( is_number ( arg[1] ) )
	    printf ( " arg num = %d\n", atoi ( arg[1] ) );
	else
	    printf ( " arg string = %s\n", arg[1] );
}

/* THE END */
