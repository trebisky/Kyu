/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket.c	8.3 (Berkeley) 4/15/94
 */

#include <bsd.h>

#include <kyu.h>
#include <thread.h>

void socantrcvmore ( struct socket * );
static void soqinsque ( struct socket *, struct socket *, int );
int soqremque ( struct socket *, int );

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)

/* under unix (BSD), write, send, sento, and sendmsg are all ways
 * of writing to a network connection.
 * write ( fd, buf, len) is like send with 0 flags
 * send ( fd, buf, len, flags ) allows various flags
 * sendto ( fd, buf, len, flags, NULL, 0 ) is the same as send()
 * sendmsg ( fd, msghdr, flags ) uses the msghdr structure
 *
 * Among other things the flags can specify out of band data.
 * write calls soo_write() in sys_socket.c
 *  this calls sosend((struct socket *)fp->f_data,
 *	(struct mbuf *)0, uio, (struct mbuf *)0, (struct mbuf *)0, 0));
 * (so the data is in a uio structure).
 * send calls osend which calls (in uipc_syscalls.c)
 *  sendit(p, uap->s, &msg, uap->flags, retval)
 *  here is msg has the address fields zeroed.
 * sendto calls:
 *  sendit(p, uap->s, &msg, uap->flags, retval))
 *  the difference here is msg has the address loaded.
 * sendmsg makes the same call, but directly passes
 *  the msg structure set up by the user.
 * sendit does a lot of work, but ultimately calls:
 *  sosend((struct socket *)fp->f_data, to, &auio,
 *          (struct mbuf *)0, control, flags)) {
 *
 * So every one of these uses the uio structure to pass the
 *  buffer to sosend.  The only call to sosend I can find that
 *  uses the "top" argument is for NFS.
 */

void
tcp_send ( struct socket *so, char *buf, int len )
{
	struct uio k_uio;
	struct iovec k_vec;
	int error;

	printf ( "kyu_send - sending %d bytes: %s\n", len, buf );

	k_vec.iov_base = buf;
	k_vec.iov_len = len;

	k_uio.uio_iov = &k_vec;
        k_uio.uio_iovcnt = 1;

        k_uio.uio_segflg = UIO_USERSPACE;
        k_uio.uio_rw = UIO_WRITE;

        k_uio.uio_offset = 0;
        // k_uio.uio_resid = 0;
        k_uio.uio_resid = len;

	error = sosend (so, (struct mbuf *)0, &k_uio,
	    (struct mbuf *)0, (struct mbuf *)0, 0);

	// return (sosend((struct socket *)fp->f_data, (struct mbuf *)0,
        //         uio, (struct mbuf *)0, (struct mbuf *)0, 0));
}

