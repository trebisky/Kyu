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

/* All this used to be in uio.h, but was moved here for Kyu
 * since this is the only place we use it.
 * (and it is nice to see it right alongside where it is used)
 * Someday perhaps we will do away with it.
 * (all well and good in BSD, but we live a simpler life).
 */

#ifdef notdef
enum	uio_rw { UIO_READ, UIO_WRITE };

/* Segment flag values. */
enum uio_seg {
	UIO_USERSPACE,		/* from user data space */
	UIO_SYSSPACE,		/* from system space */
	UIO_USERISPACE		/* from user I space */
};
#endif

/*
 * XXX --- iov_base should be a void *.
 */
struct iovec {
	char	*iov_base;	/* Base address. */
	size_t	 iov_len;	/* Length. */
};

struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_resid;
	//enum	uio_seg uio_segflg;
	//enum	uio_rw uio_rw;
	// struct	proc *uio_procp;
};

/* End of uio.h stuff */

void socantrcvmore ( struct socket * );
static void soqinsque ( struct socket *, struct socket *, int );
int soqremque ( struct socket *, int );
void soshutdown ( struct socket * );
void sorflush ( struct socket * );

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)

/* XXX - todo
 *
 * Some fine day.
 *
 * A number of things can be done to simplify (if nothing else) this code.
 * For one thing, protosw should eventually be eradicated.
 * This could lead to significant simplification of tcp_usrreq.c and
 * migration of that code to locations where it is currently referenced.
 *
 * The uiomove/xiomove business should be eradicated.
 * This is for scatter/gather IO which I have no intent of
 *  providing for.
 */

/* tcp_shutdown()
 * We found this sitting alongside of recv and send
 * and perhaps it will come in handy someday.
 */

void
tcp_shutdown ( struct socket *so )
{
	soshutdown ( so );
}

void
soshutdown ( struct socket *so )
{
	sorflush(so);
        // (void) tcp_usrreq (so, PRU_SHUTDOWN,
        //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	proto_shutdown ( so );

#ifdef notdef
	struct protosw *pr = so->so_proto;
	how++;
	if (how & FREAD)
		sorflush(so);
	if (how & FWRITE)
		return ((*pr->pr_usrreq)(so, PRU_SHUTDOWN,
		    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0));
	return (0);
#endif
}


/* Here is tcp_recv() -- what I hope to be the last addition
 * to TCP for kyu.
 *
 * Under BSD (and linux) the syscalls are:
 * 
 *  recv ( so, buf, len, flags ) == read() with flags = 0
 *  recvfrom ( so, buf, len, flags, NULL, 0 ) == recv()
 *  recvmsg ( so, msghdr, flags ) -- scatter/gather with iovec
 *
 *  supporting code is in kern/uipc_syscalls.c
 *  in particular recvit(), which calls soreceive()
 *  this is in kern/uipc_socket.c
 */

/* Prototype */
int
soreceive (
	struct socket *,
	struct mbuf **,
	struct uio *,
	struct mbuf **,
	struct mbuf **,
	int *
    );

int
tcp_recv ( struct socket *so, char *buf, int max )
{
	struct uio k_uio;
	struct iovec k_vec;
	int stat;
	int n;

	// bpf1 ( "kyu_recv - max = %d bytes\n", max );

	k_vec.iov_base = buf;
	k_vec.iov_len = max;

	k_uio.uio_iov = &k_vec;
        k_uio.uio_iovcnt = 1;

	// obsolete now given xiomove
        // k_uio.uio_segflg = UIO_USERSPACE;
        // k_uio.uio_rw = UIO_READ;

        k_uio.uio_offset = 0;
        k_uio.uio_resid = max;

	/* soo_read() does this:
	return (soreceive((struct socket *)fp->f_data, (struct mbuf **)0,
                uio, (struct mbuf **)0, (struct mbuf **)0, (int *)0));
		*/

	stat = soreceive ( so, (struct mbuf **)0, &k_uio,
	    (struct mbuf **)0, (struct mbuf **)0, (int *)0 );

	/* Typical output on two successive calls.
	 * the first had 26 bytes delivered
	 *  kyu_recv - iov = 0, 102 102, 26
	 *  kyu_recv - base = 40567f6e
	 *  kyu_recv - max = 128 bytes
	 *  kyu_recv - iov = 0, 128 128, 0
	 */

	// bpf1 ( "kyu_recv - iov = %d, %d %d, %d\n", stat, k_vec.iov_len, k_uio.uio_resid, k_uio.uio_offset );
	// bpf1 ( "kyu_recv - base = %08x\n", k_vec.iov_base );

	n = k_uio.uio_offset;

	return n;
}

