/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 */

#include <bsd.h>

// From protosw.h
/*
 * The arguments to usrreq are:
 *      (*protosw[].pr_usrreq)(up, req, m, nam, opt);
 * where up is a (struct socket *), req is one of these requests,
 * m is a optional mbuf chain containing a message,
 * nam is an optional mbuf chain containing an address,
 * and opt is a pointer to a socketopt structure or nil.
 * The protocol is responsible for disposal of the mbuf chain m,
 * the caller is responsible for any space held by nam and opt.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define PRU_ATTACH              0       /* attach protocol to up */
#define PRU_DETACH              1       /* detach protocol from up */
#define PRU_BIND                2       /* bind socket to address */
#define PRU_LISTEN              3       /* listen for connection */
#define PRU_CONNECT             4       /* establish connection to peer */
#define PRU_ACCEPT              5       /* accept connection from peer */
#define PRU_DISCONNECT          6       /* disconnect from peer */
#define PRU_SHUTDOWN            7       /* won't send any more data */
#define PRU_RCVD                8       /* have taken data; more room now */
#define PRU_SEND                9       /* send this data */
#define PRU_ABORT               10      /* abort (fast DISCONNECT, DETATCH) */
#define PRU_CONTROL             11      /* control operations on protocol */
#define PRU_SENSE               12      /* Never used: return status into m */
#define PRU_RCVOOB              13      /* retrieve out of band data */
#define PRU_SENDOOB             14      /* send out of band data */
#define PRU_SOCKADDR            15      /* Never used: fetch socket's address */
#define PRU_PEERADDR            16      /* Never used: fetch peer's address */
#define PRU_CONNECT2            17      /* Never used: connect two sockets */
/* begin for protocols internal use */
#define PRU_FASTTIMO            18      /* Never used: 200ms timeout */
#define PRU_SLOWTIMO            19      /* 500ms timeout */
#define PRU_PROTORCV            20      /* Never used: receive from below */
#define PRU_PROTOSEND           21      /* Never used: send to below */

/* At this time Kyu never uses PRU_CONTROL.
 * BSD used it to handle ioctl() calls to set interface information.
 */

static int tcp_usrreq ( struct socket *, int, struct mbuf *, struct mbuf *, struct mbuf * );

/* Originally the protosw existed to support multiple protocols
 * and they would call protocol specific routines for the
 * functions enumerated here.  Since we only support TCP in
 * this code, I want to do away with protosw.h entirely.
 * This will probably be a 3 phase cleanup.
 * 1 - introduce the proto_* routines here.
 * 2 - migrate code from the general tcp_usrreq into them.
 * 3 - in many/some cases move that code to where it is used
 */

