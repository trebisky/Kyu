/* kyu_subs.c */

/* support code for BSD 4.4 TCP code in Kyu
 * mostly hackish cruft and stubs.
 * BSD global variables are here.
 */

#include <bsd.h>

/* global variables */

struct  mbstat mbstat;		/* mbuf.h */
// union   mcluster *mclfree;

/* socketvar.h */
// u_long	sb_max;

struct  ifnet   *ifnet;		/* net/if.h */

/* from net/route.h */
// struct	route_cb route_cb;
// struct	rtstat	rtstat;
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

/* From netinet/ip_input.c */
u_char inetctlerrmap[PRC_NCMDS] = {
        0,              0,              0,              0,
        0,              EMSGSIZE,       EHOSTDOWN,      EHOSTUNREACH,
        EHOSTUNREACH,   EHOSTUNREACH,   ECONNREFUSED,   ECONNREFUSED,
        EMSGSIZE,       EHOSTUNREACH,   0,              0,
        0,              0,              0,              0,
        ENOPROTOOPT
};

static struct  in_ifaddr our_ifaddr;

static void
ifaddr_setup ( void )
{
	struct sockaddr_in *sp;

        /* list of configured interfaces */
        // in_ifaddr_head = NULL;
	bzero ( (void *) &our_ifaddr, sizeof(struct in_ifaddr) );
	our_ifaddr.ia_next = NULL;

	sp = & our_ifaddr.ia_addr;
	sp->sin_family = AF_INET;
	sp->sin_addr.s_addr = ntohl(get_our_ip());	/* order */
	sp->sin_len = sizeof(struct in_addr);

        in_ifaddr_head = &our_ifaddr;

	/*
	u_char  sin_len;
        u_char  sin_family;
        u_short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
	*/
}

void
tcp_globals_init ( void )
{
	ifaddr_setup ();

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

#ifdef notdef
/* tsleep/wakeup are key kernel synch facilities in BSD
 * found in kern/kern_synch.c
 * For now, let them be noops.
 */
int tsleep ( void *ident, int priority, char *wmesg, int timo ) {}

// void wakeup ( void ) { panic ( "wakeup" ); }
void wakeup ( void ) { }
#endif

/* from net/if.c */
/* These would scan the ifnet list looking for an interface that met
 * some criteria.
 */
// struct ifaddr * ifa_ifwithnet ( struct sockaddr *addr ) { panic ( "ifa_ifwithnet" ); }
// struct ifaddr * ifa_ifwithaddr ( struct sockaddr *addr ) { panic ( "ifa_ifwithaddr" ); }
// struct ifaddr * ifa_ifwithdstaddr ( struct sockaddr *addr ) { panic ( "ifa_ifwithstaddr" ); }

struct ifaddr * ifa_ifwithnet ( struct sockaddr *addr ) { return (struct ifaddr *) 0; }
struct ifaddr * ifa_ifwithaddr ( struct sockaddr *addr ) { return (struct ifaddr *) 0; }
struct ifaddr * ifa_ifwithdstaddr ( struct sockaddr *addr ) { return (struct ifaddr *) 0; }

/* Handy address full of all zeros */
struct in_addr zeroin_addr = { 0 };

/* From netinet/ip_input.c
 * this oddity bears some looking at -- gone in NetBSD-1.3
 */
struct  in_ifaddr *in_ifaddr_head; /* first inet address */

/* THE END */