#define	sosendallatonce(so) \
   ((so)->so_proto_flags & PR_ATOMIC)

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */

//soreceive(so, paddr, uio, mp0, controlp, flagsp)

int
soreceive (
	struct socket *so,
	struct mbuf **paddr,	// generally 0
	struct uio *uio,
	// The following are generally all 0
	struct mbuf **mp0, struct mbuf **controlp, int *flagsp )
{
	struct mbuf *m, **mp;
	int flags, len, error, s, offset;
	struct mbuf *nextrecord;
	int moff, type;
	int orig_resid = uio->uio_resid;

	struct protosw *pr = so->so_proto;

	mp = mp0;
	if (paddr)
		*paddr = 0;
	if (controlp)
		*controlp = 0;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if (flags & MSG_OOB) {
		// m = m_get(M_WAIT, MT_DATA);
		m = (struct mbuf *) mb_get ( MT_DATA );

		// error = (*pr->pr_usrreq)(so, PRU_RCVOOB,
		//     m, (struct mbuf *)(flags & MSG_PEEK), (struct mbuf *)0);
		// error = tcp_usrreq (so, PRU_RCVOOB,
		//     m, (struct mbuf *)(flags & MSG_PEEK), (struct mbuf *)0);
		error = proto_recvoob ( so, m, flags & MSG_PEEK );

		if (error)
			goto bad;
		do {
			// error = uiomove(mtod(m, caddr_t),
			//     (int) min(uio->uio_resid, m->m_len), uio);
			error = xiomove ( mtod(m, caddr_t),
			    (int) min(uio->uio_resid, m->m_len), uio, 1 );

			// m = m_free(m);
			m = mb_free(m);
		} while (uio->uio_resid && error == 0 && m);

bad:
		if (m)
			mb_freem(m);
			// m_freem(m);
		return (error);
	}

	if (mp)
		*mp = (struct mbuf *)0;

	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		proto_rcvd ( so );
		// (void) tcp_usrreq (so, PRU_RCVD,
		//     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
		// (*pr->pr_usrreq)(so, PRU_RCVD,
		// (struct mbuf *)0,
		//     (struct mbuf *)0, (struct mbuf *)0);

restart:
	if (error = sblock(&so->so_rcv, SBLOCKWAIT(flags)))
		return (error);
	s = splnet();

	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark, or
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat).
	 *   3. MSG_DONTWAIT is not set
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == 0 || ((flags & MSG_DONTWAIT) == 0 &&
	    so->so_rcv.sb_cc < uio->uio_resid) &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == 0 && (so->so_proto_flags & PR_ATOMIC) == 0) {
#ifdef DIAGNOSTIC
		if (m == 0 && so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			if (m)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				goto dontblock;
			else
				goto release;
		}
		for (; m; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}

dontblock:

	// if (uio->uio_procp)
	// 	uio->uio_procp->p_stats->p_ru.ru_msgrcv++;

	nextrecord = m->m_nextpkt;
	if (so->so_proto_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				// MFREE(m, so->so_rcv.sb_mb);
				// m = so->so_rcv.sb_mb;
				m = so->so_rcv.sb_mb = mb_free ( m );
			}
		}
	}

	while (m && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (controlp) {
				// if (pr->pr_domain->dom_externalize &&
				//     mtod(m, struct cmsghdr *)->cmsg_type ==
				//     SCM_RIGHTS)
				//    error = (*pr->pr_domain->dom_externalize)(m);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				// MFREE(m, so->so_rcv.sb_mb);
				// m = so->so_rcv.sb_mb;
				m = so->so_rcv.sb_mb = mb_free ( m );
			}
		}
		if (controlp) {
			orig_resid = 0;
			controlp = &(*controlp)->m_next;
		}
	}

	if (m) {
		if ((flags & MSG_PEEK) == 0)
			m->m_nextpkt = nextrecord;
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	}

	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == 0) {
			splx(s);
			// error = uiomove(mtod(m, caddr_t) + moff, (int)len, uio);
			error = xiomove ( mtod(m, caddr_t) + moff, (int)len, uio, 1);
			s = splnet();
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = (struct mbuf *)0;
				} else {
					// MFREE(m, so->so_rcv.sb_mb);
					// m = so->so_rcv.sb_mb;
					m = so->so_rcv.sb_mb = mb_free ( m );
				}
				if (m)
					m->m_nextpkt = nextrecord;
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == 0 && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				splx(s);
				return (0);
			}
			if (m = so->so_rcv.sb_mb)
				nextrecord = m->m_nextpkt;
		}
	}

	if (m && so->so_proto_flags & PR_ATOMIC) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		if (so->so_proto_flags & PR_WANTRCVD && so->so_pcb)
		    proto_rcvd ( so );

		    // (void) tcp_usrreq (so, PRU_RCVD,
		    // 	(struct mbuf *)0, (struct mbuf *)flags, (struct mbuf *)0 );
		    // XXX note extra arg in the following
		    // RCVD ignores the flags argument anyhow for TCP
		    // (*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
		    //	 (struct mbuf *)flags, (struct mbuf *)0, (struct mbuf *)0);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}
		
	if (flagsp)
		*flagsp |= flags;

