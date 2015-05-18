/* thread.c
 * Kyu project
 * T. Trebisky 8/25/2002 5/14/2015
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"

/* XXX - still doesn't work (w/ yield).
#define SORT_PRI
*/

static struct thread threads[MAX_THREADS];

/* This is a bit of a hack, but very important.
 * during thread system initialization I
 * need to start several special threads at
 * "prohibited" priority levels, but after that I
 * set this variable to make it impossible
 * to start threads at these levels.
 * Also ... this makes sure these special threads
 * do not actually launch until the thread system
 * is fully initialized.
 */
static int threads_running;

static enum console_mode first_console = INITIAL_CONSOLE;

struct thread *cur_thread;

/* list of thread structures to recycle
 */
static struct thread *thread_avail;

/* list of active threads
 */
static struct thread *thread_ready;

/* change via thr_debug()
 */
static int thread_debug;

static char *stack_next;
static int stack_limit;

extern long in_interrupt;
extern struct thread *in_newtp;

/* Here is an informal rule,
 * entry point names we export to the world
 * begin with "thr_", others have non-prefixed
 * names (and are static in this module).
 */
void thr_init ( void );
void thr_exit ( void );
void thr_yield ( void );
void thr_block ( enum thread_state );
void thr_unblock ( struct thread * );

void change_thread_int ( struct thread * );
static void change_thread ( struct thread *, int );
static void resched ( int );
static void setup_c ( struct thread *, tfptr, void * );

void sys_init ( int );

void thr_show ( void );

/* These names all stink!
 */
static void thr_one ( struct thread * );
void thr_one_all ( struct thread *, char *msg );
static void thr_show_state ( struct thread * );

/* policy options for resched ()
 */
#define RSF_YIELD	0x01	/* try not to run me */
#define RSF_CONT	0x02
#define RSF_INTER	0x04

void sem_init ( void );
int sem_block_t ( struct sem *, int );
void sem_block_m ( struct sem *, struct sem * );

struct stack_list {
	struct stack_list *next;
	int size;
};

static struct stack_list *stack_avail;

/* allocate a thread stack.
 */
static char *
thr_alloc ( int size )
{
	char *stack;
	struct stack_list *xp, *lp;

	/* XXX - Right now the size must be an exact match.
	 * (this is not a problem since all stacks now have
	 *  the same size).
	 */
	for ( xp = stack_avail; xp; xp = xp->next ) {
	    if ( xp->size == size ) {
		if ( xp == stack_avail )
		    stack_avail = xp->next;
		else
		    lp->next = xp->next;
			
	    	return (char *) xp;
	    }
	    lp = xp;
	}

	if ( size > stack_limit )
	    return (char *) 0;

	stack = stack_next;
	stack_limit -= size;
	stack_next += size;

	return stack;
}

/* free a thread stack.
 */
static void
thr_free ( char *stack, int size )
{
	struct stack_list *xp;

	xp = (struct stack_list *) stack;
	xp->next = stack_avail;
	xp->size = size;
	stack_avail = xp;
}

void
thr_init ( void )
{
	struct thread *tp;
	int i;

	thread_debug = DEBUG_THREADS;
	threads_running = 0;

	sem_init ();

	/* XXX */
	stack_next = (char *) THR_STACK_BASE;
	stack_limit = THR_STACK_LIMIT;
	stack_avail = (struct stack_list *) 0;

	/* put a few structures on the available list.
	 * XXX - someday this static allocation will be
	 * replaced with something dynamic.
	 */
	thread_avail = (struct thread *) 0;
	for ( i=0; i<MAX_THREADS; i++ ) {
	    tp = &threads[i];
	    tp->next = thread_avail;
	    thread_avail = tp;
	}

	/* XXX to make initial debug pretty.
	 */
	cur_thread = (struct thread *) 0;

	thread_ready = (struct thread *) 0;

	/*
	stack_db ( "thread kickoff" );
	*/

	/* let this be the initial thead.
	 */
	cur_thread = thr_new ( "sys", sys_init, (void *) 0, PRI_SYS, 0 );
}

/* OK, I have gotten tired of uncommenting and
 * recommenting statements as I am testing, so
 * I am introducing this.
 */
void
thr_debug ( int value )
{
	thread_debug = value;
}

/* OK, kick off the thread system.
 * At this point we are running in a unique stack context
 *  (belonging to no thread),
 *  at 0x8fff0 or so on the X86
 *  at 0x980003f8 on the ARM (BBB)
 *
 * We want to run a real thread.
 */
void
thr_sched ( void )
{
	if ( thread_debug )
	    printf ( "thr_sched called: %d\n", threads_running );

	/* should never be called twice.
	 */
	if ( threads_running )
	    panic ( "thr_sched" );

	threads_running = 1;

	/* launch the system by running the
	 * current thread
	 */
	 printf ( "thr_sched launching first thread %08x\n", &cur_thread->cregs ); 

#ifdef ARCH_X86
	resume_c ( cur_thread->regs.esp );
#endif
#ifdef ARCH_ARM
	resume_c ( &cur_thread->cregs );
#endif
	/* NOTREACHED */
}

