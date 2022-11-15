/* kyu_subs.c */

/* support code for BSD 4.4 TCP code in Kyu
 * mostly hackish cruft and stubs.
 * BSD global variables are here.
 */

#include "kyu.h"

#include "thread.h"
#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

// for ntohl and such
// #include "kyu_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
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

void
tcp_globals_init ( void )
{
        /* list of configured interfaces */
        in_ifaddr_head = NULL;

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

	bzero ( (void *) &tcb, sizeof(struct inpcb) );
	tcb.inp_next = tcb.inp_prev = &tcb;

	// ifnet = XXX
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

// void soisdisconnected ( void ) { panic ( "soisdisconncted" ); }
// void soisdisconnecting ( void ) { panic ( "soisdisconncting" ); }
// void soisconnected ( void ) { panic ( "soisconnected" ); }
// void soisconnecting ( void ) { panic ( "soisconnecting" ); }

void socantsendmore ( void ) { panic ( "socantsendore" ); }
void sohasoutofband ( void ) { panic ( "sohasoutofband" ); }
void socantrcvmore ( void ) { panic ( "socantrcvmore" ); }
// void soabort ( void ) { panic ( "soabort" ); }
// void sorflush ( void ) { panic ( "sorflush" ); }
// void sowakeup ( void ) { panic ( "sowakeup" ); }

void soqremque ( void ) { panic ( "soqremque" ); }

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

/* tsleep/wakeup are key kernel synch facilities in BSD
 * found in kern/kern_synch.c
 * For now, let them be noops.
 */
int tsleep ( void *ident, int priority, char *wmesg, int timo ) {}

// void wakeup ( void ) { panic ( "wakeup" ); }
void wakeup ( void ) { }

void inetctlerrmap ( void ) { panic ( "inetctlerrmap" ); }

// int in_localaddr ( struct in_addr ) { panic ( "X" ); }

// void rtalloc ( struct route * ) { panic ( "rtalloc" ); }

// void ip_stripoptions ( struct mbuf *, struct mbuf * ) { panic ( "ip_stripoptions" ); }

// struct mbuf * ip_srcroute ( void ) { panic ( "ip_srcroute" ); }

/* from net/if.c */
struct ifaddr * ifa_ifwithnet ( struct sockaddr *addr ) { panic ( "ifa_ifwithnet" ); }
struct ifaddr * ifa_ifwithaddr ( struct sockaddr *addr ) { panic ( "ifa_ifwithaddr" ); }
struct ifaddr * ifa_ifwithdstaddr ( struct sockaddr *addr ) { panic ( "ifa_ifwithstaddr" ); }

/* from net/route.c */
// void rtfree( struct rtentry *rt ) { panic ( "rtfree" ); }
// int rtrequest( int req, struct sockaddr *dst, struct sockaddr *gateway, struct sockaddr *netmask, int flags, struct rtentry **ret_nrt ) { panic ( "rtrequest" ); }

/* from net/rtsock.c */
// void rt_missmsg( int type, struct rt_addrinfo *rtinfo, int flags, int error) { panic ( "rt_missmsg" ); }

// void ip_freemoptions( register struct ip_moptions * ) { panic ( "ip_freemoptions" ); }

/* THE END */
