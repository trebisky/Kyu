/* tcpfin2.c  -  tcpfin2 */

#include <xinu.h>

extern int finwait_ms;	/* Kyu */

/*------------------------------------------------------------------------
 *  tcpfin2  -  Handle an incoming segment in the FIN-WAIT2 state
 *------------------------------------------------------------------------
 */
int32	tcpfin2(
	  struct tcb	*tcbptr,	/* Ptr to a TCB			*/
	  struct netpacket *pkt		/* Ptr to a packet		*/
	)
{
	tcpdata (tcbptr, pkt);

	/* If our FIN has been ACKed, move to TIME-WAIT	*/
	/*            and schedule the TCB to expire	*/

	if (tcbptr->tcb_flags & TCBF_FINSEEN
	    && SEQ_CMP(tcbptr->tcb_rnext, tcbptr->tcb_rfin) > 0) {
		// printf ( "Twait from fin2\n" );
		// tcptmset (TCP_MSL << 1, tcbptr, TCBC_EXPIRE);
		tcptmset (finwait_ms, tcbptr, TCBC_EXPIRE);
		tcbptr->tcb_state = TCB_TWAIT;
	}

	tcpxmit (tcbptr, tcbptr->tcb_snext);

	return OK;
}
