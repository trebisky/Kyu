/* tcbref.c - tcbref, tcbunref */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  tcbref  -  increment the reference count of a TCB
 *------------------------------------------------------------------------
 */
void	tcbref(
	  struct tcb	*ptcb		/* Ptr to a TCB			*/
       )
{
	ptcb->tcb_ref++;
	return;
}

/*------------------------------------------------------------------------
 *  tcbunref  -  decrement the reference count of a TCB and free the
 *			TCB if the reference count reaches zero
 *------------------------------------------------------------------------
 */

/* Kyu note:
 * I spent a lot of time debuggin an issue that led to my adding the simple
 * test below.   Xinu TCP does, as far as I know call this "once too many",
 * i.e. once with ref=1, which it decrements to zero (and frees memory),
 * and then again (for some reason) with ref=0, which caused the bug I
 * spent two days tracking down.  The original Xinu code works because
 * Xinu freemem is idempotent.  It detects a block that overlaps an existing
 * one and rejects it (returning SYSERR), and that return status is
 * ignored here.
 * The semantics of malloc/free certainly don't allow such things.
 * Neither does my dedicated getmem/freemem that I use solely for
 * Xinu TCP.   This makes me feel better since it more or less reassures
 * me that there is not some ugly race going on.
 * This is all about my being unaware of the precise semantics of
 *  Xinu getmem/freemem.
 *
 * Note that I rename my getmem/freemem (with different semantics,
 *  and specially tailored to use with Xinu TCP)
 *  to kyu_getmem, kyu_freemem.
 */
void	tcbunref ( struct tcb	*ptcb )
{
	/* Kyu - tjt - Avoid calling freemem twice */
	if ( ptcb->tcb_ref <= 0 )
	    return;

	if (--ptcb->tcb_ref <= 0) {
		kyu_freemem ((char *)ptcb->tcb_rbuf, ptcb->tcb_rbsize);
		kyu_freemem ((char *)ptcb->tcb_sbuf, ptcb->tcb_sbsize);
		ptcb->tcb_state = TCB_FREE;
	}
}

#ifdef notdef
#define get_FP(x)       asm volatile ("add %0, fp, #0\n" :"=r" ( x ) )

void	tcbunref_ ORIG (
	  struct tcb	*ptcb		/* Ptr to a TCB			*/
       )
{

	printf ( "TCBUNREF  %08x %d\n", ptcb, ptcb->tcb_ref );
	{
	unsigned int fp;
        get_FP ( fp );
        unroll_fp ( fp );
	}

	if (--ptcb->tcb_ref <= 0) {
		printf ( "TCBUNREF! %08x %d\n", ptcb, ptcb->tcb_ref );
		freemem ((char *)ptcb->tcb_rbuf, ptcb->tcb_rbsize);
		freemem ((char *)ptcb->tcb_sbuf, ptcb->tcb_sbsize);
		ptcb->tcb_state = TCB_FREE;
	}
	return;
}
#endif
