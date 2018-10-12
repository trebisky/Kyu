/* tcpmopen.c  -  checktuple, tcpmopen */

#include <xinu.h>

// Prior to 6-27-2018 was "wired in" as 65535
extern int tcp_bufsize;

/*------------------------------------------------------------------------
 *  ckecktuple  -  Verify that a TCP connection is not already in use by
 *			checking (src IP, src port, Dst IP, Dst port)
 *------------------------------------------------------------------------
 */
static	int32	checktuple (
		  uint32	lip,	/* Local IP address		*/
		  uint16	lport,	/* Local TCP port number	*/
		  uint32	rip,	/* Remote IP address		*/
		  uint16	rport	/* Remote TCP port number	*/
		)
{
	int32	i;			/* Walks though TCBs		*/
	struct	tcb *tcbptr;		/* Ptr to TCB entry 		*/

	/* Check each TCB that is open */

	for (i = 0; i < Ntcp; i++) {
		tcbptr = &tcbtab[i];
		if (tcbptr->tcb_state != TCB_FREE
		    && tcbptr->tcb_lip == lip
		    && tcbptr->tcb_rip == rip
		    && tcbptr->tcb_lport == lport
		    && tcbptr->tcb_rport == rport) {
			return SYSERR;
		}
	}
	return OK;
}

/*------------------------------------------------------------------------
 *  tcp_register  -  Open a master the TCP device and receive the ID of a
 *			TCP slave device for the connection
 *------------------------------------------------------------------------
 */
