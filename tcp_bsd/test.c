/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

#include <bsd.h>

// gets us htonl and such
#include <kyu.h>
#include <thread.h>

// #include <kyu_compat.h>

#ifdef notdef
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
#endif

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

void socantrcvmore ( struct socket * );
void socantsendmore ( struct socket * );

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
	    char *msg = "Dead men tell no tales\r\n";
	    cso = kyu_accept ( so );
	    printf ( "kyu_accept got a connection %08x\n", cso );
	    kyu_send ( cso, msg, strlen(msg) );
	    soclose ( cso );
	    printf ( "socket was closed\n" );
	}
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
	// Near as I can tell, this marks the socked as not
	// having a reference in file array (i.e. as an "fd")
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
	error = tcp_usrreq (so, PRU_DISCONNECT,
            (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

bad:
        splx(s);
        return (error);
}


/* Needs improvement */
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

/* We do an abbreviated form of the accept call.
 * We don't need to translate fd to sock and vice versa
 * We also don't need to fuss around copying address
 * information from kernel to user space.
 */
struct socket *
kyu_accept ( struct socket *so )
{
	struct socket *rso;

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
		printf ( "block in accept: %08x\n", so->kyu_sem );
		sem_block ( so->kyu_sem );
        }

        if (so->so_error) {
                // error = so->so_error;
                so->so_error = 0;
                // return (error);
                return NULL;
        }

	/* Pull the socket we are accepting off the queue.
	 */
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
