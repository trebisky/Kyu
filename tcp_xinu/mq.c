/* mq.c  -  mqcreate, mqdelete, mqsend, mqrecv, mqpoll, mqdisable,	*/
/*			mqenable, mqclear				*/

#include <xinu.h>

struct	mqentry	mqtab[NMQ];		/* Table of message queues	*/
sid32	mqsem;				/* Mutex semaphore		*/
int32	mqready = 0;

/* Semaphore semantics:							*/
/* 	mqcreate and mqdelete hold the mutex to avoid disabling		*/
/*	interrupts during the iteration.  However, interrupts must 	*/
/*	be disabled when an item's MQ_FREE/MQ_ALLOC status is changed.	*/
/*	Therefore, disabling interrupts is used to send or receive	*/
/*	a message.  The point is: mqsem must be held to add or remove	*/
/*	a queue, and interrupts must be disabled for the actual		*/
/*	change in status;  during send or receive, interrupts must be	*/
/*	disabled for all modifications.					*/

/*------------------------------------------------------------------------
 *  mqinit  -  Initialize the message queues that will be used by TCP
 *------------------------------------------------------------------------
 */
void	mqinit(void)
{
	int32	i;			/* index into queue table	*/

	/* Set the state to free and initialize the cookie */

	for (i=0; i<NMQ; i++) {
		mqtab[i].mq_state = MQ_FREE;
		mqtab[i].mq_cookie = 0;
	}

	/* Create the mutex */

	mqsem = semcreate(1);
	mqready = 1;
}

/* Kyu 6-21-2018
 * mqtab is currently static with 200 entries
 * Ntcp is now set to 100
 * mqcreate gets called 100 times to get 15 entries,
 *  then once to get 10 entries.
 * This is primarily to get around some weird bug
 *  in Kyu malloc/free
 */
#define KYU_MQ_STATIC

#ifdef KYU_MQ_STATIC
#define MQ_STATIC_SIZE ((Ntcp+1)*16)

static int32 mq_static[MQ_STATIC_SIZE];
static int mq_next = 0;

static int32 *
mq_alloc ( int32 size )
{
	int32 * rv;

	if ( mq_next + size > MQ_STATIC_SIZE )
	    panic ( "Xinu TCP: MQ static size too small" );

	rv = &mq_static[mq_next];
	mq_next += 16;
	return rv;
}
#endif


/*------------------------------------------------------------------------
 *  mqcreate  -  create a message queue and return its ID
 *------------------------------------------------------------------------
 */
int32	mqcreate (
	  int32		qlen		/* Length of the queue		*/
	)
{
	int32	i;			/* Index into queue table	*/
	// intmask	mask;		/* Saved interrupt mask		*/

	/* Guarantee mutual exclusion */

	wait(mqsem);

	/* Find a free entry in the table */

	for (i = 0; i < NMQ; i++) {
		if (mqtab[i].mq_state != MQ_FREE)
			continue;

		/* Create a semaphore to control access */

		if ((mqtab[i].mq_sem = semcreate (0)) == SYSERR) {
			kprintf("error1 i = %d\n", i);
			signal (mqsem);
			return SYSERR;
		}

#ifdef KYU_MQ_STATIC
		mqtab[i].mq_msgs = mq_alloc ( qlen );
#else
		/* Allocate memory to hold a set of qlen messages */
		/* KYU - note: this allocates a bunch of 60 byte objects.
		 *  As near as I can tell, they never get freed.
		 *  mqdelete provides for it, but is never called.
		 */

		if ((mqtab[i].mq_msgs = (int32 *)getmem (qlen * sizeof(int32)))
		    == (int32 *)SYSERR) {
		    	kprintf("error2\n");
			semdelete(mqtab[i].mq_sem);
			signal (mqsem);
			return SYSERR;
		}
#endif

		/* Initialize remaining entries for the queue structure */


		mqtab[i].mq_qlen = qlen;
		mqtab[i].mq_first = 0;
		mqtab[i].mq_count = 0;
		mqtab[i].mq_rcvrs = 0;
		mqtab[i].mq_cookie++;

		// mask = disable();
		INT_lock;
		mqtab[i].mq_state = MQ_ALLOC;
		// restore(mask);
		INT_unlock;

		signal (mqsem);
		return i;
	}

	/* Table is full */
	kprintf("error3\n");
	signal (mqsem);
	return SYSERR;
}