int32	tcp_register (
	uint32	ip,
	uint16	port,
	int32	active
	)
{
	struct tcb	*tcbptr;	/* Ptr to TCB			*/
	uint32		lip;		/* Local IP address		*/
	int32		i;		/* Walks through TCBs		*/
	int32		state;		/* Connection state		*/
	int32		slot;		/* Slot in TCB table		*/

#ifdef notdef
	// char		*spec;		/* IP:port as a string		*/
	/* Parse "X:machine:port" string and set variables, where	*/
	/*	X	- either 'a' or 'p' for "active" or "passive"	*/
	/*	machine	- an IP address in dotted decimal		*/
	/*	port	- a protocol port number			*/

	spec = (char *)arg1;
	if (tcpparse (spec, &ip, &port, &active) == SYSERR) {
		return SYSERR;
	}
#endif
	/* Obtain exclusive use, find free TCB, and clear it */

	wait (Tcp.tcpmutex);
	for (i = 0; i < Ntcp; i++) {
		if (tcbtab[i].tcb_state == TCB_FREE)
			break;
	}
	if (i >= Ntcp) {
		signal (Tcp.tcpmutex);
		return SYSERR;
	}
	tcbptr = &tcbtab[i];
	tcbclear (tcbptr);
	slot = i;

	/* Either set up active or passive endpoint */

	if (active) {

		/* Obtain local IP address from interface */

#ifdef KYU
		lip = get_our_ip ();
#else
		lip = NetData.ipucast;
#endif

		/* Allocate receive buffer and initialize ptrs */

		// tcbptr->tcb_rbuf = (char *) kyu_getmem (65535);
		tcbptr->tcb_rbuf = (char *) kyu_getmem ( tcp_bufsize );

		if (tcbptr->tcb_rbuf == (char *)SYSERR) {
			signal (Tcp.tcpmutex);
			return SYSERR;
		}
		// tcbptr->tcb_rbsize = 65535;
		tcbptr->tcb_rbsize = tcp_bufsize;
		tcbptr->tcb_rbdata = tcbptr->tcb_rbuf;
		tcbptr->tcb_rbend = tcbptr->tcb_rbuf + tcbptr->tcb_rbsize;

		// tcbptr->tcb_sbuf = (char *) kyu_getmem (65535);
		tcbptr->tcb_sbuf = (char *) kyu_getmem ( tcp_bufsize );
		if (tcbptr->tcb_sbuf == (char *)SYSERR) {
			// kyu_freemem ((char *)tcbptr->tcb_rbuf, 65535);
			kyu_freemem ((char *)tcbptr->tcb_rbuf, tcp_bufsize );
			signal (Tcp.tcpmutex);
			return SYSERR;
		}
		// tcbptr->tcb_sbsize = 65535;
		tcbptr->tcb_sbsize = tcp_bufsize;

		/* The following will always succeed because	*/
		/*   the iteration covers at least Ntcp+1 port	*/
		/*   numbers and there are only Ntcp slots	*/

		for (i = 0; i < Ntcp + 1; i++) {
			if (checktuple (lip, Tcp.tcpnextport,
					      ip, port) == OK) {
				break;
			}
			Tcp.tcpnextport++;
			if (Tcp.tcpnextport > 63000)
				Tcp.tcpnextport = 33000;
		}

		tcbptr->tcb_lip = lip;

		/* Assign next local port */

		tcbptr->tcb_lport = Tcp.tcpnextport++;
		if (Tcp.tcpnextport > 63000) {
			Tcp.tcpnextport = 33000;
		}
		tcbptr->tcb_rip = ip;
		tcbptr->tcb_rport = port;
		tcbptr->tcb_snext = tcbptr->tcb_suna = tcbptr->tcb_ssyn = 1;
		tcbptr->tcb_state = TCB_SYNSENT;
		wait (tcbptr->tcb_mutex);
		tcbref (tcbptr);
		signal (Tcp.tcpmutex);

		tcbref (tcbptr);
		mqsend (Tcp.tcpcmdq, TCBCMD(tcbptr, TCBC_SEND));
		while (tcbptr->tcb_state != TCB_CLOSED
		       && tcbptr->tcb_state != TCB_ESTD) {
			tcbptr->tcb_readers++;
			signal (tcbptr->tcb_mutex);
			wait (tcbptr->tcb_rblock);
			wait (tcbptr->tcb_mutex);
		}
		if ((state = tcbptr->tcb_state) == TCB_CLOSED) {
			tcbunref (tcbptr);
		}
		signal (tcbptr->tcb_mutex);
		return (state == TCB_ESTD ? slot : SYSERR);

	} else {  /* Passive connection */
		for (i = 0; i < Ntcp; i++) {
			if (tcbtab[i].tcb_state == TCB_LISTEN
			    && tcbtab[i].tcb_lip == ip
			    && tcbtab[i].tcb_lport == port) {

				/* Duplicates prior connection */

				signal (Tcp.tcpmutex);
				return SYSERR;
			}
		}

		/* New passive endpoint - fill in TCB */

		tcbptr->tcb_lip = ip;
		tcbptr->tcb_lport = port;
		tcbptr->tcb_state = TCB_LISTEN;
		tcbref (tcbptr);
		signal (Tcp.tcpmutex);

		/* Return slave device to caller */

		return slot;
	}
}

/* Added for Kyu - 1-17-2017 */
int
tcp_server ( int port, lfptr func )
{
	struct tcb	*tcbptr;
	int slot;
	int i;

	wait (Tcp.tcpmutex);

	/* Ensure we are not already serving on this port */
	for (i = 0; i < Ntcp; i++) {
	    if (tcbtab[i].tcb_state == TCB_LISTEN
		// && tcbtab[i].tcb_lip == 0
		&& tcbtab[i].tcb_lport == port) {
		    signal (Tcp.tcpmutex);
		    return SYSERR;
		}
	}

	/* Find a free TCB */
	for (slot = 0; slot < Ntcp; slot++) {
	    if (tcbtab[slot].tcb_state == TCB_FREE)
		break;
	}

	if (slot >= Ntcp) {
	    signal (Tcp.tcpmutex);
	    return SYSERR;
	}

	/* New passive endpoint - fill in TCB */
	tcbptr = &tcbtab[slot];
	tcbclear (tcbptr);

	tcbptr->tcb_lfunc = func;
	tcbptr->tcb_slot = slot;
	tcbptr->tcb_lport = port;
	tcbptr->tcb_state = TCB_LISTEN;
	tcbref (tcbptr);

	signal (Tcp.tcpmutex);

	return OK;
}

/* THE END */
