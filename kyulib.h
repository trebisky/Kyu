/* kyulib.h
 *	Tom Trebisky 12/1/2001
 */

void dump_w ( void *, int );
void dump_l ( void *, int );

/* We rely on the gcc builtins for these,
 * but need to provide prototypes.
 */
typedef int size_t;

#ifdef notyet
#define memcpy(x,y,z) __builtin_memcpy((x), (y), (z))
#endif

/* This whole gcc builtin thing is a pain in the butt.
 * We would like to use the compiler builtins, but
 * getting it all straight is a big headache right now.
 * tjt 5-39-2015
 */
void *memset( void *s, int c, size_t n );
int strcmp ( const char *, const char * );
char *strcpy ( char *, const char * );
char *strncpy( char *, const char *, size_t );

/* We provide these (at least thus far */
int printf(const char *, ...);
int sprintf(char *, const char *, ...);
/*
int snprintf(char *str, size_t size, const char *format, ...);
*/


#ifdef LED_DEBUG
void flash ( void );
void big_delay ( void );
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