void
thr_one_all ( struct thread *tp, char *msg )
{
	printf ( "Thread %s, dump (%s)\n", tp->name, msg );
	thr_one ( tp );

	printf ( " state: " );
	thr_show_state ( tp );
	printf ( "\n" );

	printf ( " stack: %8x\n", (int) tp->stack );
#ifdef ARCH_X86
	printf ( " esp: %8x\n", tp->regs.esp );
	printf ( " eip: %8x\n", tp->regs.eip );
	printf ( " esi: %8x\n", tp->regs.esi );
	printf ( " edi: %8x\n", tp->regs.edi );
	printf ( " ebp: %8x\n", tp->regs.ebp );
	printf ( " ebx: %8x\n", tp->regs.ebx );
#endif
	printf ( "\n" );
}

void
thr_show_name ( char *name )
{
	struct thread *tp;
	int ok = 0;

        for ( tp=thread_ready; tp; tp = tp->next ) {
	    if ( strcmp ( tp->name, name ) == 0 ) {
		thr_one_all ( tp, "-" );
		ok = 1;
	    }
	}
	if ( ! ok )
		printf ( "Oops, no such thread\n" );
}

static void
thr_show_state ( struct thread *tp )
{
	switch ( tp->state ) {
	    case READY:
		printf ( "  READY " );
		break;
	    case WAIT:
		printf ( "   WAIT " );
		break;
	    case SWAIT:
		printf ( "    SEM " );
		break;
	    case DELAY:
		printf ( "  DELAY " );
		break;
	    case JOIN:
		printf ( "   JOIN " );
		break;
	    case ZOMBIE:
		printf ( " ZOMBIE " );
		break;
	    case IDLE:
		printf ( "   IDLE " );
		break;
	    case FAULT:
		printf ( "  FAULT " );
		break;
	    case DEAD:
		printf ( "   DEAD " );
		break;
	    default:
		printf ( "    ?%d? ", tp->state );
		break;
	}
}

static void
thr_one ( struct thread *tp )
{
	if ( ! tp ) {
	    printf ( "  NULL!\n");
	    return;
	}

	if ( tp == cur_thread )
	    printf ( "* " );
	else
	    printf ( "  " );

	printf ( "Thread: %5s ", tp->name );
	printf ( "(%8x) ", tp );

	thr_show_state ( tp );

	if ( tp->mode == JMP )
	    printf ( "J ");
	else if ( tp->mode == INT )
	    printf ( "I ");
	else if ( tp->mode == CONT )
	    printf ( "C ");
	else
	    printf ( "%d ", tp->mode );

#ifdef ARCH_X86
	printf ( "%8x %4d\n", tp->regs.esp, tp->pri );
#else
	printf ( "%8x %4d\n", 0, tp->pri );	/* XXX */
#endif
}

/* PUBLIC - show all threads.
 */
void
thr_show ( void )
{
	struct thread *tp;

	printf ( "  Thread:  name (  &tp   )  state     esp     pri\n");
	/*
	thr_one ( thread0 );
	*/

	for ( tp=thread_ready; tp; tp = tp->next )
	    thr_one ( tp );

	printf ( "  Thread  Cur : (%08x)", cur_thread);
	if ( in_interrupt )
	    printf ( " (INT)\n" );
	else
	    printf ( "\n" );
}

void
thr_show_current ( void )
{
	thr_one_all ( cur_thread, "cur" );
#ifdef ARCH_X86
	dump_l ( (void *)cur_thread->regs.esp - 16, 4 );
#endif
}

/* goofy routines to handle thread names.
 */

static void
thrcp ( char *place, char *str, int limit )
{
	while ( limit-- && *str ) {
	    *place++ = *str++;
	}
	*place = '\0';
}

static int thr_id = 0;

static char *
thrnm_i ( char *place, int n )
{
	if ( n < 10 ) {
	    *place++ = '0' + n;
	} else {
	    place = thrnm_i ( place, n/10 );
	    *place++ = '0' + n%10;
	}
	return place;
}

/* Dynamically generate a thread ID
 */
static void
thrnm ( char *place )
{
	++thr_id;
	*place++ = 't';
	place =  thrnm_i ( place, thr_id );
	*place = '\0';
}

/* Setup a thread to come alive as a continuation
 */
static void
setup_c ( struct thread *tp, tfptr func, void *arg )
{
	long *estack;

	/* Set up the stack for the x86.
	 * We need to preload 3 words on it as follows:
	 *
	 * 1 - a place for the thread function
	 *	to return into, should it return.
	 * 2 - the argument to the thread function.
	 * 3 - the pointer to the thread function.
	 * 
	 * On the X86 to continue (resume) the thread,
	 * we push a 3 word context for an IRET on the
	 * stack above this.
	 * (the IRET will consume this, leaving things so
	 * a RET will use word 1 and go to thr_exit()
	 * the return function is always thr_exit().
	 *
	 * The argument and continuation function
	 * pointer can be (and must be able to be)
	 * used again and again.
	 */
	estack = (long *) &tp->stack[tp->stack_size];

#ifdef notdef
	printf ( "setup_c: estack at %08x\n", estack );
#endif

#ifdef ARCH_X86
	estack -= 3;
	/* any thread that returns should fall into exit */
	estack[0] = (long) thr_exit;
	estack[1] = (long) arg;
	estack[2] = (long) func;

	tp->regs.esp = (int) estack;
	tp->regs.eip = (int) 0;	/* XXX completely bogus */
#endif

#ifdef ARCH_ARM
	/* This is setup for the ldmia
	 * in resume_c (in locore.S)
	 */
	tp->cregs.regs[0] = (long) arg;
	tp->cregs.regs[1] = (long) func;
	tp->cregs.regs[2] = (long) estack;
	tp->cregs.regs[3] = (long) thr_exit;
#endif

	tp->mode = CONT;
}

