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
void	tcbunref ( struct tcb	*ptcb )
{
	/* Kyu - tjt - Avoid calling freemem twice */
	if ( ptcb->tcb_ref <= 0 )
	    return;

	if (--ptcb->tcb_ref <= 0) {
		freemem ((char *)ptcb->tcb_rbuf, ptcb->tcb_rbsize);
		freemem ((char *)ptcb->tcb_sbuf, ptcb->tcb_sbsize);
		ptcb->tcb_state = TCB_FREE;
	}
}

#ifdef notdef
#define get_FP(x)       asm volatile ("add %0, fp, #0\n" :"=r" ( x ) )

void	tcbunref(
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
