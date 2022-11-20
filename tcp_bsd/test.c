/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

#include <bsd.h>

// gets us htonl and such
#include <kyu.h>
#include <thread.h>

extern struct protosw tcp_proto;

static void bind_test ( int );
void tcb_show ( void );

struct socket *tcp_bind ( int );
struct socket *tcp_accept ( struct socket * );
struct socket *tcp_connect ( char *, int );

struct mbuf *mb_free ( struct mbuf * );

/* Can these all be static ?? */
void sofree ( struct socket * );
void sbflush ( struct sockbuf * );
void sbdrop ( struct sockbuf *, int );

void socantrcvmore ( struct socket * );
void socantsendmore ( struct socket * );

void bsd_server_test ( void );
void bsd_client_test ( void );

/* Called from user.c at the end of Kyu initialization.
 */
void
tcp_test_hook ( void )
{
	printf ( "TCP test hook\n" );

	bsd_server_test ();

	thr_delay ( 3000 );

	bsd_client_test ();
}

/* ----------------------------- */
/* ----------------------------- */

void
client_thread ( long xxx )
{
	struct socket *so;

	printf ( "Start connect to %s (%d)\n", "192.168.0.5", 13 );
	so = tcp_connect ( "192.168.0.5", 13 );
	printf ( "Connect returns: %08x\n", so );
	(void) soclose ( so );
}

void
bsd_client_test ( void )
{
	(void) safe_thr_new ( "tcp-client", client_thread, (void *) 0, 15, 0 );
}

void
server_thread ( long xxx )
{
	bind_test ( 111 );
}

/* currently called during initialization.
 */
void
bsd_server_test ( void )
{
	(void) safe_thr_new ( "tcp-server", server_thread, (void *) 0, 15, 0 );
}

static void
bind_test ( int port )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( port );

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
	    cso = tcp_accept ( so );

	    fip = tcp_getpeer_ip ( cso );
	    // printf ( "kyu_accept got a connection: %08x\n", cso );
	    printf ( "kyu_accept got a connection from: %s\n", ip2str32_h(fip) );
	    // tcb_show ();

	    tcp_send ( cso, msg, strlen(msg) );
	    (void) soclose ( cso );
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

	printf ( "soclose called with %08x\n", so );

	if ( ! so )
	    return 0;

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
			    printf ( "block in close: %08x\n", so->kyu_sem );
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
		int error2 = tcp_usrreq (so, PRU_DETACH,
                        (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);

                if (error == 0)
                        error = error2;
        }

discard:
	// Near as I can tell, this marks the socket as not
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