/* PUBLIC - make and start a new thread.
 */
struct thread *
thr_new ( char *name, tfptr func, void *arg, int prio, int flags )
{
	struct thread *tp;
	char *stack;
	struct thread *p, *lp;

	if ( flags & TF_FPU )
	    panic ( "thr_new no floating point yet" );

	/* first get the stack.
	 */
	if ( ! (stack = thr_alloc(STACK_SIZE)) ) {
	    /* XXX - bad bugs come when we
	     * don't check return values.
	     */
	    panic ( "thread stacks all gone" );
	    return (struct thread *) 0;
	}

#ifdef notdef
	/* XXX */
	fill_l ( stack, 0x0f0f0e0e, STACK_SIZE/sizeof(long) );
	printf ( "thr_new: stack at %08x\n", stack );
#endif

	if ( ! thread_avail ) {
	    thr_free ( stack, STACK_SIZE );
	    /* XXX - bad bugs come when we
	     * don't check return values.
	     */
	    panic ( "threads all gone" );
	    return (struct thread *) 0;
	}

	/* grab the available thread
	 */
	tp = thread_avail;
	thread_avail = tp->next;

	/* XXX - be sure nobody is less urgent than
	 * the idle thread (or they never run).
	 * although I don't use it now, I also
	 * reserve the single digit "hot" priorities.
	 */
	if ( threads_running ) {
	    if ( prio >= PRI_MAGIC )
		prio = PRI_MAGIC-1;
	    if ( prio < 10 )
		prio = 10;
	}

	tp->stack = stack;
	tp->stack_size = STACK_SIZE;
	if ( name )
	    thrcp ( tp->name, name, MAX_TNAME-1 );
	else
	    thrnm ( tp->name );

	tp->pri = prio;
	tp->prof = 0;
	tp->flags = flags;
	tp->join = (struct thread *) 0;
	tp->yield = (struct thread *) 0;

	/* Inherit console from creating thread,
	 * unless first thread.
	 */
	if ( cur_thread )
	    tp->con_mode = cur_thread->con_mode;
	else
	    tp->con_mode = first_console;

	/* Threads come alive as a continuation,
	 */
	setup_c ( tp, func, arg );

	if ( flags & TF_BLOCK )
	    tp->state = WAIT;
	else
	    tp->state = READY;

#ifdef notdef
	tp->next = thread_ready;
	thread_ready = tp;
#endif

	/* insert into list, keeping
	 * in sorted order.
	 */
	p = thread_ready;

	while ( p && p->pri <= tp->pri ) {
	    lp = p;
	    p = p->next;
	}

	tp->next = p;

	if ( p == thread_ready )
	    thread_ready = tp;
	else
	    lp->next = tp;

	if ( thread_debug ) {
	    printf ( "starting thread: %s\n", tp->name );
	    /*
	    thr_one_all ( tp, "starting" );
	    dump_l ( estack-1, 3 );
	    dpanic ( "starting" );
	    */
	}

	/* Since we have just introduced a new
	 * thread into the mix, we should run
	 * it RIGHT NOW, if it is more urgent
	 * than the current thread.
	 */

	/* We don't have a current thread the first
	 *  time this is called.
	 */
	if ( ! cur_thread || ! threads_running )
	    return tp;

	if ( tp->pri < cur_thread->pri && tp->state == READY )
	    change_thread ( tp, 0 );

	return tp;
}

/* PUBLIC - find out who I am,
 *  I don't want to export cur_thread to the world ...
 */
struct thread *
thr_self ( void )
{
	return cur_thread;
}

/* Common code for thr_exit() and thr_join()
 */
static void
cleanup_thread ( struct thread *xp )
{
	struct thread *tp;

	/* release stack memory.
	 */
	thr_free ( xp->stack, xp->stack_size );

	/* pull off ready list.
	 */
	if ( thread_ready == xp ) {
	    thread_ready = xp->next;	
	} else {
	    for ( tp = thread_ready; tp; tp = tp->next ) {
	    	if ( tp->next == xp )
		    tp->next = xp->next;
	    }
	}

	/* put onto free list.
	 */
	xp->next = thread_avail;
	thread_avail = xp;
}

/* Called from interrupt level when
 * current thread does something evil like
 * addressing invalid memory or trying to
 * execute an invalid instruction.
 */
void
thr_fault ( int why )
{
	struct thread *tp;

	cur_thread->state = FAULT;
	cur_thread->fault = why;

	/* XXX this isn't quite right.
	 * we really want to call thr_block,
	 * but that routine is not yet ready
	 * to be called from interrupt level
	 */

	/* Find some other thread to run */
	for ( tp=thread_ready; tp; tp = tp->next ) {
	    if ( tp->state != READY ) {
	    	in_newtp = tp;
		return;
	    }
	}

	/* XXX */
	spin ();
	/*
	kyu_startup ();
	*/

	/* NOTREACHED */

	panic ( "thr_fault - resched" );
}

