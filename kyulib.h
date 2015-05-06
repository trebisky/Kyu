/* kyulib.h
 *	Tom Trebisky 12/1/2001
 */

void dump_w ( void *, int );
void dump_l ( void *, int );

int strcmp ( const char *, const char * );

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