/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 *
 * The data to be sent is described by "uio" if nonzero,
 * otherwise by the mbuf chain "top" (which must be null
 * if uio is not).  Data provided in mbuf chain must be small
 * enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(so, addr, uio, top, control, flags)
	struct socket *so;
	struct mbuf *addr;
	struct uio *uio;
	struct mbuf *top;
	struct mbuf *control;
	int flags;
{
	// struct proc *p = curproc;
	struct mbuf **mp;
	struct mbuf *m;
	long space, len, resid;
	int clen = 0, error, s, mlen;
	// int dontroute;
	int atomic;
	struct inpcb *inp;
        struct tcpcb *tp;
	char *p;	// XXX for debug

	atomic = sosendallatonce(so) || top;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;

	printf ( "sosend, resid = %d\n", resid );
	/*
	 * In theory resid should be unsigned.
	 * However, space must be signed, as it might be less than 0
	 * if we over-committed, and we must use a signed comparison
	 * of space and resid.  On the other hand, a negative resid
	 * causes us to loop sending 0-length segments to the protocol.
	 */
	if (resid < 0)
		return (EINVAL);

	// dontroute =
	//     (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	//     (so->so_proto->pr_flags & PR_ATOMIC);

	// p->p_stats->p_ru.ru_msgsnd++;

	if (control)
		clen = control->m_len;

restart:
	if (error = sblock(&so->so_snd, SBLOCKWAIT(flags)))
		goto out;

#ifdef notYET
#endif /* notYET */

#define	snderr(err)	{ error = err; splx(s); goto release; }


	do {	/* Big loop */
		s = splnet();

		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);

		if (so->so_error)
			snderr(so->so_error);

		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}

		space = sbspace(&so->so_snd);
		printf ( "sosend, space = %d\n", space );

		if (flags & MSG_OOB)
			space += 1024;

		if (atomic && resid > so->so_snd.sb_hiwat ||
		    clen > so->so_snd.sb_hiwat)
			snderr(EMSGSIZE);

		/* Block waiting for space if necessary and allowed */
		if (space < resid + clen && uio &&
		    (atomic || space < so->so_snd.sb_lowat || space < clen)) {
			if (so->so_state & SS_NBIO)
				snderr(EWOULDBLOCK);

			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);	/* Blocks */

			splx(s);
			if (error)
				goto out;
			goto restart;
		}

		splx(s);
		mp = &top;
		space -= clen;

		do {	/* Little loop */
		    if (uio == NULL) {
			/*
			 * Data is prepackaged in "top".
			 */
			resid = 0;
			if (flags & MSG_EOR)
				top->m_flags |= M_EOR;
		    } else do {
			if (top == 0) {
				// MGETHDR(m, M_WAIT, MT_DATA);
				m = mb_gethdr ( MT_DATA );

				mlen = MHLEN;
				m->m_pkthdr.len = 0;
				m->m_pkthdr.rcvif = (struct ifnet *)0;
			} else {
				// MGET(m, M_WAIT, MT_DATA);
				m = mb_get ( MT_DATA );
				mlen = MLEN;
			}
			printf ( "sosend, space, resid = %d, %d\n", space, resid );

			if (resid >= MINCLSIZE && space >= MCLBYTES) {
				// MCLGET(m, M_WAIT);
				mb_clget ( m );

				if ((m->m_flags & M_EXT) == 0)
					goto nopages;

				mlen = MCLBYTES;
#ifdef	MAPPED_MBUFS
				len = min(MCLBYTES, resid);
#else
				if (atomic && top == 0) {
					len = min(MCLBYTES - max_hdr, resid);
					m->m_data += max_hdr;
				} else
					len = min(MCLBYTES, resid);
#endif
				space -= MCLBYTES;
			} else {
nopages:
				len = min(min(mlen, resid), space);
				space -= len;
				/*
				 * For datagram protocols, leave room
				 * for protocol headers in first mbuf.
				 */
				if (atomic && top == 0 && len < mlen)
					MH_ALIGN(m, len);
			}

			// error = uiomove(mtod(m, caddr_t), (int)len, uio);
			// printf ( "Calling xiomove for %d\n", len );

			error = xiomove(mtod(m, caddr_t), (int)len, uio, 0 );

			// p = mtod(m, caddr_t);
			// printf ( "After xiomove: %s\n", p );

			resid = uio->uio_resid;
			m->m_len = len;
			*mp = m;

			// p = mtod(top, caddr_t);
			// printf ( "top 1: %s\n", p );
			// printf ( "top 2: %s\n", top->m_data );

			top->m_pkthdr.len += len;
			// printf ( "top 1x: %s\n", p );
			// printf ( "top 2x: %s\n", top->m_data );
			if (error)
				goto release;

			mp = &m->m_next;
			printf ( "-- resid = %d\n", resid );
			if (resid <= 0) {
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
				break;
			}
		    } while (space > 0 && atomic);

		    // if (dontroute)
		    // 	    so->so_options |= SO_DONTROUTE;

		    // p = mtod(top, caddr_t);
		    // printf ( "top 1a: %s\n", p );
		    // printf ( "top 2a: %s\n", top->m_data );

#ifdef notdef
		    s = splnet();				/* XXX */
		    error = (*so->so_proto->pr_usrreq)(so,
			(flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			top, addr, control);
		    splx(s);
#endif

#ifdef KYU
		    /* Kyu ignores OOB (for now anyway).
		     * also assumes these pointers won't be NULL.
		     */
		    inp = sotoinpcb(so);
		    tp = intotcpcb(inp);

		    if ( top->m_len )
			printf ( "sosend: mb-data: (%d) %s\n", top->m_len, top->m_data );
		    else
			printf ( "sosend: mb-len: %d\n", top->m_len );

		    sbappend(&so->so_snd, top);
		    error = tcp_output(tp);

		    // printf ( "sosend: append: %d\n", error );
#endif

		    // if (dontroute)
		    // 	    so->so_options &= ~SO_DONTROUTE;

		    clen = 0;
		    control = 0;
		    top = 0;
		    mp = &top;
		    if (error)
			goto release;
		} while (resid && space > 0);

	} while (resid);