void
thr_exit ( void )
{
	struct thread *tp;

	if ( thread_debug ) {
	    printf ( "exit of %s\n", cur_thread->name );
	    printf ( "exit, prof = %d\n", cur_thread->prof );
	}

	cur_thread->state = DEAD;

#ifdef notdef
	/* release stack memory.
	 * This used to be OK for plain old exits,
	 * but things got really ugly when zombie
	 * threads were letting go of their stacks
	 * early.  Now I do this in cleanup_thread().
	 *
	 * XXX - I think I could still achieve the
	 * idea of zombie threads releasing their
	 * stacks * early if I waited till ....
	 * somewhere in resched() ??
	 */
	thr_free ( cur_thread->stack, cur_thread->stack_size );
#endif

	/* If we expect to be joined,
	 * either we got here first (in which case, go zombie);
	 * or they are already waiting for us (so unblock them!)
	 */
	if ( cur_thread->join ) {
	    thr_unblock ( cur_thread->join );
	    cleanup_thread ( cur_thread );
	} else if ( cur_thread->flags & TF_JOIN ) {
	    cur_thread->state = ZOMBIE;
	} else {
	    cleanup_thread ( cur_thread );
	}

	/* 
	 * Now things get a bit tricky.
	 * cur_thread points to an invalid thread.
	 * (marking it DEAD avoids having the
	 *  scheduler decide it can return to us).
	 */

	resched ( 0 );
	/* NOTREACHED */

	panic ( "thr_exit - resched" );
}

void
thr_join ( struct thread *tp )
{
	/* This could happen if the thread was
	 * specified without TF_JOIN, but they
	 * did a join anyway (not guaranteed to
	 * work you know).
	 */
	if ( tp->state == DEAD )
	    return;

	if ( tp->state == ZOMBIE ) {
	    tp->state = DEAD;
	    cleanup_thread ( tp );
	    return;
	}

	tp->join = cur_thread;
	thr_block ( JOIN );
}

void
thr_kill ( struct thread *tp )
{
	if ( tp == cur_thread )
	    thr_exit ();
	    /* NOTREACHED */

	setup_c ( tp, (tfptr) thr_exit, 0 );
	tp->state = READY;
}

/* block ourself
 * the why is mostly for debugging.
 */
void
thr_block ( enum thread_state why )
{
	cpu_enter ();
	if ( why == READY ) {	/* XXX paranoia !? */
	    cpu_leave ();
	    return;
	}

	if ( thread_debug )
	    printf ( "thr_block(%d) for %s\n", why, cur_thread->name );

	cur_thread->state = why;

	/* here we *must* switch
	 * to another thread
	 * (or go idle).
	 */
	resched ( 0 );

	/* actually we can return here if the
	 * current thread blocks, then unblocks
	 * with no intervening thread.
	 */
}

/* block ourself with a continuation.
 */
void
thr_block_c ( enum thread_state why, tfptr func, void *arg )
{
	if ( why == READY )	/* XXX paranoia !? */
	    return;

	cpu_enter ();
	cur_thread->state = why;

	setup_c ( cur_thread, func, arg );

	/* here we *must* switch
	 * to another thread
	 * (or go idle).
	 * Since we have just saved our state,
	 * we tell the scheduler about it.
	 */
	resched ( RSF_CONT );

	/* NEW 7/21/2002
	 * The current thread blocked and resumed
	 * with no intervening threads, so we
	 * do not want to simply return.
	 */
#ifdef ARCH_X86
	resume_c ( cur_thread->regs.esp );
#endif
#ifdef ARCH_ARM
	resume_c ( &cur_thread->cregs );
#endif

	/* NOTREACHED */

	/*
	panic ( "thr_block_c -- resched" );
	*/
}

/* block ourself  --  quick!
 *  Just reuse a previous continuation
 */
void
thr_block_q ( enum thread_state why )
{
	if ( why == READY )	/* XXX paranoia !? */
	    return;

	cpu_enter ();
	cur_thread->state = why;

	/* XXX - heck, this might just work
	 * in any case, in particular, I think
	 * that JMP would be just fine.
	 * for now, the panic at least lets us
	 * take a close look at things first.
	 */
	if ( cur_thread->mode != CONT )
	    panic ( "thr_block_q" );

	resched ( RSF_CONT );

	/* NEW 7/21/2002
	 * The current thread blocked and resumed
	 * with no intervening threads, so we
	 * do not want to simply return.
	 */
#ifdef ARCH_X86
	resume_c ( cur_thread->regs.esp );
#endif
#ifdef ARCH_ARM
	resume_c ( &cur_thread->cregs );
#endif
	/* NOTREACHED */

	/*
	panic ( "thr_block_q -- resched" );
	*/
}

/* Special flavor of thread blocking
 * to handle condition variable blocking.
 * We have an extra argument: a mutex lock to
 * be atomically released.
 * XXX -
 * should we add thr_block_m_c() and thr_block_m_q() ?
 * at this point, I think not, easy to do if we want to.
 */
void
thr_block_m ( enum thread_state why, struct sem *mutex )
{
	cpu_enter ();

	sem_unblock ( mutex );
	thr_block ( why );
}


/* unblock somebody.
 * OK (and common) from interrupt code.
 */
