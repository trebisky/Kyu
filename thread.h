/* thread.h
 * $Id: thread.h,v 1.7 2002/12/24 04:08:20 tom Exp $
 * T. Trebisky  8/25/2002
 */

#ifndef NULL
#define NULL	(0)
#endif

enum console_mode { SERIAL, VGA, SIO_0, SIO_1 };

typedef void (*tfptr) ( int );
typedef void (*vfptr) ( void );

/* priorities >= MAGIC are for special use.
 * (and I will probably reserve single digit
 *  priorities similarly.)
 * No priority *ever* gets set as high as MAX_PRI
#define		PRI_USER	950
 */
#define		PRI_SYS		5
#define		PRI_USER	10
#define		PRI_MAGIC	900
#define		MAX_PRI		1001

struct thread *thr_new ( char *, tfptr, void *, int, int );
struct thread *thr_self ( void );
void thr_kill ( struct thread * );
void thr_exit ( void );

struct thread * safe_thr_new ( char *, tfptr, void *, int, int );

/* flags for thr_new:
 */

#define	TF_BLOCK	0x0001
#define	TF_FPU		0x0002
#define	TF_JOIN		0x0004

/* flags for sem_new:
 */
#define	SEM_FIFO	0x0000
#define	SEM_PRIO	0x0001

enum sem_state { CLEAR, SET };

struct sem *sem_new ( enum sem_state, int );
struct sem *sem_mutex_new ( int );
struct sem *sem_signal_new ( int );

void sem_block ( struct sem * );
void sem_unblock ( struct sem * );

struct cv *cv_new ( struct sem * );
void cv_wait ( struct cv * );
void cv_signal ( struct cv * );

struct sem * cpu_new ( void );
void cpu_wait ( struct sem * );
void cpu_signal ( struct sem * );

struct sem * safe_sem_new ( int );

#define MAX_TNAME	6

/* This is what is saved by save_t/restore_t
 * if the size of this changes, you must fiddle
 * some constants in locore.S so that the next
 * batch of registers get found properly.
 */
struct jmp_regs {
	int ebx;
	int esp;
	int ebp;
	int esi;
	int edi;
	int eip;	/* We need this */
};

/* We don't need eip here, since ...
 * well things are just different here.
 */
struct int_regs {
	int eax;
	int ebx;
	int ecx;
	int edx;
	int esi;
	int edi;
	int ebp;
	int esp;
};

enum thread_state {
	READY,		/* ready to go */
	WAIT,		/* blocked */
	SWAIT,		/* blocked on semaphore */
	DELAY,		/* blocked on timer event */
	IDLE,		/* running idle loop */
	JOIN,		/* waiting to join somebody */
	ZOMBIE,		/* waiting to be joined */
	FAULT,		/* did something bad */
	DEAD		/* on free list */
};

enum thread_mode { JMP, INT, CONT };

struct thread {
	struct int_regs iregs;
	struct jmp_regs regs;
	/* -- everthing before there must be kept in
	 *    a stict order (and size) since assembly
	 *    language routines have hard coded offsets
	 *    that expect a certain layout.
	 */
	int prof;
	struct thread *next;		/* all threads */
	struct thread *wnext;		/* waiting list */
	enum thread_state state;
	enum thread_mode mode;		/* how to resume */
	enum console_mode con_mode;	/* type of console */
	char *stack;
	int stack_size;
	int pri;
	int delay;
	int fault;		/* why we are suspended */
	char name[MAX_TNAME];
#ifdef notdef
	tfptr c_func;		/* continuation function */
	int c_arg;		/* continuation argument */
#endif
	int flags;
	struct thread *join;	/* who wants to join us */
	struct thread *yield;	/* who yielded to us */
};

/* Here are fault codes (kind of like errno)
 * XXX - maybe this should be an enum.
 */

#define FA_NIL		0
#define FA_ZDIV		1

/* ---------------------------------------------------------
 */

struct sem {
	struct sem *next;	/* links together avail */
	enum sem_state state;	/* full or empty */
	int flags;
	struct thread *wait;	/* folks blocked on this */
};

struct cv {
	struct cv *next;
	struct sem *signal;
	struct sem *mutex;
};

/* THE END */
