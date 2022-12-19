/* kyu_api.c */

/* Top level interface for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-18-2022
 */

#include <bsd.h>

#include <kyu.h>
#include <thread.h>

struct socket * tcp_bind ( int );
struct socket * tcp_accept ( struct socket * );
static int kyu_soconnect ( struct socket *, struct mbuf * );

void
tcp_close ( struct socket *so )
{
	user_lock();
	(void) soclose ( so );	/* in socket_io.c */
	user_unlock();
}

/* Connect as a client */
static struct socket *
tcp_connect_int ( char *name, int port )
{
	unsigned long server_ip;
	struct socket *so;
	struct mbuf *nam;
	struct sockaddr_in myaddr;
	int len;
	int e;

	/* First we need a socket */
        so = (struct socket *) k_sock_alloc ();

        e = socreate ( so, AF_INET, SOCK_STREAM, 0 );
        if ( e ) {
            bpf2 ( "socreate fails for connect\n" );
            return NULL;
        }

	if ( ! net_dots ( name, &server_ip ) ) {
	    bpf2 ( "tcp_connect dots fails\n" );
            return NULL;
	}

	/* XXX */
	// return NULL;

        len = sizeof(myaddr);
        bzero ( &myaddr, len );
        myaddr.sin_family = AF_INET;

        /* oddly enough these go in network order */
	/* (and net_dots() returns network order) */
        // myaddr.sin_addr.s_addr = htonl(server_ip);
        myaddr.sin_addr.s_addr = server_ip;
        myaddr.sin_port = htons(port);

	/* This allocates an mbuf and copies the
	 * name information into it.
	 */
        e = sockargs(&nam, &myaddr, len, MT_SONAME);
        if ( e ) {
            bpf2 ( "connect - sockargs fails\n" );
            return NULL;
        }

	/* Code from kern/uipc_syscalls.c
	 */
        // e = soconnect(so, nam);
        e = kyu_soconnect(so, nam);
        if ( e ) {
	    so->so_state &= ~SS_ISCONNECTING;
	    mb_freem ( nam );
	    bpf2 ( "connect - soconnect fails %d\n", e );
	    return NULL;
	}

        // s = splnet();
        // while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
        //         if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH, netcon, 0))
        //                 break;

	// bpf2 ( "block in connect: %08x\n", so->kyu_sem );

        while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
	    sem_block ( so->kyu_sem );

        // splx(s);
        // so->so_state &= ~SS_ISCONNECTING;

        mb_freem ( nam );
	return so;
}

struct socket *
tcp_connect ( char *name, int port )
{
	struct socket *rv;

	user_lock ();
	rv = tcp_connect_int ( name, port );
	user_unlock ();
	return rv;
}

static int
kyu_soconnect ( struct socket *so, struct mbuf *nam)
{
        int s;
        int error;

	// bpf2 ( "kyu_soconnect 0\n" );

        if (so->so_options & SO_ACCEPTCONN)
	    return (EOPNOTSUPP);
        if ( so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) )
	    error = EISCONN;

	// bpf2 ( "kyu_soconnect 1\n" );

	// error = tcp_usrreq ( so, PRU_CONNECT,
	//     (struct mbuf *)0, nam, (struct mbuf *)0);
	error = proto_connect ( so, nam );

	// bpf2 ( "kyu_soconnect 2 %d\n", error );

#ifdef notdef
        // s = splnet();
        /*
         * If protocol is connection-based, can only connect once.
         * Otherwise, if connected, try to disconnect first.
         * This allows user to disconnect by connecting to, e.g.,
         * a null address.
         */
        //    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
        if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
            ((so->so_proto_flags & PR_CONNREQUIRED) ||
            (error = sodisconnect(so))))
                error = EISCONN;
        else
                error = (*so->so_proto->pr_usrreq)(so, PRU_CONNECT,
                    (struct mbuf *)0, nam, (struct mbuf *)0);
        // splx(s);
#endif

        return (error);
}


/* Returns peer IP in host order */
int
tcp_getpeer_ip ( struct socket *so )
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	//printf ( "get - inp - %08x\n", inp );
	//printf ( "get - f  - %08x\n", inp->inp_faddr.s_addr );
	//printf ( "get - ff - %08x\n", ntohl ( inp->inp_faddr.s_addr ) );
	return ntohl ( inp->inp_faddr.s_addr );
}

int
tcp_getpeer_port ( struct socket *so )
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	return ntohs ( inp->inp_fport );
}


static struct socket *
tcp_bind_int ( int port )
{
	int e;
	struct mbuf *nam;
	struct sockaddr_in myaddr;
	int len;
	struct socket *cso;
	struct socket *so;

	/* ------------- make a socket */
	so = (struct socket *) k_sock_alloc ();

	// error = socreate(uap->domain, &so, uap->type, uap->protocol);
	e = socreate ( so, AF_INET, SOCK_STREAM, 0 );
	if ( e ) {
	    printf ( "bind - socreate fails\n" );
	    return NULL;
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
	    printf ( "bind - sockargs fails\n" );
	    return NULL;
	}

	e = sobind ( so, nam );
	if ( e ) {
	    printf ( "sobind fails\n" );
	    // tcb_show ();
	    return NULL;
	}

	/* ------------- now do a listen */

	e = solisten ( so, 10 );
	if ( e ) {
	    printf ( "solisten fails\n" );
	    return NULL;
	}

	return so;
}

struct socket *
tcp_bind ( int port )
{
	struct socket *rv;

	user_lock ();
	rv = tcp_bind_int ( port );
	user_unlock ();
	return rv;
}

/* We do an abbreviated form of the accept call.
 * We don't need to translate fd to sock and vice versa
 * We also don't need to fuss around copying address
 * information from kernel to user space.
 */
static struct socket *
tcp_accept_int ( struct socket *so )
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
#ifdef BIG_LOCKS
		user_waiting ();
                // if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH, netcon, 0)) {
		// bpf2 ( "block in accept: %08x\n", so->kyu_sem );
		sem_block ( so->kyu_sem );
		user_lock ();
#else
		sem_block ( so->kyu_sem );
#endif
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
                bsd_panic("accept");

	// if we wanted the peer name, this call
	// would get it for us, albeit into an mbuf.

	// this is what PRU_ACCEPT would do ...
	// inp = sotoinpcb(rso);
	// in_setpeeraddr(inp, nam);

	return rso;
}

struct socket *
tcp_accept ( struct socket *so )
{
	struct socket *rv;

	user_lock ();
	rv = tcp_accept_int ( so );
	user_unlock ();

	return rv;
}

/* THE END */
