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
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
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

void    bzero ( void *, u_int );
struct	mbuf *mb_devget ( char *, int, int, struct ifnet *, void (*)() );

/* global variables */

#ifdef notdef
/* For malloc/free */
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
};
#endif

struct  mbstat mbstat;		/* mbuf.h */
// union   mcluster *mclfree;
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
struct  in_ifaddr *in_ifaddr_head; /* first inet address */

/* Both of these from in_proto.c
 *  In the original there is an array "inetsw" of which the tcp_proto
 *  entry below is just one of many entries.
 */

/* From net/radix.c
 * -- something to do with routing.
 */
int
rn_inithead ( void **head, int off )
{
	panic ( "rn_inithead called" );
}


extern struct domain inetdomain;

struct protosw tcp_proto =
{ SOCK_STREAM,  &inetdomain,    IPPROTO_TCP,    PR_CONNREQUIRED|PR_WANTRCVD,
  tcp_input,    0,              tcp_ctlinput,   tcp_ctloutput,
  tcp_usrreq,
  tcp_init,     tcp_fasttimo,   tcp_slowtimo,   tcp_drain };

struct domain inetdomain =
    { AF_INET, "internet", 0, 0, 0,
      // inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
      &tcp_proto, &tcp_proto, 0,
      rn_inithead, 32, sizeof(struct sockaddr_in) };

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

	// in mbuf.c
	mb_init ();

	// in tcp_subr.c
	tcp_init ();

	bsd_server_test ();
}