void
thr_unblock ( struct thread *tp )
{
	if ( thread_debug ) {
	    if ( in_interrupt )
		printf ( "thr_unblock: %s (INT)\n", tp->name );
	    else
		printf ( "thr_unblock: %s\n", tp->name );
	}

	if ( tp->state == READY )
	    panic ( "unblock, already ready" );

	/* We may be marking the current thread
	 * ready, (if so, it is looping watching
	 * for this), so the best thing to do,
	 * is nothing at all!  To do a change_thread
	 * transfer to ourself would just make
	 * for a bunch of odd nested contexts.
	 */
	tp->state = READY;

	/* Here is a policy issue,
	 * we have just marked someone else
	 * runnable, but we could keep running.
	 *
	 * And it is fairly common to mark the current
	 * thread runnable (all threads may have
	 * been blocked waiting for an event).
	 *
	 * We must switch if the thread we
	 * just marked is of more urgent
	 * priority, but if it isn't, we just
	 * keep running the current thread,
	 * as that economizes on switch overhead.
	 *
	 * If we are doing this inside an interrupt,
	 * we defer running the scheduler till we
	 * are ready to return from the interrupt.
	 *
	 * An issue is what if we are activating
	 * someone at the same priority?  They
	 * could get starved if the current thread
	 * computes forever.  One answer would be
	 * round-robin scheduling, but I say this
	 * situation must be resolved by a proper
	 * choice of (non-equivalent) priorities.
	 */

	/* If we are in an interrupt,
	 * the current thread may not even be ready,
	 * so it might be silly to compare our priority
	 * to it.  8/22/2002
	 */
	if ( in_interrupt ) {
	    if ( cur_thread->state != READY ||
		    (tp->pri < cur_thread->pri) ) {
		if ( ! in_newtp || tp->pri < in_newtp->pri )
		    in_newtp = tp;
	    }
	} else {
	    if ( tp->pri < cur_thread->pri )
		change_thread ( tp, 0 );
	}

#ifdef OLDWAY
	if ( tp->pri < cur_thread->pri ) {
	    if ( in_interrupt ) {
		if ( ! in_newtp || tp->pri < in_newtp->pri )
		    in_newtp = tp;
	    } else {
		change_thread ( tp, 0 );
	    }
	}
#endif
}

/* put a thread on a waiting list till
 * a certain number of clock ticks elapses.
 */
void
thr_delay ( int nticks )
{
	if ( thread_debug )
	    printf ( "thr_delay(%d) for %s\n", nticks, cur_thread->name );

	if ( nticks > 0 ) {
	    cpu_enter ();
	    timer_add_wait ( cur_thread, nticks );
	    thr_block ( DELAY );
	}
}

/* as above, but resume with an continuation
 */
void
thr_delay_c ( int nticks, tfptr func, void *arg )
{
	if ( thread_debug )
	    printf ( "thr_delay_c(%d) for %s\n", nticks, cur_thread->name );

	if ( nticks > 0 ) {
	    cpu_enter ();
	    timer_add_wait ( cur_thread, nticks );
	    thr_block_c ( DELAY, func, arg );
	}
}

/* as above, but resume with quick continuation
 * (perfect for tail recursion)
 */
void
thr_delay_q ( int nticks )
{
	if ( thread_debug )
	    printf ( "thr_delay_q(%d) for %s\n", nticks, cur_thread->name );

	if ( nticks > 0 ) {
	    cpu_enter ();
	    timer_add_wait ( cur_thread, nticks );
	    thr_block_q ( DELAY );
	}
}

/* PUBLIC - offer to yield the processor.
 * (never an internal call, but rather a
 *  politeness call, we prefer to let another
 *  thread run, even one of the same priority.)
 * This call will cause the highest READY thread
 *  other than the current one to run, i.e. this
 *  will cause a violation of the usual principle
 *  that the highest priority ready thread is
 *  always the one running.  So this is evil.
 * I make is an empty function so some old tests
 *  won't need to be recoded.  It was a kind of crutch
 *  during early testing, but caused trouble later.
 */

#ifdef REAL_YIELD
void
thr_yield ( void )
{
#ifndef SORT_PRI
	resched ( RSF_YIELD );
#endif
}
#else
void
thr_yield ( void )
{
}
#endif

/* Called from interrupt level when it is
 * clear that a different thread should be resumed.
 * (instead of the one interrupted).
 */
void
change_thread_int ( struct thread *new_tp )
{
	change_thread ( new_tp, RSF_INTER );
}

/* We have decided to resume some other
 * thread (than the one we are now running).
 * We either know because we are doing a
 * handoff to a more urgent thread, or we
 * have made a decision via the scheduling
 * loop.
 * XXX - we may want to do something clever when
 * a single thread wakes up and changes to itself.
 */
