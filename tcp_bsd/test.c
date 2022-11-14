/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

// gets us htonl and such
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
#include "kyu.h"
#include "thread.h"
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
void tcb_show ( void );

/* currently called during initialization.
 */
void
bsd_server_test ( void )
{
	server_bind ( 111 );
}

static void
server_bind ( int port )
{
	struct socket *so;
	int e;
	struct mbuf *nam;
	struct sockaddr_in myaddr;
	int len;

	so = (struct socket *) k_sock_alloc ();
	// error = socreate(uap->domain, &so, uap->type, uap->protocol);
	e = socreate ( so, AF_INET, SOCK_STREAM, 0 );
	if ( e ) {
	    printf ( "socreate fails\n" );
	    return;
	}

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

	printf ( "--- Server bind OK\n" );
	tcb_show ();
}

void
tcb_show ( void )
{
	struct inpcb *inp;

	inp = &tcb;
	while ( inp->inp_next != &tcb ) {
	    inp = inp->inp_next;
	    printf ( "INPCB: lport: %d\n", inp->inp_lport );
	}
}

struct mbuf *mb_free ( struct mbuf * );

/* Can these all be static ?? */
void sbrelease ( struct sockbuf * );
void sofree ( struct socket * );
void sbflush ( struct sockbuf * );
void sbdrop ( struct sockbuf *, int );

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


/* ------------- */

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
