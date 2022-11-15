/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

// gets us htonl and such
#include <kyu.h>
#include <thread.h>
#include <kyu_compat.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/protosw.h>

// for stripoptions
#include <in_systm.h>
#include <in.h>
#include <ip.h>

// for inpcb
#include <net/route.h>
#include <ip_var.h>
#include <in_pcb.h>
#include <tcp.h>
#include <tcp_seq.h>
#include <tcp_timer.h>
#include <tcpip.h>
#include <tcp_var.h>

/* -------- */
#ifdef notdef
#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

#include <sys/param.h>
#include <sys/malloc.h>
// #include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <in_pcb.h>
#include <ip_var.h>

#include <tcp.h>
#include <tcp_seq.h>
#include <tcp_timer.h>
#include <tcpip.h>
#include <tcp_var.h>
#include <tcp_debug.h>
#endif

extern struct protosw tcp_proto;


static void server_bind ( int );
struct socket * kyu_accept ( struct socket * );
void tcb_show ( void );

struct mbuf *mb_free ( struct mbuf * );

/* Can these all be static ?? */
void sbrelease ( struct sockbuf * );
void sofree ( struct socket * );
void sbflush ( struct sockbuf * );
void sbdrop ( struct sockbuf *, int );

void
server_thread ( long xxx )
{
	server_bind ( 111 );
}

/* currently called during initialization.
 */
void
bsd_server_test ( void )
{
	(void) safe_thr_new ( "tcp-test", server_thread, (void *) 0, 15, 0 );
}

struct socket *global_so;

static void
server_bind ( int port )
{
	struct socket *so;
	int e;
	struct mbuf *nam;
	struct sockaddr_in myaddr;
	int len;
	struct socket *cso;

	register struct inpcb *inp;
        register struct tcpcb *tp;

	/* ------------- make a socket */
	so = (struct socket *) k_sock_alloc ();
	// error = socreate(uap->domain, &so, uap->type, uap->protocol);
	e = socreate ( so, AF_INET, SOCK_STREAM, 0 );
	if ( e ) {
	    printf ( "socreate fails\n" );
	    return;
	}

	/* ------------- now do a bind */
#ifdef notdef
	struct sockaddr_in serv_addr;
	bzero ( &serv_addr, sizeof(serv_addr) );
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl ( INADDR_ANY );
	serv_addr.sin_port = htons ( MY_TCP_PORT );
	bind ( sockfd, &serv_addr, sizeof(serv_addr) );
	e = sockargs(&nam, uap->name, uap->namelen, MT_SONAME);
#endif

	len = sizeof(myaddr);
	bzero ( &myaddr, len );
	myaddr.sin_family = AF_INET;

	/* oddly enough these go in network order */
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);	/* All zeros */
	myaddr.sin_port = htons(port);

	e = sockargs(&nam, &myaddr, len, MT_SONAME);
	if ( e ) {
	    printf ( "sockargs fails\n" );
	    return;
	}

	e = sobind ( so, nam );
	if ( e ) {
	    printf ( "sobind fails\n" );
	    tcb_show ();
	    return;
	}

	/* ------------- now do a listen */

	e = solisten ( so, 10 );
	if ( e ) {
	    printf ( "solisten fails\n" );
	    return;
	}

	/* XXX */
	global_so = so;

	// This is set by listen
	// so->so_options |= SO_ACCEPTCONN;

	/* ------------- finally do an accept */
	/* Accept is the most interesting perhaps.
	 * the unix call blocks and returns a new
	 * socket when a connection is made.
	 * The arguments to accept give a place where
	 * the address on the peer end of the connection
	 * can be returned.  This is ignored if both
	 * address and length are set to zero.
	 */
	printf ( "--- Server bind OK\n" );
	printf ( "--- waiting for connections\n" );
	// tcb_show ();

	for ( ;; ) {
	    cso = kyu_accept ( so );
	    printf ( "kyu_accept got a connection %08x\n", cso );
	}
}

void
test_connect ( void )
{
	printf ( " *** in TEST connect\n" );
	(void) soclose ( global_so );
}

/*
 * Must be called at splnet...
 */