static void
change_thread ( struct thread *new_tp, int options )
{
	/*
	printf ( "change_thread, %08x, %d\n", new_tp, options );
	*/

	if ( thread_debug ) {
	    char *mode; 

	    switch ( new_tp->mode ) {
		case JMP:
		    mode = "J";
		    break;
		case INT:
		    mode = "I";
		    break;
		case CONT:
		    mode = "C";
		    break;
		default:
		    mode = "?";
		    break;
	    }

	    if ( in_interrupt )
		printf ( "change_thread: %s -> %s (via %s) %04x (INT)\n",
		    cur_thread->name, new_tp->name, mode, options );
	    else
		printf ( "change_thread: %s -> %s (via %s) %04x\n",
		    cur_thread->name, new_tp->name, mode, options );
	}

	/* ---------------------------------------------
	 * OK, the debug is over, now check for panics
	 */

	if ( ! new_tp )		/* XXX - paranoia */
	    panic ( "change_thread, null" );

	/* This is a worthy panic,
	 * it revealed our first race condition.
	 */
	if ( new_tp == cur_thread )
	    panic ( "change_thread, already running current" );

	/* ---------------------------------------------
	 * Now we are in the proper mode to launch
	 * off into something new, but wait!
	 * have we saved our state?
	 * If we are doing a continuation sort of
	 * thing, we have that all set up, and if
	 * this is happening out of interrupt level
	 * we are good to go, the only case is
	 * the regular synchronous transfer.
	 */

	/* Special case for some threads, if they
	 * are *always* launched via a continuation,
	 * need not save any state.
	 *
	 * also if we are switching away from a thread
	 * that has already saved a continuation,
	 * we don't need to save anything ???
	 *
	 * Notice that the thread will come to life again
	 * right here and return.
	 *
	 * This next bit of code is ugly, there must be
	 * a better way, at least to express it.
	 *
	 * The idea, is to express which cases do not
	 * require us to save our current state.
	 *
	 * 1) if this is within an interrupt
	 *	(we have saved state on interrupt entry)
	 * 2) if some routine has already saved a
	 *	continuation to resume with,
	 *	(this is how block_q and block_c work.)
	 */

	if ( ! (options & RSF_INTER) &&
		(options & RSF_CONT) == 0 ) {
		    cur_thread->mode = JMP;
		    if ( save_j ( &cur_thread->regs ) ) {
			if ( thread_debug )
			    printf ( "Change_thread: returns\n" );
			return;
		    }
	}

	/* If the current thread is running at interrupt level,
	 * we need to mark the context we are leaving
	 * so it is resumed using the interrupt resumer.
	 */
	if ( options & RSF_INTER )
	    cur_thread->mode = INT;

	/* Cancel any idea of a thread that yielded
	 * to us.
	 */
	cur_thread->yield = (struct thread *) 0;

	/*
	 * Remember, tp->mode tells how the thread should be resumed,
	 *  not how it should be saved.
	 */

	cur_thread = new_tp;

	/* We need to stay locked between deciding how to resume
	 * and actually doing so.  5-14-2015
	 * Otherwise a bad race results.
	 * Note that each resume reenables interrupts.
	 */
	cpu_enter ();
	switch ( cur_thread->mode ) {
	    case JMP:
		if ( thread_debug )
		    printf ("change_thread_resume_j\n");
		resume_j ( &new_tp->regs );
		panic ( "change_thread, switch_j" );
		break;
	    case INT:
		if ( thread_debug )
		    printf ("change_thread_resume_i\n");
		resume_i ( &new_tp->iregs );
		panic ( "change_thread, switch_i" );
		break;
	    case CONT:
		/*
		printf ("change_thread_resume_c: %08x\n", new_tp->regs.esp);
		dump_l ( (void *)new_tp->regs.esp, 1 );
		*/
		if ( thread_debug )
		    printf ("change_thread_resume_c: %08x\n", &new_tp->cregs );
#ifdef ARCH_X86
		resume_c ( new_tp->regs.esp );
#endif
#ifdef ARCH_ARM
		resume_c ( &new_tp->cregs );
#endif
		panic ( "change_thread, switch_c" );
		break;
	    default:
		panic ( "change_thread, unknown mode: %d for thread %s\n", cur_thread->mode, cur_thread->name );
		break;
	}
	/* NOTREACHED */
}

/*
 * call this to do a reschedule.
 *
 * This is called in these cases:
 *	1) the current thread has terminated,
 *	   so we need to find another thread to run.
 *	2) the current thread has blocked,
 *	   so we need to find another thread to run.
 *	3) the current thread is being nice
 *	   and calling yield.
 *	4) we are resuming after being blocked
 *	   when no thread was runnable.
 *
 * The argument supplies scheduling hints
 * (such as, "try not to keep running current thread")
 *
 */