release:
	sbunlock(&so->so_rcv);
	splx(s);
	return (error);
}

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

	// bpf1 ( "kyu_send - sending %d bytes: %s\n", len, buf );

	k_vec.iov_base = buf;
	k_vec.iov_len = len;

	k_uio.uio_iov = &k_vec;
        k_uio.uio_iovcnt = 1;

	// obsolete now given xiomove
        // k_uio.uio_segflg = UIO_USERSPACE;
        // k_uio.uio_rw = UIO_WRITE;

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

	bpf3 ( "sosend, resid = %d\n", resid );
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
	//     (so->so_proto_flags & PR_ATOMIC);

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
			if (so->so_proto_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}

		space = sbspace(&so->so_snd);
		bpf3 ( "sosend, space = %d\n", space );

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
			bpf3 ( "sosend, space, resid = %d, %d\n", space, resid );

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
			// bpf3 ( "Calling xiomove for %d\n", len );

			error = xiomove(mtod(m, caddr_t), (int)len, uio, 0 );

			// p = mtod(m, caddr_t);
			// bpf3 ( "After xiomove: %s\n", p );

			resid = uio->uio_resid;
			m->m_len = len;
			*mp = m;

			// p = mtod(top, caddr_t);
			// bpf3 ( "top 1: %s\n", p );
			// bpf3 ( "top 2: %s\n", top->m_data );

			top->m_pkthdr.len += len;
			// bpf3 ( "top 1x: %s\n", p );
			// bpf3 ( "top 2x: %s\n", top->m_data );
			if (error)
				goto release;

			mp = &m->m_next;
			bpf3 ( "-- resid = %d\n", resid );
			if (resid <= 0) {
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
				break;
			}
		    } while (space > 0 && atomic);

		    // if (dontroute)
		    // 	    so->so_options |= SO_DONTROUTE;

		    // p = mtod(top, caddr_t);
		    // bpf3 ( "top 1a: %s\n", p );
		    // bpf3 ( "top 2a: %s\n", top->m_data );

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
			bpf3 ( "sosend: mb-data: (%d) %s\n", top->m_len, top->m_data );
		    else
			bpf3 ( "sosend: mb-len: %d\n", top->m_len );

		    sbappend(&so->so_snd, top);
		    error = tcp_output(tp);

		    // bpf3 ( "sosend: append: %d\n", error );
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
	    bpf3 ( "sosend - FINISH: %d\n", error );
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
		// bpf3 ( "xiomove: %d\n", cnt );
		// bpf3 ( "xiomove: len = %d\n", iov->iov_len );
		// bpf3 ( "xiomove: buf = %s\n", iov->iov_base );
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

	bpf3 ( "sonewconn - %08x %d\n", head, connstatus );

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
        // error = tcp_usrreq (so, PRU_ATTACH,
        //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
        error = proto_attach (so );

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
        // struct protosw *pr = so->so_proto;
        int s;
        struct sockbuf asb;

        sb->sb_flags |= SB_NOINTR;

        (void) sblock(sb, M_WAITOK);

        s = splimp();
        socantrcvmore(so);
        sbunlock(sb);

        asb = *sb;
        bzero((caddr_t)sb, sizeof (*sb));
        splx(s);

        // if (so->so_proto_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
        //         (*pr->pr_domain->dom_dispose)(asb.sb_mb);

        sbrelease(&asb);
}

