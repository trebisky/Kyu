/* kyu_bsd.c */

/* support code for BSD 4.4 TCP code in Kyu
 * BSD global variables are here.
 */

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

#include <sys/param.h>
// #include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
// #include <sys/protosw.h>
// #include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <in.h>
#include <in_systm.h>
#include <ip.h>
#include <in_pcb.h>
#include <ip_var.h>

#include <tcp.h>
#include <tcp_seq.h>
#include <tcp_timer.h>
#include <tcpip.h>
#include <tcp_var.h>
#include <tcp_debug.h>

struct	mbuf *m_devget ( char *, int, int, struct ifnet *, void (*)() );

/* global variables */

int mbtypes[] = {                               /* from mbuf.h */
        M_FREE,         /* MT_FREE      0          should be on free list */
        M_MBUF,         /* MT_DATA      1          dynamic (data) allocation */
        M_MBUF,         /* MT_HEADER    2          packet header */
        M_SOCKET,       /* MT_SOCKET    3          socket structure */
        M_PCB,          /* MT_PCB       4          protocol control block */
        M_RTABLE,       /* MT_RTABLE    5          routing tables */
        M_HTABLE,       /* MT_HTABLE    6          IMP host tables */
        0,              /* MT_ATABLE    7          address resolution tables */
        M_MBUF,         /* MT_SONAME    8          socket name */
        0,              /*              9 */
        M_SOOPTS,       /* MT_SOOPTS    10         socket options */
        M_FTABLE,       /* MT_FTABLE    11         fragment reassembly header */
        M_MBUF,         /* MT_RIGHTS    12         access rights */
        M_IFADDR,       /* MT_IFADDR    13         interface address */
        M_MBUF,         /* MT_CONTROL   14         extra-data protocol message */
        M_MBUF,         /* MT_OOBDATA   15         expedited data  */
#ifdef DATAKIT
        25, 26, 27, 28, 29, 30, 31, 32          /* datakit ugliness */
#endif
};

struct  mbstat mbstat;		/* mbuf.h */
union   mcluster *mclfree;	/* mbuf.h */
int max_linkhdr;		/* mbuf.h */
int max_protohdr;		/* mbuf.h */
int max_hdr;			/* mbuf.h */
int max_datalen;		/* mbuf.h */

u_long	sb_max;			/* socketvar.h */

struct  ifnet   *ifnet;		/* net/if.h */

/* from net/route.h */
struct	route_cb route_cb;
struct	rtstat	rtstat;
// struct	radix_node_head *rt_tables[AF_MAX+1];

/* from ip_var.h */
struct	ipstat	ipstat;
struct	ipq	ipq;			/* ip reass. queue */
u_short	ip_id;				/* ip packet ctr, for ids */
int	ip_defttl;			/* default IP ttl */

/* from tcp_seq.h, tcp.h */
tcp_seq	tcp_iss;		/* tcp initial send seq # */

/* from tcp_var.h */
struct	inpcb tcb;		/* head of queue of active tcpcb's */
struct	tcpstat tcpstat;	/* tcp statistics */
u_long	tcp_now;		/* for RFC 1323 timestamps */

/* from tcp_debug.h */
struct	tcp_debug tcp_debug[TCP_NDEBUG];
int	tcp_debx;

/* Handy address full of all zeros */
struct in_addr zeroin_addr = { 0 };

/* From netinet/ip_input.c
 * this oddity bears some looking at -- gone in NetBSD-1.3
 */
struct  in_ifaddr *in_ifaddr;                   /* first inet address */

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

static void bsd_init ( void );
static void tcp_globals_init ( void );
static void tcp_bsd_rcv ( struct netbuf *nbp );

struct tcp_ops bsd_ops = {
	bsd_init,
	tcp_bsd_rcv
};


/* Kyu calls this during initialization */
struct tcp_ops *
tcp_bsd_init ( void )
{
	return &bsd_ops;
}

static void
bsd_init ( void )
{
	printf ( "BSD tcp init called\n" );
	tcp_globals_init ();

	// in tcp_subr.c
	tcp_init ();
}

static void
tcp_globals_init ( void )
{
	mclfree = NULL;

	/* From netiso/tuba_subr.c */
	#define TUBAHDRSIZE (3 /*LLC*/ + 9 /*CLNP Fixed*/ + 42 /*Addresses*/ \
	     + 6 /*CLNP Segment*/ + 20 /*TCP*/)

	/* From kern/uipc_domain.c */
	max_protohdr = TUBAHDRSIZE;
	max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	/* MHLEN is defined in mbuf.h */
	max_datalen = MHLEN - max_hdr;

	/* maximum chars per socket buffer, from socketvar.h */
	sb_max = SB_MAX;

	tcb.inp_next = tcb.inp_prev = &tcb;

	// ifnet = XXX
}

void
tcp_bsd_show ( void )
{
    printf ( "Status for BSD tcp code:\n" );
    // ...
}

/* This is where arriving packets get delivered by Kyu
 *  From ip_rcv() in net/net_ip.c
 * Be sure and let Kyu free the packet.
 */
