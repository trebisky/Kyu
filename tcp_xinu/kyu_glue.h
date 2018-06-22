/* kyu_glue.h */

/* Define as much xinu stuff as is possible, sensible,
 * and reasonable as macros
 */

#define kprintf printf

/* semaphores */

#define wait	sem_block
#define signal	sem_unblock

#define semdelete	sem_destroy

/* XXX - we don't implement or need this yet */
#define semreset(x,y)	panic ( "semreset not implemented" );

/* THE END */
