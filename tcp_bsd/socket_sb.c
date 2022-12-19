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
 */

/* sock_sb.c -- collect socket buffer routines here.
 */

#include <bsd.h>

#include <kyu.h>
#include <thread.h>

void sbrelease ( struct sockbuf * );
void sbdrop ( struct sockbuf *, int );
void sbcompress ( struct sockbuf *, struct mbuf *, struct mbuf * );
void sbappendrecord ( struct sockbuf *, struct mbuf * );

static int soreserve ( struct socket *, u_long, u_long );

/* sballoc() is a macro in sys/socketvar.h
 */

/* Size of socket buffers is given here.
 * This used to be in tcp_usrreq.c and referenced in
 * tcp_attach ()
 */
static u_long  tcp_sendspace = 1024*8;
static u_long  tcp_recvspace = 1024*8;

void
sb_init ( struct socket *so )
{
	struct sockbuf *sb;

	bpf3 ( "SB init called for %08x\n", so );

	if ( soreserve ( so, tcp_sendspace, tcp_recvspace ) )
	    bsd_panic ( "sb_init - cannot reserve space" );

	sb = &so->so_rcv;
	sb->sb_sleep = sem_signal_new ( SEM_FIFO );
        if ( ! sb->sb_sleep )
            bsd_panic ("Cannot get sb sleep semaphore");

	sb->sb_lock = sem_mutex_new ( SEM_FIFO );
        if ( ! sb->sb_lock )
            bsd_panic ("Cannot get sb lock semaphore");

	sb = &so->so_snd;
	sb->sb_sleep = sem_signal_new ( SEM_FIFO );
        if ( ! sb->sb_sleep )
            bsd_panic ("Cannot get sb sleep semaphore");

	sb->sb_lock = sem_mutex_new ( SEM_FIFO );
        if ( ! sb->sb_lock )
            bsd_panic ("Cannot get sb lock semaphore");

}

/* The original is in kern/uipc_socket2.c
 * This is the closest thing to sb_init()
 * that I can find.
 */
static int
soreserve ( struct socket *so, u_long sndcc, u_long rcvcc )
{

        if (sbreserve(&so->so_snd, sndcc) == 0)
                goto bad;
        if (sbreserve(&so->so_rcv, rcvcc) == 0)
                goto bad2;

        if (so->so_rcv.sb_lowat == 0)
                so->so_rcv.sb_lowat = 1;

        if (so->so_snd.sb_lowat == 0)
                so->so_snd.sb_lowat = MCLBYTES;

        if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
                so->so_snd.sb_lowat = so->so_snd.sb_hiwat;

        return (0);

bad2:
        sbrelease(&so->so_snd);
bad:
        return (ENOBUFS);
}

/*
 * Allot mbufs to a sockbuf.
 * Attempt to scale mbmax so that mbcnt doesn't become limiting
 * if buffering efficiency is near the normal case.
 */
