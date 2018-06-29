/* tcpsynrcvd.c  -  tcpsynrcvd */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  tcpsynrcvd  -  Handle an incoming segment in state SYN-RECEIVED
 *------------------------------------------------------------------------
 */
int32	tcpsynrcvd(
	  struct tcb	*tcbptr,	/* Ptr to a TCB			*/
	  struct netpacket *pkt		/* Ptr to a packet		*/
	)
{
	/* Check for ACK or bas sequence number */

	if (!(pkt->net_tcpcode & TCPF_ACK)
	    || pkt->net_tcpseq != tcbptr->tcb_rnext) {
	    	//kprintf("tcpsynrcvd: no ack or bad seq\n");
		return SYSERR;
	}

	/* The following increment of the refernce count is for	*/
	/*    the system because the TCB is now synchronized	*/

	tcbref (tcbptr);

	/* Change state to ESTABLISHED */

	tcbptr->tcb_state = TCB_ESTD;

	/* Move past SYN */

	tcbptr->tcb_suna++;
	if (tcpdata (tcbptr, pkt)) {
		tcpxmit (tcbptr, tcbptr->tcb_snext);
	}

	/* Permit reading */

	// KYU printf ( "TCP ESTD - synrcvd\n" );
	// KYU printf ( "TCP ESTD - signal readers\n" );

	/* XXX - KYU callback addition */
	/* We must unlock the mutex or we deadlock.
	 * This is from tcp_in() via tcpdisp()
	 * maybe there are better ways to do this?
	 */
	if ( tcbptr->tcb_lfunc ) {
	    signal ( tcbptr->tcb_mutex );
	    (*tcbptr->tcb_lfunc) (tcbptr->tcb_slot);
	    wait ( tcbptr->tcb_mutex );
	    return OK;
	}

	if (tcbptr->tcb_readers > 0) {
		tcbptr->tcb_readers--;
		// KYU printf ( "TCP ESTD - signaling readers\n" );
		signal (tcbptr->tcb_rblock);
	}

	return OK;
}