#ifdef notdef
/*------------------------------------------------------------------------
 *  mqdelete  -  delete a message queue
 *------------------------------------------------------------------------
 */
int32	mqdelete (
	  int32		mq		/* Message queue to use		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	struct	mqentry	*mqp;		/* ptr to queue entry		*/

	/* Obtain a pointer to the specified queue */

	mqp = &mqtab[mq];

	/* Guarantee exclusive access and check status */

	wait (mqsem);
	if (!MQVALID(mq) || mqp->mq_state != MQ_ALLOC) {
		signal (mqsem);
		return SYSERR;
	}

	/* Make the queue free and return the memory */

	// mask = disable();
	INT_lock;
	mqp->mq_state = MQ_FREE;
	// restore(mask);
	INT_unlock;

	semdelete (mqp->mq_sem);
	freemem ((char *)mqp->mq_msgs, mqp->mq_qlen * sizeof(int));
	signal (mqsem);
	return OK;
}
#endif

/*------------------------------------------------------------------------
 *  mqsend  -  deposit a message on a queue
 *------------------------------------------------------------------------
 */
int32	mqsend (
	  int32		mq,		/* Message queue to use		*/
	  int32		msg		/* Message to deposit		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	struct	mqentry	*mqp;		/* ptr to message queue		*/

	/* Obtain ptr to specified queue */

	mqp = &mqtab[mq];

	/* Insure exclusive access */

	// mask = disable();
	INT_lock;
	if (!MQVALID(mq) || mqp->mq_state != MQ_ALLOC
	    || mqp->mq_count == mqp->mq_qlen) {
		// restore(mask);
		INT_unlock;
		return SYSERR;
	}

	/* Add message in next available slot and increase count */

	mqp->mq_msgs[(mqp->mq_first + mqp->mq_count) % mqp->mq_qlen] = msg;
	mqp->mq_count++;

	/* Make message available */

	/* XXX - tjt should we unlock first ?? */
	signal (mqp->mq_sem);

	// restore(mask);
	INT_unlock;
	return OK;
}


/*------------------------------------------------------------------------
 *  mqrecv  -  extract a message from a queue
 *------------------------------------------------------------------------
 */
int32	mqrecv (
	  int32		mq		/* Message queue to use		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	struct	mqentry	*mqp;		/* Ptr to queue			*/
	int32	msg;			/* Extracted message		*/
	int32	cookie;			/* Old value of cookie		*/

	/* Obtain a pointer ot the queue */

	mqp = &mqtab[mq];

	/* Insure exclusive access */

	// mask = disable();
	INT_lock;
	if (!MQVALID(mq) || mqp->mq_state != MQ_ALLOC) {
		// restore(mask);
		INT_unlock;
		return SYSERR;
	}

	/* Record current cookie */

	cookie = mqp->mq_cookie;
	mqp->mq_rcvrs++;
	wait(mqp->mq_sem);
	if (cookie != mqp->mq_cookie) {	  /* queue changed */
		// restore(mask);
		INT_unlock;
		return SYSERR;
	}
	mqp->mq_rcvrs--;

	/* Extract message and move 'first' pointer to next message */

	msg = mqp->mq_msgs[mqp->mq_first];
	mqp->mq_first = (mqp->mq_first + 1) % mqp->mq_qlen;

	/* Decrement count of messages remaining and return msg */

	mqp->mq_count--;
	// restore(mask);
	INT_unlock;
	return msg;
}