static void
tcp_bsd_rcv ( struct netbuf *nbp )
{
	// struct netpacket *pkt;
	struct mbuf *m;
	struct ip *iip;

	/* byte swap fields in IP header */
	nbp->iptr->len = ntohs ( nbp->iptr->len );
	nbp->iptr->id = ntohs ( nbp->iptr->id );
	nbp->iptr->offset = ntohs ( nbp->iptr->offset );

	// printf ( "TCP: bsd_rcv %d bytes\n", ntohs(nbp->iptr->len) );
	printf ( "TCP: bsd_rcv %d, %d bytes\n", nbp->ilen, nbp->iptr->len );

	dump_buf ( (char *) nbp->iptr, nbp->ilen );

	m = m_devget ( (char *) nbp->iptr, nbp->ilen, 0, NULL, 0 );
	if ( ! m ) {
	    printf ( "** m_devget fails\n" );
	    return;
	}
	printf ( "M_devget: %08x %d\n", m, nbp->ilen );
	dump_buf ( (char *) m, 128 );

	/* Ah, but what goes on during IP processing by the BSD code */
	/* For one thing, it subtracts the size of the IP header
	 * off of the length field in the IP header itself.
	 */
	iip = mtod(m, struct ip *);
	iip->ip_len -= sizeof(struct ip);

	tcp_input ( m, nbp->ilen );

	/* do NOT free the netbuf here */

	/* Must give eptr since we may have PREPAD */
	// pkt = (struct netpacket *) nbp->eptr;
	// ip_ntoh ( pkt );
	// tcp_ntoh ( pkt );

	// tcp_in ( pkt );

}

/* Called when TCP wants to hand a packet to IP for transmission
 */
int
ip_output ( struct mbuf *, struct mbuf *, struct route *, int,  struct ip_moptions * )
{
	printf ( "TCP(bsd): ip_output\n" );
}

/* Called when TCP wants to send a control message.
 */
int
ip_ctloutput ( int i, struct socket *s, int j, int k, struct mbuf **mm )
{
	printf ( "TCP(bsd): ctl output\n" );
}

/* Something more clever could be done for these,
 * but this gets us going for now.
 */

void    bcopy (const void *from, void *to, u_int len)
{
	memcpy ( to, from, len );
}

void    bzero (void *buf, u_int len)
{
	memset ( buf, 0, len );
}

/* -------------------------------------------------------------------------------------------- */

/* Here is our mbuf allocation scheme.
 */

void * kyu_malloc ( unsigned long );

void *
k_mbuf_alloc ( void )
{
	void *rv;

	rv = kyu_malloc ( MSIZE );	/* 128 */
	printf ( "kyu_mbuf_alloc: %d %08x\n", MSIZE, rv );
	memset ( rv, 0xab, MSIZE );
	return rv;
}

/* XXX todo */
void
k_mbuf_free ( void *m )
{
}

void *
k_mbufcl_alloc ( void )
{
	void *rv;

	rv = kyu_malloc ( MCLBYTES );	/* 2048 */
	return rv;
}

// void k_mbufcl_free ( void *cl ) { }

/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */

/* This won't do of course, but keeps things quiet for now.
*/

int splnet ( void ) {}
int splimp ( void ) {}
int splx ( int arg ) {}

/* More stubs
 */

void sbflush ( void ) {}
void sbdrop ( void ) {}
void sbappend ( void ) {}
void sbreserve ( void ) {}

void soisdisconnected ( void ) {}
void soisdisconnecting ( void ) {}
void soisconnected ( void ) {}
void soisconnecting ( void ) {}
void socantsendmore ( void ) {}
void sohasoutofband ( void ) {}
void socantrcvmore ( void ) {}
void soabort ( void ) {}
void soreserve ( void ) {}
void sofree ( struct socket *so ) {}


struct  socket * sonewconn1 ( struct socket *head, int connstatus) {}

// -- in in_pcb.c
// int in_pcballoc ( struct socket *s, struct inpcb *i ) {}
// int in_pcbdetach ( struct inpcb * ) {}

// int in_setsockaddr ( struct inpcb *, struct mbuf * ) {}
// int in_setpeeraddr ( struct inpcb *, struct mbuf * ) {}

// void tcp_ctlinput ( int i, struct sockaddr *s, struct ip *x ) {}

// int in_pcbbind ( struct inpcb *n, struct mbuf *m ) {}
// int in_pcbconnect ( struct inpcb *n, struct mbuf *m ) {}
// int in_pcbdisconnect ( struct inpcb *n ) {}

// int in_pcbnotify ( struct inpcb *n, struct sockaddr *s, unsigned int, struct in_addr, unsigned int, int, void (*)() ) {}
// struct inpcb * in_pcblookup ( struct inpcb *, struct in_addr,  unsigned int,  struct in_addr,  unsigned int,  int) {}

// int in_losing ( struct inpcb *n ) {}

// -- in in.c
// int in_control ( struct socket *n, int x, caddr_t y, struct ifnet *f ) {}

void ip_pcblookup ( struct route * ) {}

void wakeup ( void ) {}
void sowakeup ( void ) {}
void inetctlerrmap ( void ) {}

void _insque ( void ) {}
void _remque ( void ) {}

// int in_localaddr ( struct in_addr ) {}

void rtalloc ( struct route * ) {}

void ip_stripoptions ( struct mbuf *, struct mbuf * ) {}

struct mbuf * ip_srcroute ( void ) {}

/* from net/if.c */
struct ifaddr * ifa_ifwithnet ( struct sockaddr *addr ) {}
struct ifaddr * ifa_ifwithaddr ( struct sockaddr *addr ) {}
struct ifaddr * ifa_ifwithdstaddr ( struct sockaddr *addr ) {}

/* from net/route.c */
void rtfree( struct rtentry *rt ) {}
int rtrequest( int req, struct sockaddr *dst, struct sockaddr *gateway, struct sockaddr *netmask, int flags, struct rtentry **ret_nrt ) {}

/* from net/rtsock.c */
void rt_missmsg( int type, struct rt_addrinfo *rtinfo, int flags, int error) {}

void ip_freemoptions( register struct ip_moptions * ) {}


/* THE END */