static void
resched ( int options )
{
	struct thread *tp;
	struct thread *best_tp = (struct thread *) 0;
	int best_pri = MAX_PRI;
	static int debug_count = 0;

	if ( thread_debug ) {
#ifdef notdef
	    if ( cur_thread == thread0 ) {
	    	printf ( "*" );
		if ( (++debug_count % 25 ) == 0 )
		    printf ( "\n" );
	    } else {
	    }
#endif
	    printf ( "resched: current thread: %s ", cur_thread->name );
	    thr_show_state ( cur_thread );
	    printf ( " (%04x)\n", options );
	}

	/* We have been tempted to allow this
	 * (a null cur_thread pointer), but the problem
	 * is that interrupt routines expect this
	 * pointer (and they certainly need a valid
	 * stack).
	 */
	if ( ! cur_thread )
	    panic ( "resched, invalid current thread" );

	cpu_enter ();		/* XXX XXX */

#ifdef SORT_PRI
	/* Now we keep the ready list in priority order,
	 * so we can just run the first ready thing
	 * we find (and we never examine the current
	 * thread)
	 */
	if ( cur_thread->yield )
	    best_tp = tp;
	else
	    best_tp = cur_thread->next;

	while ( best_tp ) {
	    if ( best_tp->state == READY )
	    	break;
	    best_tp = best_tp->next;
	}

#else
	/* Try to find some other thread we can run.
	 */
	for ( tp=thread_ready; tp; tp = tp->next ) {
	    if ( tp->state != READY )
	    	continue;
	    if ( (options & RSF_YIELD) && tp == cur_thread )
		continue;
	    if ( tp->pri > best_pri )
	    	continue;
	    best_pri = tp->pri;
	    best_tp = tp;
	}

#endif

	if ( thread_debug ) {
	    if ( best_tp ) {
		printf ( "resched chose: %s ", best_tp->name );
		thr_show_state ( best_tp );
		printf ( " %d\n", best_tp->pri );
	    } else
		printf ( "resched came up empty\n" );
	}

	/* Notice that we cannot have found the
	 * current thread at this point.
	 * If we are blocked or exiting, we are
	 * not ready, and if we are doing a
	 * yield that should have been taken
	 * care of.
	 */
	if ( best_tp == cur_thread )
	    panic ( "resched, current" );

	if ( best_tp ) {
	    if ( options & RSF_YIELD)
	    	best_tp->yield = cur_thread;
	    change_thread ( best_tp, options );
	}

	cpu_leave ();		/* XXX XXX */

	/* We could find no other thread to
	 * switch to, if the current thread is
	 * ready, we must be doing a yield, so
	 * we can just go back and keep running
	 * the current thread.
	 */
	if ( cur_thread->state == READY )
	    return;

	/* OK, nobody is runnable, not even
	 * ourselves, so we are forced to
	 * switch to idle state.
	 * ... whatever it might be.
	 */

	if ( thread_debug ) {
	    printf ( "Idle in %s\n", cur_thread->name );
	}

	/* IDLE - Just spin right here
	 * in idle state.  (Compiler does
	 * aggressive optimization without
	 * some volatile declaration).
	 *
	 * What happens if we have several blocked threads?
	 * In particular, what if when some event (interrupt) happens?
	 * If the interrupt unblocks the current thread,
	 *  then this loop pops loose and that is the easy case.
	 * What if some other thread gets unblocked?
	 * This gets handled in thr_unblock() where it marks
	 * the thread to change to when the interrupt returns.
	 */
	{
	    volatile enum thread_state *state;

	    state = &cur_thread->state;

	    while ( *state != READY )
		;
	}

	if ( thread_debug )
	    printf ( "Awake in %s\n", cur_thread->name );

	return;
}

/*-------------------------------------
 * semaphore facility
 * XXX - Need to get rid of the following static allocation scheme.
 */

static struct sem sem_pool[MAX_SEM];
static struct sem *sem_avail;

static struct cv cv_pool[MAX_CV];
static struct cv *cv_avail;

void
sem_init ( void )
{
	struct sem *sp;
	struct cv *cp;
	int i;

	sem_avail = (struct sem *) 0;
	for ( i=0; i<MAX_SEM; i++ ) {
	    sp = &sem_pool[i];
	    sp->next = sem_avail;
	    sem_avail = sp;
	}

	cv_avail = (struct cv *) 0;
	for ( i=0; i<MAX_CV; i++ ) {
	    cp = &cv_pool[i];
	    cp->next = cv_avail;
	    cv_avail = cp;
	}
}

/* initialize a new semaphore.
 * state = 1	set, (full).
 * state = 0	clear, (empty).
 *
 * Note that if you think of a mutex, the names are
 * non-intuitive, i.e. the semaphore is full(set) when
 * nobody is in the critical section.  Maybe this is
 * intuitive if you think about it in the right way.
 * The semaphore hold the "token" and you need to take
 * it from the semaphore to enter the section.
 *
 * XXX - ignore flags for now.
 */

struct sem *
sem_new ( enum sem_state state, int flags )
{
	struct sem *sp;

	if ( ! sem_avail ) {
	    /* XXX - bad bugs come when we
	     * don't check return values.
	     */
	    panic ( "semaphores all gone" );
	    return (struct sem *) 0;
	}

	/* get us a semaphore node.
	 */
	sp = sem_avail;
	sem_avail = sp->next;

	sp->state = state;
	sp->flags = flags;

	sp->wait = (struct thread *) 0;
	return sp;
}

struct sem *
sem_mutex_new ( int flags )
{
	return sem_new ( SET, flags );
}

struct sem *
sem_signal_new ( int flags )
{
	return sem_new ( CLEAR, flags );
}

void
sem_destroy ( struct sem *sp )
{
	sp->next = sem_avail;
	sem_avail = sp;
}

/* OK from interrupt code.
 */
void
sem_unblock ( struct sem *sem )
{
	struct thread *tp;

	if ( sem->state == SET ) {
	    return;
	} else {	/* CLEAR */
	    if ( sem->wait ) {
		/* XXX - race */
		tp = sem->wait;
		sem->wait = tp->wnext;
	    	thr_unblock ( tp );
		return;
	    }
	    sem->state = SET;
	}
}

int
sem_block_try ( struct sem *sem )
{
	cpu_enter ();	/* XXX */
	if ( sem->state == CLEAR ) {
	    cpu_leave ();
	    return 0;
	} else {
	    sem_block ( sem );
	    return 1;
	}
}

/* Used by the 3 flavors of sem_block below
 */
