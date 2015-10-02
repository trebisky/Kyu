/* shell.c
 * Tom Trebisky
 *
 * added to kyu 10/1/2015
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

/* ------------- */

#define SYM_SIZE       200000
static char sym_buf[SYM_SIZE];

static char *sym_file = "kyu.sym";

void
shell_init ( void )
{
	int count;

	count = tftp_fetch ( sym_file, sym_buf, SYM_SIZE );
	printf ( "fetched symbol table: %d bytes\n", count );
}

void
shell_x ( char **arg, int narg )
{
	printf ( "shell_x, narg = %d\n" );
	printf ( " arg 1 = %s\n", arg[0] );
}

/* THE END */