release:
	sbunlock(&so->so_snd);
out:
	if (top)
		mb_freem(top);
	if (control)
		mb_freem(control);

	if ( error )
	    printf ( "sosend - FINISH: %d\n", error );
	return (error);
}

/* My butchered version of uiomove() -- (see below)
 */
int
xiomove(cp, n, uio, read)
        caddr_t cp;
        int n;
        struct uio *uio;
	int read;
{
        struct iovec *iov;
        u_int cnt;
        int error = 0;

        while (n > 0 && uio->uio_resid) {
                iov = uio->uio_iov;
                cnt = iov->iov_len;
		// printf ( "xiomove: %d\n", cnt );
		// printf ( "xiomove: len = %d\n", iov->iov_len );
		// printf ( "xiomove: buf = %s\n", iov->iov_base );
                if (cnt == 0) {
                        uio->uio_iov++;
                        uio->uio_iovcnt--;
                        continue;
                }
                if (cnt > n)
                        cnt = n;

		// bcopy ( src, dst, len )
		if ( read )
		    bcopy((caddr_t)cp, iov->iov_base, cnt);
		else
		    bcopy(iov->iov_base, (caddr_t)cp, cnt);

                iov->iov_base += cnt;
                iov->iov_len -= cnt;

                uio->uio_resid -= cnt;
                uio->uio_offset += cnt;

                cp += cnt;
                n -= cnt;
        }

        return (error);
}

#ifdef notdef
/* From kern/kern_subr.c
 */
int
uiomove(cp, n, uio)
        register caddr_t cp;
        register int n;
        register struct uio *uio;
{
        register struct iovec *iov;
        u_int cnt;
        int error = 0;

#ifdef DIAGNOSTIC
        if (uio->uio_rw != UIO_READ && uio->uio_rw != UIO_WRITE)
                panic("uiomove: mode");
        if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
                panic("uiomove proc");
#endif
        while (n > 0 && uio->uio_resid) {
                iov = uio->uio_iov;
                cnt = iov->iov_len;
                if (cnt == 0) {
                        uio->uio_iov++;
                        uio->uio_iovcnt--;
                        continue;
                }
                if (cnt > n)
                        cnt = n;
                switch (uio->uio_segflg) {

                case UIO_USERSPACE:
                case UIO_USERISPACE:
                        if (uio->uio_rw == UIO_READ)
                                error = copyout(cp, iov->iov_base, cnt);
                        else
                                error = copyin(iov->iov_base, cp, cnt);
                        if (error)
                                return (error);
                        break;

                case UIO_SYSSPACE:
                        if (uio->uio_rw == UIO_READ)
                                bcopy((caddr_t)cp, iov->iov_base, cnt);
                        else
                                bcopy(iov->iov_base, (caddr_t)cp, cnt);
                        break;
                }
                iov->iov_base += cnt;
                iov->iov_len -= cnt;
                uio->uio_resid -= cnt;
                uio->uio_offset += cnt;
                cp += cnt;
                n -= cnt;
        }
        return (error);
}
#endif

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, propoerly linked into the
 * data structure of the original socket, and return this.
 * Connstatus may be 0, or SO_ISCONFIRMING, or SO_ISCONNECTED.
 */