static void
sem_add ( struct sem *sem )
{
	struct thread *tp;
	struct thread *p, *lp;

	if ( sem->flags & SEM_PRIO ) {
	    /* SEM_PRIO, sort into list
	     */
	    p = sem->wait;
	    while ( p && p->pri <= cur_thread->pri ) {
		lp = p;
		p = p->wnext;
	    }

	    cur_thread->wnext = p;

	    if ( p == sem->wait )
		sem->wait = cur_thread;
	    else
		lp->wnext = cur_thread;

	} else {
	    /* SEM_FIFO, put at end of list
	     */
	    cur_thread->wnext = (struct thread *) 0;
	    if ( sem->wait ) {
		tp = sem->wait;
		while ( tp->wnext )
		    tp = tp->wnext;
		tp->wnext = cur_thread;
	    } else {
		sem->wait = cur_thread;
	    }
	}
}

/* XXX - should never be called from interrupt code,
 * perhaps should be a panic if we do ??
 */
void
sem_block ( struct sem *sem )
{
	cpu_enter ();	/* XXX */
	if ( sem->state == SET ) {
	    sem->state = CLEAR;
	    cpu_leave ();
	    return;
	}

	/* CLEAR */
	sem_add ( sem );

	thr_block ( SWAIT );
	/* usually does not return here,
	 * but can if this is the only thread.
	 */
}

void
sem_block_c ( struct sem *sem, tfptr func, void *arg )
{
	cpu_enter ();	/* XXX */
	if ( sem->state == SET ) {
	    sem->state = CLEAR;
	    cpu_leave ();
	    return;
	}

	/* CLEAR */
	sem_add ( sem );
	thr_block_c ( SWAIT, func, arg );
}

void
sem_block_q ( struct sem *sem )
{
	cpu_enter ();	/* XXX */
	if ( sem->state == SET ) {
	    sem->state = CLEAR;
	    cpu_leave ();
	    return;
	}

	/* CLEAR */
	sem_add ( sem );
	thr_block_q ( SWAIT );
}

#ifdef notyet
static
sem_timeout_func ( void )
{
}

/* NEW - block on semaphore with timeout.
 *	(timeout is number of ticks)
 *	returns: 0 when normal return.
 *	returns: 1 when timed out.
 */
int
sem_block_t ( struct sem *sem, int timeout );
{
	sem_block ( sem );
	return 0;
}
#endif

/* Condition Variable style blocking
 */
void
sem_block_m ( struct sem *sem, struct sem *mutex )
{
	cpu_enter ();
	sem_unblock ( mutex );
	sem_block ( sem );
}

/* Condition variables ...
 * I bind together the mutex and the cv when the
 * cv is created.  This may seem contrary to the
 * advice that a mutex may have several predicates,
 * each with its own condition variable, but this
 * is not the intent, but rather this is just some
 * "tidiness" so the wait call need not specify the
 * mutex.  The cv just "knows" what mutex to fool
 * with when I do it this way.
 *
 * One mutex may be used to lock a resource with
 * several predicates, and each predicate should
 * have its own condition variable.
 */

struct cv *
cv_new ( struct sem *mutex )
{
	struct cv *cp;
	struct sem *sp;

	/* allocate a signaling semaphore
	 * (empty so the first wait will block).
	 */
	sp = sem_new ( CLEAR, SEM_FIFO );
	if ( ! sp ) {
	    return (struct cv *) 0;
	}

	if ( ! cv_avail ) {
	    /* XXX - bad bugs come when we
	     * don't check return values.
	     */
	    panic ( "condition variables all gone" );
	    return (struct cv *) 0;
	}

	cp = cv_avail;
	cv_avail = cp->next;

	cp->signal = sp;
	cp->mutex = mutex;

	return cp;
}

void
cv_destroy ( struct cv *cp )
{
	sem_destroy ( cp->signal );
	cp->next = cv_avail;
	cv_avail = cp;
}

void
cv_signal ( struct cv *cp )
{
	sem_unblock ( cp->signal );
}

void
cv_wait ( struct cv *cp )
{
	cpu_enter ();
	sem_unblock ( cp->mutex );
	sem_block ( cp->signal );
	/* XXX
	 * seems like a race is possible right here,
	 * unless we come out of sem_block() with interrupts masked
	 */
	sem_block ( cp->mutex );
}

#ifdef notdef
/* defined in sklib.h */
extern inline void cpu_enter ( void )
{
    __asm__ __volatile__ ( "cli" );
}

extern inline void cpu_leave ( void )
{
    __asm__ __volatile__ ( "sti" );
}
#endif

/* "safe" versions
 * (hack to make printf work from interrupt
 *  code when we are using a serial console)
 */

#ifdef ARCH_X86
void cpu_enter_s ( void )
{
    if ( ! in_interrupt ) {
	__asm__ __volatile__ ( "cli" );
    }
}

void cpu_leave_s ( void )
{
    if ( ! in_interrupt ) {
	__asm__ __volatile__ ( "sti" );
    }
}
#endif

/* Just some "sugar" to make the cpu_xxx() calls all nice
 * and analogous to the cv_xxx() calls.
 */
struct sem *
cpu_new ( void )
{
	sem_new ( CLEAR, SEM_FIFO );
}

void
cpu_signal ( struct sem *xp )
{
	sem_unblock ( xp );
}

/* This works, but still has race conditions
 * this gets called with interrupts masked off.
 * we want to block and release interrupts in one
 * swell foop, and likewise, we want to become
 * unblocked with atomic blocking of interrupts.
 */
void
cpu_wait ( struct sem *xp )
{
	cpu_leave ();
	sem_block ( xp );
	cpu_enter ();
}

/* THE END */