/*------------------------------------------------------------------------
 *  mqpoll  -  Extract a message from a queue if one is available
 *------------------------------------------------------------------------
 */
int32	mqpoll (
	  int32		mq		/* Message queue to use		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	int32	msg;			/* Message to return		*/

	/* Insure exclusive access */

	// mask = disable();
	INT_lock;
	if (!MQVALID(mq) || mqtab[mq].mq_state != MQ_ALLOC
	    || mqtab[mq].mq_count == 0) {	/* No message available	*/
		// restore(mask);
		INT_unlock;
		return SYSERR;
	}

	/* Extract a message and return */

	msg = mqrecv (mq);
	// restore(mask);
	INT_unlock;
	return msg;
}

/*------------------------------------------------------------------------
 *  mqdisable  -  Temporarily disable a message queue
 *------------------------------------------------------------------------
 */
int32	mqdisbale (
	  int32		mq		/* Message queue to use		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	struct	mqentry	*mqp;		/* Ptr to the queue		*/

	/* Obtain pointer to the queue */

	mqp = &mqtab[mq];

	/* Insure exclusive access */

	wait (mqsem);

	/* Disable interrupts for state change */

	// mask = disable();
	INT_lock;
	if (!MQVALID(mq) || mqp->mq_state != MQ_ALLOC) {
		// restore(mask);
		INT_unlock;
		signal (mqsem);
		return SYSERR;
	}
	mqp->mq_cookie++;
	mqp->mq_state = MQ_DISABLE;

	/* Reset semaphore in case processes are waiting */
	/* XXX will cause panic in Kyu */

	semreset(mqp->mq_sem, 0);
	mqp->mq_rcvrs = 0;

	// restore(mask);
	INT_unlock;
	signal (mqsem);
	return OK;
}

/*------------------------------------------------------------------------
 *  mqenable  -  Re-enable a message queue that has been disabled
 *------------------------------------------------------------------------
 */
int32	mqenable (
	  int32		mq		/* Message queue to use		*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	int32	retval;			/* Return value			*/

	/* Insure exclusive access */

	wait (mqsem);
	// mask = disable();
	INT_lock;

	/* Re-enable if currently disabled */
	if (MQVALID(mq) && mqtab[mq].mq_state == MQ_DISABLE) {
		mqtab[mq].mq_state = MQ_ALLOC;
		retval = OK;
	} else {
		retval = SYSERR;
	}
	// restore(mask);
	INT_unlock;

	signal (mqsem);
	return retval;
}

/*------------------------------------------------------------------------
 *  mqclear  -  clear all messages from a queue
 *------------------------------------------------------------------------
 */
int32	mqclear (
	  int32		mq,		/* Message queue to use		*/
	  int32		(*func)(int32)	/* Message disposal function	*/
	)
{
	// intmask	mask;		/* Saved interrupt mask		*/
	struct	mqentry	*mqp;		/* Ptr to the queue		*/
	int32	i;			/* walks through message list	*/

	/* Obtain a pointer to the queue */

	mqp = &mqtab[mq];

	/* Obtain exclusive access and insure queue is disabled */

	wait (mqsem);
	// mask = disable();
	INT_lock;
	if (!MQVALID(mq) || mqp->mq_state != MQ_DISABLE) {
		// restore(mask);
		INT_unlock;
		signal (mqsem);
		return SYSERR;

	}

	/* Set the state to "clearing" during iteration */

	mqp->mq_state = MQ_CLEAR;
	// restore(mask);
	INT_unlock;
	signal (mqsem);

	/* Iterate through all messages and use func to dispose of each	*/

	for (i = 0; i < mqp->mq_count; i++) {
		(*func)(mqp->mq_msgs[(mqp->mq_first + i) % mqp->mq_qlen]);
	}
	mqp->mq_count = 0;

	/* Reset the state to disabled*/

	// mask = disable();
	INT_lock;
	mqp->mq_state = MQ_DISABLE;
	// restore(mask);
	INT_unlock;

	return OK;
}

/* THE END */
