/* tcpclosing.c  -  tcpclosing */

#include <xinu.h>

extern int finwait_ms;	/* Kyu */

/*------------------------------------------------------------------------
 *  tcpclosing  -  Schedule expiration on a TCB that is closing
 *------------------------------------------------------------------------
 */
int32	tcpclosing(
	  struct tcb	*tcbptr,		/* Ptr to a TCB			*/
	  struct netpacket *pkt		/* Ptr to packet		*/
	)
{
	/* If FIN is within range, schedule TCB expiration */

	if (SEQ_CMP (tcbptr->tcb_suna, tcbptr->tcb_sfin) > 0) {
		// printf ( "TWAIT from closing\n" );
		// tcptmset (TCP_MSL << 1, tcbptr, TCBC_EXPIRE);
		tcptmset (finwait_ms, tcbptr, TCBC_EXPIRE);
		tcbptr->tcb_state = TCB_TWAIT;
	}
	return OK;
}