int
soabort ( struct socket *so )
{
	int rv = tcp_usrreq (so, PRU_ABORT,
                (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	return rv;
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
 */
 /* from kern/uipc_socket.c */
int
soclose ( struct socket *so )
{
        int s = splnet();               /* conservative */
        int error = 0;

        if (so->so_options & SO_ACCEPTCONN) {
                while (so->so_q0)
                        (void) soabort(so->so_q0);
                while (so->so_q)
                        (void) soabort(so->so_q);
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
                                if (error = tsleep((caddr_t)&so->so_timeo,
                                    PSOCK | PCATCH, netcls, so->so_linger))
                                        break;
                }
        }

drop:
        if (so->so_pcb) {
                // int error2 = (*so->so_proto->pr_usrreq)(so, PRU_DETACH,
                //         (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
		int error2 = tcp_usrreq (so, PRU_DETACH,
                        (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

                if (error == 0)
                        error = error2;
        }

discard:
        if (so->so_state & SS_NOFDREF)
                panic("soclose: NOFDREF");
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
	error = tcp_usrreq (so, PRU_DISCONNECT,
            (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

bad:
        splx(s);
        return (error);
}


void
tcb_show ( void )
{
	struct inpcb *inp;

	inp = &tcb;
	while ( inp->inp_next != &tcb ) {
	    inp = inp->inp_next;
	    printf ( "INPCB: %08x -- lport: %d\n", inp, ntohs(inp->inp_lport) );
	}
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

static int
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

/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup ( struct socket *so, struct sockbuf *sb )
{
        // struct proc *p;

        // selwakeup(&sb->sb_sel);
        sb->sb_flags &= ~SB_SEL;
        if (sb->sb_flags & SB_WAIT) {
                sb->sb_flags &= ~SB_WAIT;
                // wakeup((caddr_t)&sb->sb_cc);
        }

#ifdef notdef
        if (so->so_state & SS_ASYNC) {
                if (so->so_pgid < 0)
                        gsignal(-so->so_pgid, SIGIO);
                else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
                        psignal(p, SIGIO);
        }
#endif
}

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, propoerly linked into the
 * data structure of the original socket, and return this.
 * Connstatus may be 0, or SO_ISCONFIRMING, or SO_ISCONNECTED.
 *
 * Currently, sonewconn() is defined as sonewconn1() in socketvar.h
 * to catch calls that are missing the (new) second parameter.
 */
/* From kern/uipc_socket2.c
 */
struct socket *
sonewconn1 ( struct socket *head, int connstatus)
{
        struct socket *so;
        int soqueue = connstatus ? 1 : 0;
	int error;

	printf ( "sonewconn1 - %08x %d\n", head, connstatus );

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

        (void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
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
                // sorwakeup(head);

                // wakeup((caddr_t)&head->so_timeo);
		sem_unblock ( so->kyu_sem );

                so->so_state |= connstatus;
        }

        return (so);
}

/* The original is in kern/uipc_socket.c
 */
int
socreate ( struct socket *so, int dom, int type, int proto)
// socreate ( int dom, struct socket **aso, int type, int proto)
{
        // struct proc *p = curproc;               /* XXX */
        register struct protosw *prp;
        // register struct socket *so;
        register int error;

	prp = &tcp_proto;
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
        so->so_proto = prp;

        // if (p->p_ucred->cr_uid == 0)
        //         so->so_state = SS_PRIV;

        // error = (*prp->pr_usrreq)(so, PRU_ATTACH,
        //         (struct mbuf *)0, (struct mbuf *)proto, (struct mbuf *)0);

        error = tcp_usrreq (so, PRU_ATTACH,
                (struct mbuf *)0, (struct mbuf *)proto, (struct mbuf *)0);

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
        error = tcp_usrreq (so, PRU_BIND,
                (struct mbuf *)0, nam, (struct mbuf *)0);

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
        error = tcp_usrreq (so, PRU_LISTEN,
                (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

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

/* XXX - tacky to have this a single global
 */
struct sem *accept_sem = NULL;

/* We do an abbreviated form of the accept call.
 * We don't need to translate fd to sock and vice versa
 * We also don't need to fuss around copying address
 * information from kernel to user space.
 */
struct socket *
kyu_accept ( struct socket *so )
{
	struct socket *rso;

	if ( ! accept_sem ) {
	    accept_sem = sem_signal_new ( SEM_FIFO );
	}
        if ( ! accept_sem )
            panic ("Cannot get accept semaphore");

        if ((so->so_options & SO_ACCEPTCONN) == 0) {
		printf ( "socket not ready to accept\n" );
                // return (EINVAL);
                return NULL;
        }

        while (so->so_qlen == 0 && so->so_error == 0) {
                if (so->so_state & SS_CANTRCVMORE) {
                        so->so_error = ECONNABORTED;
                        break;
                }
                // if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH, netcon, 0)) {
		sem_block ( accept_sem );
        }

        if (so->so_error) {
                // error = so->so_error;
                so->so_error = 0;
                // return (error);
                return NULL;
        }
        rso = so->so_q;
        if ( soqremque(rso, 1) == 0)
                panic("accept");

	// if we wanted the peer name, this call
	// would get it for us, albeit into an mbuf.
	// this is PRU_ACCEPT.
	// inp = sotoinpcb(rso);
	// in_setpeeraddr(inp, nam);

	return rso;
}

#ifdef notdef
/* From: kern/uipc_syscalls.c
 */
accept1(p, uap, retval)
        struct proc *p;
        register struct accept_args *uap;
        int *retval;
{
        struct file *fp;
        struct mbuf *nam;
        int namelen, error, s;
        register struct socket *so;

        if (uap->name && (error = copyin((caddr_t)uap->anamelen,
            (caddr_t)&namelen, sizeof (namelen))))
                return (error);

        if (error = getsock(p->p_fd, uap->s, &fp))
                return (error);

        s = splnet();
        so = (struct socket *)fp->f_data;

        if ((so->so_options & SO_ACCEPTCONN) == 0) {
                splx(s);
                return (EINVAL);
        }
        if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
                splx(s);
                return (EWOULDBLOCK);
        }

        while (so->so_qlen == 0 && so->so_error == 0) {
                if (so->so_state & SS_CANTRCVMORE) {
                        so->so_error = ECONNABORTED;
                        break;
                }
                if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH, netcon, 0)) {
                        splx(s);
                        return (error);
                }
        }

        if (so->so_error) {
                error = so->so_error;
                so->so_error = 0;
                splx(s);
                return (error);
        }

        if (error = falloc(p, &fp, retval)) {
                splx(s);
                return (error);
        }

        { struct socket *aso = so->so_q;
          if (soqremque(aso, 1) == 0)
                panic("accept");
          so = aso;
        }
	fp->f_type = DTYPE_SOCKET;
        fp->f_flag = FREAD|FWRITE;
        fp->f_ops = &socketops;
        fp->f_data = (caddr_t)so;

        nam = m_get(M_WAIT, MT_SONAME);

	// The soaccept call just does this,
	// fetching the peer address.
        // (void) soaccept(so, nam);
	// Note that so is now aso from the above.
	// the peer name from so rather than 
	inp = sotoinpcb(so);
	in_setpeeraddr(inp, nam);

        if (uap->name) {
#ifdef COMPAT_OLDSOCK
                if (uap->compat_43)
                        mtod(nam, struct osockaddr *)->sa_family =
                            mtod(nam, struct sockaddr *)->sa_family;
#endif
                if (namelen > nam->m_len)
                        namelen = nam->m_len;
                /* SHOULD COPY OUT A CHAIN HERE */
                if ((error = copyout(mtod(nam, caddr_t), (caddr_t)uap->name,
                    (u_int)namelen)) == 0)
                        error = copyout((caddr_t)&namelen,
                            (caddr_t)uap->anamelen, sizeof (*uap->anamelen));
        }

        m_freem(nam);
        splx(s);
        return (error);
}

int
soaccept ( struct socket *so, struct mbuf *nam)
{
        int s = splnet();
        int error;

        if ((so->so_state & SS_NOFDREF) == 0)
	    panic("soaccept: !NOFDREF");
        so->so_state &= ~SS_NOFDREF;

        // error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT,
        //    (struct mbuf *)0, nam, (struct mbuf *)0);
        error = tcp_usrreq (so, PRU_ACCEPT,
            (struct mbuf *)0, nam, (struct mbuf *)0);

        splx(s);
        return (error);
}
#endif

/* ------------- */

void
sorflush ( struct socket *so )
{
        struct sockbuf *sb = &so->so_rcv;
        struct protosw *pr = so->so_proto;
        int s;
        struct sockbuf asb;

	printf ( "Lazy sorflush\n" );
#ifdef notdef
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
#endif
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


/* The original is in kern/uipc_socket2.c
 */
int
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

        if (cc > sb_max * MCLBYTES / (MSIZE + MCLBYTES))
                return (0);

        sb->sb_hiwat = cc;
        sb->sb_mbmax = min(cc * 2, sb_max);
        if (sb->sb_lowat > sb->sb_hiwat)
                sb->sb_lowat = sb->sb_hiwat;
        return (1);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease ( struct sockbuf *sb )
{
        sbflush(sb);
        sb->sb_hiwat = sb->sb_mbmax = 0;
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush ( struct sockbuf *sb )
{
        if (sb->sb_flags & SB_LOCK)
                panic("sbflush");

        while (sb->sb_mbcnt)
                sbdrop(sb, (int)sb->sb_cc);

        if (sb->sb_cc || sb->sb_mb)
                panic("sbflush 2");
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
                        if (next == 0)
                                panic("sbdrop");
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
        register struct socket *so;
{
        register struct socket *head = so->so_head;

        so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING|SS_ISCONFIRMING);
        so->so_state |= SS_ISCONNECTED;
        if (head && soqremque(so, 0)) {
                soqinsque(head, so, 1);
                sorwakeup(head);

                // wakeup((caddr_t)&head->so_timeo);
		sem_unblock ( so->kyu_sem );

        } else {
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