/* The original is in kern/uipc_socket2.c */
int
sbreserve ( struct sockbuf *sb, u_long cc )
{

	// What the heck is this about ??
        // if (cc > sb_max * MCLBYTES / (MSIZE + MCLBYTES))
        //         return (0);

        sb->sb_hiwat = cc;
        sb->sb_mbmax = min(cc * 2, SB_MAX);

        if (sb->sb_lowat > sb->sb_hiwat)
                sb->sb_lowat = sb->sb_hiwat;

	// Always works these days
        return (1);
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush ( struct sockbuf *sb )
{
        if (sb->sb_flags & SB_LOCK)
                bsd_panic("sbflush");

        while (sb->sb_mbcnt)
                sbdrop(sb, (int)sb->sb_cc);

        if (sb->sb_cc || sb->sb_mb)
                bsd_panic("sbflush 2");
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease ( struct sockbuf *sb )
{
        sbflush(sb);
	sem_destroy ( sb->sb_lock );
	sem_destroy ( sb->sb_sleep );

        // sb->sb_hiwat = 0;
	sb->sb_mbmax = 0;
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop ( struct sockbuf *sb, int len)
{
        struct mbuf *m, *mn;
        struct mbuf *next;

        next = (m = sb->sb_mb) ? m->m_nextpkt : 0;

        while (len > 0) {
                if (m == 0) {
                        if (next == 0) {
			    printf ( "sb, m, len = %08x, %08x, %d\n", sb, m, len );
			    bsd_panic("sbdrop");
			}
                        m = next;
                        next = m->m_nextpkt;
                        continue;
                }
                if (m->m_len > len) {
                        m->m_len -= len;
                        m->m_data += len;
                        sb->sb_cc -= len;
                        break;
                }
                len -= m->m_len;
                sbfree(sb, m);
                // MFREE(m, mn);
                // m = mn;
		m = mb_free ( m );
        }
        while (m && m->m_len == 0) {
                sbfree(sb, m);
                // MFREE(m, mn);
                // m = mn;
		m = mb_free ( m );
        }
        if (m) {
                sb->sb_mb = m;
                m->m_nextpkt = next;
        } else
                sb->sb_mb = next;
}


/*
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 */
void
sbdroprecord ( struct sockbuf *sb )
{
        struct mbuf *m, *mn;

        m = sb->sb_mb;
        if (m) {
                sb->sb_mb = m->m_nextpkt;
                do {
                        sbfree(sb, m);
                        // MFREE(m, mn);
			mn = mb_free ( m );
                } while (m = mn);
        }
}

#ifdef notdef
-/*
- * Wakeup processes waiting on a socket buffer.
- * Do asynchronous notification via SIGIO
- * if the socket has the SS_ASYNC flag set.
- */
-void
-sowakeup ( struct socket *so, struct sockbuf *sb )
-{
-        // struct proc *p;
-
-        // selwakeup(&sb->sb_sel);
-        sb->sb_flags &= ~SB_SEL;
-        if (sb->sb_flags & SB_WAIT) {
-                sb->sb_flags &= ~SB_WAIT;
-                // wakeup((caddr_t)&sb->sb_cc);
-        }
-
-#ifdef signalSTUFF
-        if (so->so_state & SS_ASYNC) {
-                if (so->so_pgid < 0)
-                        gsignal(-so->so_pgid, SIGIO);
-                else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
-                        psignal(p, SIGIO);
-        }
-#endif
-}
#endif

/* This replaces the above for Kyu
 * It is only referenced in the macros sorwakeup and sowwakeup
 *  (in sys/socketvar.h)
 * We get rid of the "so" argument (only needed for signals).
 * And we are not trying to handle any select() behavior.
 */
void
sbwakeup ( struct sockbuf *sb )
{
	// printf ( "sbwakeup called\n" );
	sb->sb_flags &= ~SB_WAIT;
	sem_unblock ( sb->sb_sleep );

#ifdef notdef
	/* Why the test on SB_WAIT ? */
        // sb->sb_flags &= ~SB_SEL;
        if (sb->sb_flags & SB_WAIT) {
                sb->sb_flags &= ~SB_WAIT;
                // wakeup((caddr_t)&sb->sb_cc);
		sem_unblock ( sb->sb_sleep );
        }
#endif
}

/*
 * Wait for data to arrive at/drain from a socket buffer.
 */
int
sbwait ( struct sockbuf *sb )
{
        sb->sb_flags |= SB_WAIT;
	sem_block ( sb->sb_sleep );

        // return (tsleep((caddr_t)&sb->sb_cc,
        //     (sb->sb_flags & SB_NOINTR) ? PSOCK : PSOCK | PCATCH, netio, sb->sb_timeo));
	return 0;
}

#ifdef notdef
/*
 * Lock a sockbuf already known to be locked;
 * return any error returned from sleep (EINTR).
 */
static int
sb_lock ( struct sockbuf *sb )
{
        int error;

        while (sb->sb_flags & SB_LOCK) {
                sb->sb_flags |= SB_WANT;
                if (error = tsleep((caddr_t)&sb->sb_flags,
                    (sb->sb_flags & SB_NOINTR) ? PSOCK : PSOCK|PCATCH, netio, 0))
                        return (error);
        }

        sb->sb_flags |= SB_LOCK;
        return (0);
}

/* release lock on sockbuf sb */
void
sbunlock ( struct sockbuf *sb )
{
        sb->sb_flags &= ~SB_LOCK;
        if ( sb->sb_flags & SB_WANT) {
                sb->sb_flags &= ~SB_WANT;
                wakeup ((caddr_t)&sb->sb_flags);
        }
}

int
sblock ( struct sockbuf *sb, int wf )
{
	if ( sb->sb_flags & SB_LOCK )
	    if ((wf) == M_WAITOK)
		return sb_lock (sb);
	    else
		return EWOULDBLOCK;

	sb->sb_flags |= SB_LOCK;
	return 0;
}
#endif

/* Leave the details to Kyu semaphores */
void
sb_unlock ( struct sockbuf *sb )
{
	sem_unblock ( sb->sb_lock );
}

/* In lieu of sblock() above, not sb_lock() above !!
 * I'm not sure what the original sb_lock() was about,
 * but apparently it wasn't being used in the TCP code.
 */
int
sb_lock ( struct sockbuf *sb, int wf )
{
	sem_block ( sb->sb_lock );
	return 0;
}

#ifdef notdef
/* These were originally macros in sys/socketvar.h */
/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
#define sblock(sb, wf) ((sb)->sb_flags & SB_LOCK ? \
                (((wf) == M_WAITOK) ? sb_lock(sb) : EWOULDBLOCK) : \
                ((sb)->sb_flags |= SB_LOCK), 0)

/* release lock on sockbuf sb */
#define sbunlock(sb) { \
        (sb)->sb_flags &= ~SB_LOCK; \
        if ((sb)->sb_flags & SB_WANT) { \
                (sb)->sb_flags &= ~SB_WANT; \
                wakeup((caddr_t)&(sb)->sb_flags); \
        } \
}
#endif

/*
 * Routines to add and remove
 * data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to
 * append new mbufs to a socket buffer, after checking that adequate
 * space is available, comparing the function sbspace() with the amount
 * of data to be added.  sbappendrecord() differs from sbappend() in
 * that data supplied is treated as the beginning of a new record.
 * To place a sender's address, optional access rights, and data in a
 * socket receive buffer, sbappendaddr() should be used.  To place
 * access rights and data in a socket receive buffer, sbappendrights()
 * should be used.  In either case, the new data begins a new record.
 * Note that unlike sbappend() and sbappendrecord(), these routines check
 * for the caller that there will be enough space to store the data.
 * Each fails if there is not enough space, or if it cannot find mbufs
 * to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data
 * awaiting acknowledgement.  Data is normally copied from a socket
 * send buffer in a protocol with m_copy for output to a peer,
 * and then removing the data from the socket buffer with sbdrop()
 * or sbdroprecord() when the data is acknowledged by the peer.
 */
/*
 * Append mbuf chain m to the last record in the
 * socket buffer sb.  The additional space associated
 * the mbuf chain is recorded in sb.  Empty mbufs are
 * discarded and mbufs are compacted where possible.
 */
/* From kern/uipc_socket2.c */
void
sbappend (sb, m)
        struct sockbuf *sb;
        struct mbuf *m;
{
        struct mbuf *n;

        if (m == 0)
                return;

        if (n = sb->sb_mb) {
                while (n->m_nextpkt)
                        n = n->m_nextpkt;
                do {
                        if (n->m_flags & M_EOR) {
                                sbappendrecord(sb, m); /* XXX XXXXXX!!!! */
                                return;
                        }
                } while (n->m_next && (n = n->m_next));
        }
        sbcompress(sb, m, n);
}

/*
 * As above, except the mbuf chain
 * begins a new record.
 */
void
sbappendrecord ( struct sockbuf *sb, struct mbuf *m0 )
{
        register struct mbuf *m;

        if (m0 == 0)
                return;
        if (m = sb->sb_mb)
                while (m->m_nextpkt)
                        m = m->m_nextpkt;
        /*
         * Put the first mbuf on the queue.
         * Note this permits zero length records.
         */
        sballoc(sb, m0);
        if (m)
                m->m_nextpkt = m0;
        else
                sb->sb_mb = m0;
        m = m0->m_next;
        m0->m_next = 0;
        if (m && (m0->m_flags & M_EOR)) {
                m0->m_flags &= ~M_EOR;
                m->m_flags |= M_EOR;
        }
        sbcompress(sb, m, m0);
}

/*
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 */
void
sbcompress ( struct sockbuf *sb, struct mbuf *m, struct mbuf *n )
{
        register int eor = 0;
        register struct mbuf *o;

        while (m) {
                eor |= m->m_flags & M_EOR;
                if (m->m_len == 0 &&
                    (eor == 0 ||
                     (((o = m->m_next) || (o = n)) &&
                      o->m_type == m->m_type))) {
                        // m = m_free(m);
                        m = mb_free(m);
                        continue;
                }
                if (n && (n->m_flags & (M_EXT | M_EOR)) == 0 &&
                    (n->m_data + n->m_len + m->m_len) < &n->m_dat[MLEN] &&
                    n->m_type == m->m_type) {
                        bcopy(mtod(m, caddr_t), mtod(n, caddr_t) + n->m_len,
                            (unsigned)m->m_len);
                        n->m_len += m->m_len;
                        sb->sb_cc += m->m_len;
                        // m = m_free(m);
                        m = mb_free(m);
                        continue;
                }
                if (n)
                        n->m_next = m;
                else
                        sb->sb_mb = m;
                sballoc(sb, m);
                n = m;
                m->m_flags &= ~M_EOR;
                m = m->m_next;
                n->m_next = 0;
        }
        if (eor) {
                if (n)
                        n->m_flags |= eor;
                else
                        printf("semi-panic: sbcompress\n");
        }
}

// THE END