void
sofree ( struct socket *so )
{
	printf ( "sofree called: %08x\n", so );

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
	printf ( "k_sock_free called: %08x\n", so );
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

		bpf3 ( "soisconnected - wakeup head %08x\n", head->kyu_sem );
                // wakeup((caddr_t)&head->so_timeo);
		sem_unblock ( head->kyu_sem );

        } else {
		bpf3 ( "soisconnected - wakeup sock\n" );
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
	// OK, but who would be waiting for this?
	sem_unblock ( so->kyu_sem );

        sowwakeup(so);
        sorwakeup(so);
}

/* Kyu hack -- 12/5/2022
 * (it doesn't work)
 * I see the state set to 0x03 when the socket is OK.
 * When the peer closes, I see the state go to 0x23
 * (the can't receive more bit is set)
 */
int
is_socket_connected ( struct socket *so )
{
	return ( so->so_state & SS_ISCONNECTED );
}

int
is_socket_receiving ( struct socket *so )
{
	return ( ! (so->so_state & SS_CANTRCVMORE) );
}

#ifdef notdef
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */
#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x4
#endif

int
get_socket_state ( struct socket *so )
{
	return ( so->so_state );
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

/* =================================================================================================== */
/* =================================================================================================== */
/* =================================================================================================== */
/* =================================================================================================== */

/* Put the following into a better more logical order and merge with the above.
 */

/*
 * Must be called at splnet...
 * called from tcp_input ();
 */
void
soabort ( struct socket *so )
{
	// (void) tcp_usrreq (so, PRU_ABORT,
        //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	proto_abort ( so );
}

/* From kern/uipc_socket2.c ---
 * they clearly don't belong here, but ...
 */
/* strings for sleep message: */
char    netio[] = "netio";
char    netcon[] = "netcon";
char    netcls[] = "netcls";

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 *
 * We get tempted for Kyu to call this tcp_close(), but that
 *  name is already taken, with different semantics.
 */
 /* from kern/uipc_socket.c */
int
soclose ( struct socket *so )
{
        int s = splnet();               /* conservative */
        int error = 0;

	bpf3 ( "soclose called with %08x\n", so );

	if ( ! so )
	    return 0;

        if (so->so_options & SO_ACCEPTCONN) {
                while (so->so_q0)
                        soabort(so->so_q0);
                while (so->so_q)
                        soabort(so->so_q);
        }

        if (so->so_pcb == 0)
                goto discard;

        if (so->so_state & SS_ISCONNECTED) {
                if ((so->so_state & SS_ISDISCONNECTING) == 0) {
                        error = sodisconnect(so);
                        if (error)
                                goto drop;
                }
                if (so->so_options & SO_LINGER) {
                        if ((so->so_state & SS_ISDISCONNECTING) &&
                            (so->so_state & SS_NBIO))
                                goto drop;
                        while (so->so_state & SS_ISCONNECTED)
			    bpf3 ( "block in close: %08x\n", so->kyu_sem );
			    sem_block ( so->kyu_sem );
                                //if (error = tsleep((caddr_t)&so->so_timeo,
                                //    PSOCK | PCATCH, netcls, so->so_linger))
                                //        break;
                }
        }

drop:
        if (so->so_pcb) {
                // int error2 = (*so->so_proto->pr_usrreq)(so, PRU_DETACH,
                //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
		// int error2 = tcp_usrreq (so, PRU_DETACH,
                //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

		int error2 = proto_detach ( so );

                if (error == 0)
                        error = error2;
        }

discard:
	// Near as I can tell, this marks the socket as not
	// having a reference in file array (i.e. as an "fd")
	// In Kyu this bit is always set.
        // if (so->so_state & SS_NOFDREF)
        //         panic("soclose: NOFDREF");
        so->so_state |= SS_NOFDREF;

        sofree(so);
        splx(s);

        return (error);
}

int
sodisconnect ( struct socket *so )
{
        int s = splnet();
        int error;

        if ((so->so_state & SS_ISCONNECTED) == 0) {
                error = ENOTCONN;
                goto bad;
        }

        if (so->so_state & SS_ISDISCONNECTING) {
                error = EALREADY;
                goto bad;
        }

        // error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT,
        //     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	// error = tcp_usrreq (so, PRU_DISCONNECT,
        //     (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	error = proto_disconnect ( so );

bad:
        splx(s);
        return (error);
}

int
sockargs ( struct mbuf **mp, caddr_t buf, int buflen, int type)
{
        struct sockaddr *sa;
        struct mbuf *m;
        int error;

        if ( (u_int)buflen > MLEN ) {
                return (EINVAL);
        }

        // m = m_get(M_WAIT, type);
        m = (struct mbuf *) mb_get(type);
        if (m == NULL)
                return (ENOBUFS);

        m->m_len = buflen;
        bcopy ( buf, mtod(m, caddr_t), (u_int)buflen);
        // error = copyin(buf, mtod(m, caddr_t), (u_int)buflen);
        // if (error) {
        //      (void) mb_free(m);
	//	return error;
	//	}
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);
		sa->sa_len = buflen;
	}
        return 0;
}