int
proto_attach ( struct socket *so )
{
	return tcp_usrreq (so, PRU_ATTACH,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
}

int
proto_detach ( struct socket *so )
{
	struct inpcb *inp;
	struct tcpcb *tp;

	// return tcp_usrreq (so, PRU_DETACH,
	//     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	/*
	 * PRU_DETACH detaches the TCP protocol from the socket.
	 * If the protocol state is non-embryonic, then can't
	 * do this directly: have to initiate a PRU_DISCONNECT,
	 * which may finish later; embryonic TCB's can just
	 * be discarded here.
	 */
	net_lock ();

	inp = sotoinpcb(so);
	tp = intotcpcb(inp);

	if (tp->t_state > TCPS_LISTEN)
	    tp = tcp_disconnect(tp);
	else
	    tp = tcpcb_close(tp);

	net_unlock ();
}

int
proto_bind ( struct socket *so, struct mbuf *nam )
{
	return tcp_usrreq (so, PRU_BIND,
	    (struct mbuf *)0, nam, (struct mbuf *)0);
}

int
proto_listen ( struct socket *so )
{
	return tcp_usrreq (so, PRU_LISTEN,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
}

int
proto_connect ( struct socket *so, struct mbuf *nam )
{
	return tcp_usrreq (so, PRU_CONNECT,
	    (struct mbuf *)0, nam, (struct mbuf *)0);
}

#ifdef notdef
int
proto_accept ( struct socket *so, struct mbuf *nam )
{
	struct inpcb *inp;

	net_lock ();
	inp = sotoinpcb(so);
	if ( inp == 0 )
	    return (EINVAL);

	in_setpeeraddr(inp, nam);
	net_unlock ();

	return 0;
}
#endif

int
proto_disconnect ( struct socket *so )
{
	return tcp_usrreq (so, PRU_DISCONNECT,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
}
/*
 * Mark the connection as being incapable of further output.
 */
void
proto_shutdown ( struct socket *so )
{
	struct tcpcb *tp;
	struct inpcb *inp;

	// (void) tcp_usrreq (so, PRU_SHUTDOWN,
	//     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	net_lock ();

	socantsendmore ( so );

	inp = sotoinpcb(so);

	if ( inp  ) {

	    tp = intotcpcb(inp);

	    tp = tcp_usrclosed ( tp );
	    if ( tp )
		(void) tcp_output ( tp );
	}

	net_unlock ();
}

/*
 * After a receive, possibly send window update to peer.
 */
int
proto_rcvd ( struct socket *so )
{
	struct tcpcb *tp;
	struct inpcb *inp;

	// return tcp_usrreq (so, PRU_RCVD,
	//     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	net_lock ();

	inp = sotoinpcb(so);
	if ( inp == 0 ) {
	    net_unlock ();
	    return (EINVAL);
	}

	tp = intotcpcb(inp);

	(void) tcp_output(tp);

	net_unlock ();

	return 0;
}



#ifdef notdef
/*
 * Do a send by putting data in output queue and updating urgent
 * marker if URG set.  Possibly send more data.
 */
int
proto_send ( struct socket *so, struct mbuf *m )
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int rv;

	// return tcp_usrreq (so, PRU_SEND,
	//     m, (struct mbuf *)0, (struct mbuf *)0);

	net_lock ();

	inp = sotoinpcb(so);
	if ( inp == 0 ) {
	    net_unlock ();
	    return (EINVAL);
	}

	tp = intotcpcb(inp);

	sbappend(&so->so_snd, m);
	rv = tcp_output(tp);

	net_unlock ();

	return rv;

}

/* Needs verification (and testing).
 * not currently used.
 */
int
proto_sendoob ( struct socket *so, struct mbuf *m )
{
	return tcp_usrreq (so, PRU_SEND,
	    m, (struct mbuf *)0, (struct mbuf *)0);
}

#endif

/* Called, but not in use yet */
int
proto_recvoob ( struct socket *so, struct mbuf *m, int peek )
{
	return tcp_usrreq (so, PRU_RCVOOB,
	    m, (struct mbuf *)peek, (struct mbuf *)0);
}


void
proto_abort ( struct socket *so )
{
	struct tcpcb *tp;
	struct inpcb *inp;

	// return tcp_usrreq (so, PRU_ABORT,
	//     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	net_lock ();

	inp = sotoinpcb(so);
	if ( inp == 0 ) {
	    net_unlock ();
	    return;
	}

	tp = intotcpcb(inp);

	// tp was only returned to pass to tcp_trace
	// tp = tcp_drop(tp, ECONNABORTED);
	(void) tcp_drop ( tp, ECONNABORTED );

	net_unlock ();
}

/*
 * TCP slow timer went off; going through this
 * routine for tracing's sake.
 *    PRU_SLOWTIMO
 */
void
proto_slowtimo ( struct socket *so, int index )
{
	struct tcpcb *tp;
	struct inpcb *inp;

	net_lock ();

	inp = sotoinpcb(so);
	if ( inp == 0 ) {
	    net_unlock ();
	    return;
	}

	tp = intotcpcb(inp);

	// tp = tcp_timers ( tp, index );
	(void) tcp_timers ( tp, index );

	net_unlock ();

	/* for debug's sake */
	// req |= (long)nam << 8;
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/*
 * TCP protocol interface to socket abstraction.
 */
extern	char *tcpstates[];

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/*ARGSUSED*/
static int
tcp_usrreq ( struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	//int s;
	int error = 0;
	int ostate;

#ifdef KYU
	if (req == PRU_CONTROL)
	    return (EINVAL);
#else
	    /* Handle ioctl */
	if (req == PRU_CONTROL)
	    return in_control(so, (long)m, (caddr_t)nam, (struct ifnet *)control);
#endif

	if (control && control->m_len) {
		// m_freem(control);
		mb_freem(control);
		if (m)
			mb_freem(m);
		return (EINVAL);
	}

	// s = splnet();
	net_lock ();

	inp = sotoinpcb(so);
	// bpf3 ( "tcp_usrreq 1: req, so, inp = %d, %08x, %08x\n", req, so, inp );

	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidary (struct tcpcb).
	 */
	if (inp == 0 && req != PRU_ATTACH) {
		// splx(s);
		net_unlock ();
		return (EINVAL);		/* XXX */
	}

	// bpf3 ( "tcp_usrreq 2\n" );

	if (inp) {
		tp = intotcpcb(inp);
		/* WHAT IF TP IS 0? */
#ifdef KPROF
		tcp_acounts[tp->t_state][req]++;
#endif
		ostate = tp->t_state;
	} else
		ostate = 0;


	switch (req) {

	/*
	 * TCP attaches to socket via PRU_ATTACH, reserving space,
	 * and an internet control block.
	 */
	case PRU_ATTACH:
		// bpf3 ( "tcp_usrreq 3\n" );
		if (inp) {
			error = EISCONN;
			break;
		}
		// bpf3 ( "tcp_usrreq 3b\n" );
		error = tcp_attach(so);
		if (error)
			break;
		// bpf3 ( "tcp_usrreq 4\n" );
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME;
		tp = sototcpcb(so);
		// bpf3 ( "tcp_usrreq 5\n" );
		break;

	/*
	 * PRU_DETACH detaches the TCP protocol from the socket.
	 * If the protocol state is non-embryonic, then can't
	 * do this directly: have to initiate a PRU_DISCONNECT,
	 * which may finish later; embryonic TCB's can just
	 * be discarded here.
	 */
	case PRU_DETACH:
		if (tp->t_state > TCPS_LISTEN)
			tp = tcp_disconnect(tp);
		else
			tp = tcpcb_close(tp);
		break;

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
		// bpf3 ( "tcp_usrreq 10\n" );
		error = in_pcbbind(inp, nam);
		// bpf3 ( "tcp_usrreq 11 %d\n", error );
		if (error)
			break;
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
		if (inp->inp_lport == 0)
			error = in_pcbbind(inp, (struct mbuf *)0);
		if (error == 0)
			tp->t_state = TCPS_LISTEN;
		break;

	/*
	 * Initiate connection to peer.
	 * Create a template for use in transmissions on this connection.
	 * Enter SYN_SENT state, and mark socket as connecting.
	 * Start keep-alive timer, and seed output sequence space.
	 * Send initial segment on connection.
	 */
	case PRU_CONNECT:
		// bpf2 ( "Tconnect 0\n" );
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, (struct mbuf *)0);
			if (error)
				break;
		}

		// bpf2 ( "Tconnect 1\n" );
		error = in_pcbconnect(inp, nam);
		if (error)
			break;

		// bpf2 ( "Tconnect 2\n" );
		tp->t_template = tcp_template(tp);
		if (tp->t_template == 0) {
			in_pcbdisconnect(inp);
			error = ENOBUFS;
			break;

		}
		// bpf2 ( "Tconnect 3\n" );

		/* Compute window scaling to request.  */
		while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
		    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
			tp->request_r_scale++;

		soisconnecting(so);
		tcpstat.tcps_connattempt++;
		tp->t_state = TCPS_SYN_SENT;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		tp->iss = tcp_iss; tcp_iss += TCP_ISSINCR/2;
		// bpf2 ( "Tconnect 4\n" );

		tcp_sendseqinit(tp);
		error = tcp_output(tp);

		// bpf2 ( "Tconnect 5\n" );
		break;

	/*
	 * Create a TCP connection between two sockets.
	 */
	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Initiate disconnect from peer.
	 * If connection never passed embryonic stage, just drop;
	 * else if don't need to let data drain, then can just drop anyways,
	 * else have to begin TCP shutdown process: mark socket disconnecting,
	 * drain unread data, state switch to reflect user close, and
	 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
	 * when peer sends FIN and acks ours.
	 *
	 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
	 */
	case PRU_DISCONNECT:
		tp = tcp_disconnect(tp);
		break;

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	case PRU_ACCEPT:
		in_setpeeraddr(inp, nam);
		break;

	/*
	 * Mark the connection as being incapable of further output.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp);
		break;

	/*
	 * After a receive, possibly send window update to peer.
	 */
	case PRU_RCVD:
		(void) tcp_output(tp);
		break;

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	case PRU_SEND:
		sbappend(&so->so_snd, m);
		error = tcp_output(tp);
		break;

	/*
	 * Abort the TCP.
	 */
	case PRU_ABORT:
		tp = tcp_drop(tp, ECONNABORTED);
		break;

#ifdef notdef
	case PRU_SENSE:
		((struct stat *) m)->st_blksize = so->so_snd.sb_hiwat;
		// (void) splx(s);
		net_unlock ();
		return (0);
#endif

	case PRU_RCVOOB:
		if ((so->so_oobmark == 0 &&
		    (so->so_state & SS_RCVATMARK) == 0) ||
		    so->so_options & SO_OOBINLINE ||
		    tp->t_oobflags & TCPOOB_HADDATA) {
			error = EINVAL;
			break;
		}
		if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
			error = EWOULDBLOCK;
			break;
		}
		m->m_len = 1;
		*mtod(m, caddr_t) = tp->t_iobc;
		if (((long)nam & MSG_PEEK) == 0)
			tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		break;

	case PRU_SENDOOB:
		if (sbspace(&so->so_snd) < -512) {
			// m_freem(m);
			mb_freem(m);
			error = ENOBUFS;
			break;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappend(&so->so_snd, m);
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_force = 1;
		error = tcp_output(tp);
		tp->t_force = 0;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

#ifdef notdef
	/*
	 * TCP slow timer went off; going through this
	 * routine for tracing's sake.
	 */
	case PRU_SLOWTIMO:
		tp = tcp_timers(tp, (long)nam);
		req |= (long)nam << 8;		/* for debug's sake */
		break;
#endif

	default:
		bsd_panic("tcp_usrreq");
	}
	

	// bpf3 ( "tcp_usrreq 6\n" );

	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (struct tcpiphdr *)0, req);

	// splx(s);
	net_unlock ();

	return (error);
}