/* From kern/uipc_socket2.c
 */
struct socket *
sonewconn ( struct socket *head, int connstatus )
{
        struct socket *so;
        int soqueue = connstatus ? 1 : 0;
	int error;

	printf ( "sonewconn - %08x %d\n", head, connstatus );

        if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
                return ((struct socket *)0);

        // MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_DONTWAIT);
	so = (struct socket *) k_sock_alloc ();
        if (so == NULL)
                return ((struct socket *)0);

        bzero((caddr_t)so, sizeof(*so));

	so->kyu_sem = sem_signal_new ( SEM_FIFO );
	if ( ! so->kyu_sem ) {
	    panic ( "sonewcomm1 - semaphores all gone" );
	}

        so->so_type = head->so_type;
        so->so_options = head->so_options &~ SO_ACCEPTCONN;
        so->so_linger = head->so_linger;
        so->so_state = head->so_state | SS_NOFDREF;
        so->so_proto = head->so_proto;
        so->so_timeo = head->so_timeo;
        so->so_pgid = head->so_pgid;

	// This call is/was weird, it passes sb_hiwat, yet soreserve sets it.
	// Kyu: I just did away with this weirdness.
        // (void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
	// XXX in fact, this gets called in the ATTACH, so why do this here?
        // sb_init ( so );

        soqinsque(head, so, soqueue);

        // if ((*so->so_proto->pr_usrreq)(so, PRU_ATTACH,
        //     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0)) {
        error = tcp_usrreq (so, PRU_ATTACH,
                (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	if ( error ) {
                (void) soqremque(so, soqueue);
		sem_destroy ( so->kyu_sem );

                // (void) free((caddr_t)so, M_SOCKET);
		k_sock_free ( so );
                return ((struct socket *)0);
        }

        if (connstatus) {
                sorwakeup(head);

                // wakeup((caddr_t)&head->so_timeo);
		sem_unblock ( so->kyu_sem );

                so->so_state |= connstatus;
        }

        return (so);
}

/* ------------- */
/* ------------- */
/* ------------- */

static void
soqinsque ( struct socket *head, struct socket *so, int q )
{
        struct socket **prev;

        so->so_head = head;

        if (q == 0) {
                head->so_q0len++;
                so->so_q0 = 0;
                for (prev = &(head->so_q0); *prev; )
                        prev = &((*prev)->so_q0);
        } else {
                head->so_qlen++;
                so->so_q = 0;
                for (prev = &(head->so_q); *prev; )
                        prev = &((*prev)->so_q);
        }
        *prev = so;
}

int
soqremque ( struct socket *so, int q )
{
        struct socket *head, *prev, *next;

        head = so->so_head;
        prev = head;

        for (;;) {
                next = q ? prev->so_q : prev->so_q0;
                if (next == so)
                        break;
                if (next == 0)
                        return (0);
                prev = next;
        }
        if (q == 0) {
                prev->so_q0 = next->so_q0;
                head->so_q0len--;
        } else {
                prev->so_q = next->so_q;
                head->so_qlen--;
        }
        next->so_q0 = next->so_q = 0;
        next->so_head = 0;
        return (1);
}

void
sorflush ( struct socket *so )
{
        struct sockbuf *sb = &so->so_rcv;
        struct protosw *pr = so->so_proto;
        int s;
        struct sockbuf asb;

	// printf ( "Lazy sorflush\n" );
        sb->sb_flags |= SB_NOINTR;
        (void) sblock(sb, M_WAITOK);

        s = splimp();
        socantrcvmore(so);
        sbunlock(sb);
        asb = *sb;
        bzero((caddr_t)sb, sizeof (*sb));
        splx(s);

        // if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
        //         (*pr->pr_domain->dom_dispose)(asb.sb_mb);
        sbrelease(&asb);
}

void
sofree ( struct socket *so )
{
	printf ( "sofree called\n" );

        if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
                return;

        if (so->so_head) {
                if (!soqremque(so, 0) && !soqremque(so, 1))
                        panic("sofree dq");
                so->so_head = 0;
        }

        sbrelease(&so->so_snd);
        sorflush(so);

	sem_destroy ( so->kyu_sem );

        // FREE(so, M_SOCKET);
	k_sock_free ( so );
}

/*
 * Procedures to manipulate state flags of socket
 * and do appropriate wakeups.  Normal sequence from the
 * active (originating) side is that soisconnecting() is
 * called during processing of connect() call,
 * resulting in an eventual call to soisconnected() if/when the
 * connection is established.  When the connection is torn down
 * soisdisconnecting() is called during processing of disconnect() call,
 * and soisdisconnected() is called when the connection to the peer
 * is totally severed.  The semantics of these routines are such that
 * connectionless protocols can call soisconnected() and soisdisconnected()
 * only, bypassing the in-progress calls when setting up a ``connection''
 * takes no time.
 *
 * From the passive side, a socket is created with
 * two queues of sockets: so_q0 for connections in progress
 * and so_q for connections already made and awaiting user acceptance.
 * As a protocol is preparing incoming connections, it creates a socket
 * structure queued on so_q0 by calling sonewconn().  When the connection
 * is established, soisconnected() is called, and transfers the
 * socket structure to so_q, making it available to accept().
 *
 * If a socket is closed with sockets on either
 * so_q0 or so_q, these sockets are dropped.
 *
 * If higher level protocols are implemented in
 * the kernel, the wakeups done here will sometimes
 * cause software-interrupt process scheduling.
 */
/* From kern/uipc_socket2.c */

void
soisconnecting(so)
        register struct socket *so;
{

        so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
        so->so_state |= SS_ISCONNECTING;
}

void
soisconnected(so)
        struct socket *so;
{
        struct socket *head = so->so_head;

        so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING|SS_ISCONFIRMING);
        so->so_state |= SS_ISCONNECTED;

        if (head && soqremque(so, 0)) {
                soqinsque(head, so, 1);
                sorwakeup(head);

		printf ( "soisconnected - wakeup head %08x\n", head->kyu_sem );
                // wakeup((caddr_t)&head->so_timeo);
		sem_unblock ( head->kyu_sem );

        } else {
		printf ( "soisconnected - wakeup sock\n" );
                // wakeup((caddr_t)&so->so_timeo);
		sem_unblock ( so->kyu_sem );

                sorwakeup(so);
                sowwakeup(so);
        }
}

void
soisdisconnecting(so)
        register struct socket *so;
{

        so->so_state &= ~SS_ISCONNECTING;
        so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);

        // wakeup((caddr_t)&so->so_timeo);
	sem_unblock ( so->kyu_sem );

        sowwakeup(so);
        sorwakeup(so);
}

void
soisdisconnected(so)
        register struct socket *so;
{

        so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
        so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);

        // wakeup((caddr_t)&so->so_timeo);
	sem_unblock ( so->kyu_sem );

        sowwakeup(so);
        sorwakeup(so);
}

/*
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 */

void
socantsendmore ( struct socket *so )
{

        so->so_state |= SS_CANTSENDMORE;
        sowwakeup(so);
}

void
socantrcvmore ( struct socket *so )
{

        so->so_state |= SS_CANTRCVMORE;
        sorwakeup(so);
}

/* From kern/uipc_socket.c */
void
sohasoutofband(so)
        register struct socket *so;
{
#ifdef notdef
        struct proc *p;

        if (so->so_pgid < 0)
                gsignal(-so->so_pgid, SIGURG);
        else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
                psignal(p, SIGURG);
        selwakeup(&so->so_rcv.sb_sel);
#endif
}

// THE END