// extern struct protosw tcp_proto;

#ifdef notdef
// We shrank it down to this.
struct protosw tcp_proto =
{
    PR_CONNREQUIRED|PR_WANTRCVD,	// flags
};
#endif

/* The original is in kern/uipc_socket.c
 */
int
socreate ( struct socket *so, int dom, int type, int proto)
// socreate ( int dom, struct socket **aso, int type, int proto)
{
        // struct proc *p = curproc;               /* XXX */
        // register struct protosw *prp;
        // register struct socket *so;
        register int error;

	// prp = &tcp_proto;
        // if (proto)
        //         prp = pffindproto(dom, proto, type);
        // else
        //         prp = pffindtype(dom, type);
	// 
        // if (prp == 0 || prp->pr_usrreq == 0)
        //	return (EPROTONOSUPPORT);
        // if (prp->pr_type != type)
	//	return (EPROTOTYPE);

        // MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_WAIT);
        bzero((caddr_t)so, sizeof(*so));

	so->kyu_sem = sem_signal_new ( SEM_FIFO );
	if ( ! so->kyu_sem ) {
	    panic ( "socreate - semaphores all gone" );
	}

        so->so_type = type;	/* SOCK_STREAM */
        //so->so_proto = prp;
	so->so_proto_flags = PR_CONNREQUIRED | PR_WANTRCVD;

        // if (p->p_ucred->cr_uid == 0)
        //         so->so_state = SS_PRIV;

        // error = (*prp->pr_usrreq)(so, PRU_ATTACH,
        //         (struct mbuf *)0, (struct mbuf *)proto, (struct mbuf *)0);

        // error = tcp_usrreq (so, PRU_ATTACH,
        //         (struct mbuf *)0, (struct mbuf *)proto, (struct mbuf *)0);

        error = proto_attach (so );

        if (error) {
                so->so_state |= SS_NOFDREF;
                sofree(so);
                return 1;
        }
        // *aso = so;
        return (0);
}

int
sobind ( struct socket *so, struct mbuf *nam )
{
        // int s = splnet();
        int error;

        // error = (*so->so_proto->pr_usrreq)(so, PRU_BIND,
        //         (struct mbuf *)0, nam, (struct mbuf *)0);
        // error = tcp_usrreq (so, PRU_BIND,
        //         (struct mbuf *)0, nam, (struct mbuf *)0);

	error = proto_bind ( so, nam );

        // splx(s);
        return (error);
}

int
solisten ( struct socket *so, int backlog )
{
        int s = splnet();
	int error;

        // error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN,
        //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
        // error = tcp_usrreq (so, PRU_LISTEN,
        //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

	error = proto_listen ( so );

        if (error) {
                splx(s);
                return (error);
        }

        if (so->so_q == 0)
                so->so_options |= SO_ACCEPTCONN;

        if (backlog < 0)
                backlog = 0;
        so->so_qlimit = min(backlog, SOMAXCONN);

        splx(s);
        return (0);
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 */
/* From netinet/ip_input.c */
void
ip_stripoptions ( struct mbuf *m )
{
        int i;
        struct ip *ip = mtod(m, struct ip *);
        caddr_t opts;
        int olen;

        olen = (ip->ip_hl<<2) - sizeof (struct ip);
        opts = (caddr_t)(ip + 1);
        i = m->m_len - (sizeof (struct ip) + olen);
        bcopy(opts  + olen, opts, (unsigned)i);
        m->m_len -= olen;
        if (m->m_flags & M_PKTHDR)
                m->m_pkthdr.len -= olen;
        ip->ip_hl = sizeof(struct ip) >> 2;
}

/* THE END */

// THE END