#ifdef notdef
// used by getsockopt and setsockopt calls
int
tcp_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int error = 0;
	// int s;
	struct inpcb *inp;
	register struct tcpcb *tp;
	register struct mbuf *m;
	register int i;

	// s = splnet();
	net_lock ();

	inp = sotoinpcb(so);
	if (inp == NULL) {
		// splx(s);
		net_unlock ();
		if (op == PRCO_SETOPT && *mp)
			(void) mb_free(*mp);
		return (ECONNRESET);
	}
	if (level != IPPROTO_TCP) {
		error = ip_ctloutput(op, so, level, optname, mp);
		// splx(s);
		net_unlock ();
		return (error);
	}
	tp = intotcpcb(inp);

	switch (op) {

	case PRCO_SETOPT:
		m = *mp;
		switch (optname) {

		case TCP_NODELAY:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:
			if (m && (i = *mtod(m, int *)) > 0 && i <= tp->t_maxseg)
				tp->t_maxseg = i;
			else
				error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void) mb_free(m);
		break;

	case PRCO_GETOPT:
		// *mp = m = m_get(M_WAIT, MT_SOOPTS);
		*mp = m = mb_get ( MT_SOOPTS );
		m->m_len = sizeof(int);

		switch (optname) {
		case TCP_NODELAY:
			*mtod(m, int *) = tp->t_flags & TF_NODELAY;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_maxseg;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}

	// splx(s);
	net_unlock ();

	return (error);
}
#endif

