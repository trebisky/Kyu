/* tcplisten.c  -  tcplisten */

#include <xinu.h>

// Prior to 6-27-2018 was "wired in" as 65535
extern int tcp_bufsize;

/*------------------------------------------------------------------------
 *  tcplisten  -  handle a segment in the LISTEN state
 *------------------------------------------------------------------------
 */
int32	tcplisten(
	  struct tcb	*tcbptr,	/* Ptr to a TCB			*/
	  struct netpacket *pkt		/* Ptr to a packet		*/
	)
{
	struct	tcb	*pnewtcb;	/* Ptr to TCB for new connection*/
	int32	i;			/* Walks through TCP devices	*/

	/* When listening, incoming segment must be a SYN */

	if ((pkt->net_tcpcode & TCPF_SYN) == 0) {
		return SYSERR;
	}

	/* Obtain exclusive use */

	wait (Tcp.tcpmutex);

	/* Find a free TCB for this connection */

	for (i = 0; i < Ntcp; i++) {
	    if (tcbtab[i].tcb_state == TCB_FREE)
		break;
	}

	if (i == Ntcp) {
	    signal (Tcp.tcpmutex);
	    return SYSERR;
	}

	/*	initialize the TCB				*/

	pnewtcb = &tcbtab[i];
	tcbclear (pnewtcb);

	/* Kyu */
	pnewtcb->tcb_lfunc = tcbptr->tcb_lfunc;
	pnewtcb->tcb_slot = i;
	/* end of Kyu addition */

	pnewtcb->tcb_rbuf = (char *) kyu_getmem ( tcp_bufsize );
	if (pnewtcb->tcb_rbuf == (char *)SYSERR) {
		signal (Tcp.tcpmutex);
		return SYSERR;
	}
	pnewtcb->tcb_rbsize = tcp_bufsize;

	pnewtcb->tcb_rbdata = pnewtcb->tcb_rbuf;
	pnewtcb->tcb_rbend = pnewtcb->tcb_rbuf + pnewtcb->tcb_rbsize;
	//pnewtcb->tcb_rbsize = 25*1024;
	pnewtcb->tcb_sbuf = (char *) kyu_getmem ( tcp_bufsize );
	if (pnewtcb->tcb_sbuf == (char *)SYSERR) {
		kyu_freemem ((char *)pnewtcb->tcb_rbuf, tcp_bufsize );
		signal (Tcp.tcpmutex);
		return SYSERR;
	}
	pnewtcb->tcb_sbsize = tcp_bufsize;

	/* New connection is in SYN-Received State */

	pnewtcb->tcb_state = TCB_SYNRCVD;
	tcbref (pnewtcb);
	wait (pnewtcb->tcb_mutex);
	if (mqsend (tcbptr->tcb_lq, i) == SYSERR) {
		tcbunref (pnewtcb);
		signal (pnewtcb->tcb_mutex);
		signal (Tcp.tcpmutex);
		return SYSERR;
	}
	signal (Tcp.tcpmutex);

	tcbptr->tcb_qlen++;
	if (tcbptr->tcb_readers > 0) {
		tcbptr->tcb_readers--;
		signal (tcbptr->tcb_rblock);
	}

	/* Copy data from SYN segment to TCB */

	pnewtcb->tcb_lip = pkt->net_ipdst;
	pnewtcb->tcb_lport = pkt->net_tcpdport;
	pnewtcb->tcb_rip = pkt->net_ipsrc;
	pnewtcb->tcb_rport = pkt->net_tcpsport;

	/* Record sequence number and window size */

	pnewtcb->tcb_rnext = pnewtcb->tcb_rbseq = pkt->net_tcpseq + 1;
	pnewtcb->tcb_rwnd = pnewtcb->tcb_ssthresh = pkt->net_tcpwindow;
	pnewtcb->tcb_snext = pnewtcb->tcb_suna = pnewtcb->tcb_ssyn = 1;
	pnewtcb->tcb_ssthresh = 0x0fffffff;

	/* Handle any data in the segment (unexpected, but required) */

	tcpdata (pnewtcb, pkt);

	/* Can this deadlock? */
	tcpxmit (pnewtcb, pnewtcb->tcb_snext);

	signal (pnewtcb->tcb_mutex);

	return OK;
}