static void
tcp_globals_init ( void )
{
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

	bzero ( (char *) &tcb, sizeof(struct inpcb) );
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

	/* list of configured interfaces */
	in_ifaddr_head = NULL;

	/* byte swap fields in IP header */
	nbp->iptr->len = ntohs ( nbp->iptr->len );
	nbp->iptr->id = ntohs ( nbp->iptr->id );
	nbp->iptr->offset = ntohs ( nbp->iptr->offset );

	// printf ( "TCP: bsd_rcv %d bytes\n", ntohs(nbp->iptr->len) );
	printf ( "TCP: bsd_rcv %d, %d bytes\n", nbp->ilen, nbp->iptr->len );

	dump_buf ( (char *) nbp->iptr, nbp->ilen );

	m = mb_devget ( (char *) nbp->iptr, nbp->ilen, 0, NULL, 0 );
	if ( ! m ) {
	    printf ( "** mb_devget fails\n" );
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

#ifdef notdef
void
udp_send ( u32 dest_ip, int sport, int dport, char *buf, int size )
{
        struct udp_hdr *udp;
        struct netbuf *nbp;
        struct bogus_ip *bip;

        nbp = netbuf_alloc ();
        if ( ! nbp )
            return;

        nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
        nbp->dptr = nbp->pptr + sizeof ( struct udp_hdr );

        memcpy ( nbp->dptr, buf, size );

        size += sizeof(struct udp_hdr);
        nbp->plen = size;
        nbp->ilen = size + sizeof(struct ip_hdr);

        udp = (struct udp_hdr *) nbp->pptr;
        udp->sport = htons(sport);
        udp->dport = htons(dport);
        udp->len = htons(nbp->plen);
        udp->sum = 0;

        /* Fill out a bogus IP header just to do
         * checksum computation.  It will pick up
         * the proto field from this however.
         */
        bip = (struct bogus_ip *) nbp->iptr;
        memset ( (char *) bip, 0, sizeof(struct bogus_ip) );

        bip->proto = IPPROTO_UDP;
        bip->len = udp->len;
        bip->dst = dest_ip;
        bip->src = host_info.my_ip;

        /*
        udp->sum = in_cksum ( nbp->iptr, nbp->ilen );
        */
        udp->sum = ~in_cksum_i ( nbp->iptr, nbp->ilen, 0 );

        ip_send ( nbp, dest_ip );
}
#endif


/* Called when TCP wants to hand a packet to IP for transmission
 */
int
ip_output ( struct mbuf *A, struct mbuf *B, struct route *R, int N,  struct ip_moptions *O )
{
        struct netbuf *nbp;
        struct ip_hdr *ipp;
	struct mbuf *mp;
        int len;
        int size = 0;
	char *buf;

	printf ( "TCP(bsd): ip_output\n" );
	printf ( " ip_output A = %08x\n", A );
	printf ( " ip_output B = %08x\n", B );
	printf ( " ip_output R = %08x\n", R );
	printf ( " ip_output N = %d\n", N );
	printf ( " ip_output O = %08x\n", O );
	mbuf_show ( A, "ip_output" );
	dump_buf ( (char *) A, 128 );

        nbp = netbuf_alloc ();
        if ( ! nbp )
            return 1;

	printf ( "IP output 1\n" );
	buf = (char *) nbp->iptr;
	for (mp = A; mp; mp = mp->m_next) {
                len = mp->m_len;
                if (len == 0)
                        continue;
                // bcopy ( mtod(mp, char *), buf, len );
                memcpy ( buf, mtod(mp, char *), len );
                buf += len;
                size += len;
        }
	printf ( "IP output 2\n" );

        mb_freem ( A );

	printf ( "IP output 3\n" );

        nbp->ilen = size;
        nbp->plen = size - sizeof(struct ip_hdr);

	/* BSD has given us a partially completed IP header.
	 * In particular, it contains src/dst IP numbers.
	 */

        nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );

	// not needed on output.
        // nbp->dptr = nbp->pptr + sizeof ( struct udp_hdr );

	ipp = nbp->iptr;

        ip_send ( nbp, ipp->dst );
	printf ( "IP output 4\n" );
	return 0;
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

/* This is taken from i386/i386/machdep.c and sys/proc.h
 */

struct  llist {
        struct  llist *f_link;
        struct  llist *r_link;
};

void
_insque ( struct llist *element, struct llist *head)
{
        element->f_link = head->f_link;
        head->f_link = element;
        element->r_link = head;
        (element->f_link)->r_link = element;
}

/*
 * remove an element from a queue
 */
void
_remque ( struct llist *element)
{
        (element->f_link)->r_link = element->r_link;
        (element->r_link)->f_link = element->f_link;
        // element->r_link = (struct llist *)0;
}


/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */

/* This won't do of course, but keeps things quiet for now.
*/

int splnet ( void ) {}
int splimp ( void ) {}
int splx ( int arg ) {}

#ifdef notdef
/* Doesn't work */
void malloc ( void )
{
	panic ( "malloc called\n" );
}

void free ( void )
{
	panic ( "free called\n" );
}
#endif


/* More stubs
 */

void sbappend ( void ) { panic ( "sbappend" ); }

void soisdisconnected ( void ) { panic ( "soisdisconncted" ); }
void soisdisconnecting ( void ) { panic ( "soisdisconncting" ); }
void soisconnected ( void ) { panic ( "soisconnected" ); }
void soisconnecting ( void ) { panic ( "soisconnecting" ); }
void socantsendmore ( void ) { panic ( "socantsendore" ); }
void sohasoutofband ( void ) { panic ( "sohasoutofband" ); }
void socantrcvmore ( void ) { panic ( "socantrcvmore" ); }
void soabort ( void ) { panic ( "soabort" ); }
void sorflush ( void ) { panic ( "sorflush" ); }

void soqremque ( void ) { panic ( "soqremque" ); }

struct  socket * sonewconn1 ( struct socket *head, int connstatus) { panic ( "sonewconn1" ); }

// -- in in_pcb.c
// int in_pcballoc ( struct socket *s, struct inpcb *i ) { panic ( "X" ); }
// int in_pcbdetach ( struct inpcb * ) { panic ( "X" ); }

// int in_setsockaddr ( struct inpcb *, struct mbuf * ) { panic ( "X" ); }
// int in_setpeeraddr ( struct inpcb *, struct mbuf * ) { panic ( "X" ); }

// void tcp_ctlinput ( int i, struct sockaddr *s, struct ip *x ) { panic ( "X" ); }

// int in_pcbbind ( struct inpcb *n, struct mbuf *m ) { panic ( "X" ); }
// int in_pcbconnect ( struct inpcb *n, struct mbuf *m ) { panic ( "X" ); }
// int in_pcbdisconnect ( struct inpcb *n ) { panic ( "X" ); }

// int in_pcbnotify ( struct inpcb *n, struct sockaddr *s, unsigned int, struct in_addr, unsigned int, int, void (*)() ) { panic ( "X" ); }
// struct inpcb * in_pcblookup ( struct inpcb *, struct in_addr,  unsigned int,  struct in_addr,  unsigned int,  int) { panic ( "X" ); }

// int in_losing ( struct inpcb *n ) { panic ( "X" ); }

// -- in in.c
// int in_control ( struct socket *n, int x, caddr_t y, struct ifnet *f ) { panic ( "X" ); }

void ip_pcblookup ( struct route * ) { panic ( "in_pcblookup" ); }

void wakeup ( void ) { panic ( "wakeup" ); }
void sowakeup ( void ) { panic ( "sowakeup" ); }
void inetctlerrmap ( void ) { panic ( "inetctlerrmap" ); }

// int in_localaddr ( struct in_addr ) { panic ( "X" ); }

void rtalloc ( struct route * ) { panic ( "rtalloc" ); }

// void ip_stripoptions ( struct mbuf *, struct mbuf * ) { panic ( "ip_stripoptions" ); }

struct mbuf * ip_srcroute ( void ) { panic ( "X" ); }

/* from net/if.c */
struct ifaddr * ifa_ifwithnet ( struct sockaddr *addr ) { panic ( "X" ); }
struct ifaddr * ifa_ifwithaddr ( struct sockaddr *addr ) { panic ( "X" ); }
struct ifaddr * ifa_ifwithdstaddr ( struct sockaddr *addr ) { panic ( "X" ); }

/* from net/route.c */
void rtfree( struct rtentry *rt ) { panic ( "X" ); }
int rtrequest( int req, struct sockaddr *dst, struct sockaddr *gateway, struct sockaddr *netmask, int flags, struct rtentry **ret_nrt ) { panic ( "X" ); }

/* from net/rtsock.c */
void rt_missmsg( int type, struct rt_addrinfo *rtinfo, int flags, int error) { panic ( "X" ); }

void ip_freemoptions( register struct ip_moptions * ) { panic ( "X" ); }

/* THE END */