// u_long	tcp_sendspace = 1024*8;
// u_long	tcp_recvspace = 1024*8;

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
int
tcp_attach ( struct socket *so )
{
	register struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	// bpf3 ( "tcp_attach 0\n" );

	sb_init ( so );
	// printf ( "tcp_attach == sb_init done, so = %08x\n", so );

#ifdef notdef
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		// error = soreserve(so, tcp_sendspace, tcp_recvspace);
		error = sb_init ( so );
		if (error)
			return (error);
	}
#endif
	// bpf3 ( "tcp_attach 1\n" );

	error = in_pcballoc(so, &tcb);
	if (error)
		return (error);

	// bpf3 ( "tcp_attach 2\n" );

	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp);

	if (tp == 0) {
		// bpf3 ( "tcp_attach 2e\n" );

		// I got rid of this.  Will I regret it?  12-2022
		// int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
		in_pcbdetach(inp);
		// so->so_state |= nofd;
		so->so_state |= SS_NOFDREF;
		return (ENOBUFS);
	}
	// bpf3 ( "tcp_attach 3\n" );
	tp->t_state = TCPS_CLOSED;
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb *
tcp_disconnect(tp)
	register struct tcpcb *tp;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (tp->t_state < TCPS_ESTABLISHED)
		tp = tcpcb_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb *
tcp_usrclosed(tp)
	register struct tcpcb *tp;
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcpcb_close(tp);
		break;

	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2)
		soisdisconnected(tp->t_inpcb->inp_socket);
	return (tp);
}

/* THE END */
