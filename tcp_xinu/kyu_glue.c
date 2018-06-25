/* kyu_glue.c */

/* Provide Xinu functionality in terms of
 *  Kyu function calls.
 */

#include <xinu.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"
#include "../arch/cpu.h"

static void xinu_memb_init ( void );
void xinu_memb_stats ( void );

/* Make this a tuneable parameter.
 * The "proper" value is 120 seconds !!
 * Linux uses 15 seconds, and that is our default.
 *
 * Making this tuneable allows shorter values to be set,
 * which may make sense for devices that only operate
 * on a fast LAN.  Long values tie up TCB entries and
 * thus limit the number of open connections, and ultimately
 * limit the number of connections per second that can
 * be made.   Short values and a big TCB table are
 * the only solution if a high connection rate is needed.
 * Note that the freemem buffers are not tied up when
 * a connection is in finwait.
 *
 * For some crazy reason all the calls were shifting it by 1,
 * making the actual timeout 30 seconds.
 * I do away with that confusing nonsense.
 */
// int finwait_ms = TCP_MSL<<1;
int finwait_ms = TCP_MSL;

void
set_finwait ( int secs )
{
	finwait_ms = secs * 1000;
}

/* See timer.c */
static void
xinu_timer ( void )
{
	if ( tmnext && (--(*tmnext)) == 0 )
	    tmfire ();
}

/* Kyu calls this during initialization */
void
tcp_xinu_init ( void )
{
	xinu_memb_init ();

	xinu_memb_stats ();
	netbuf_show ();

	set_finwait ( 5 );

	net_timer_hookup ( xinu_timer );

	tcp_init ();

#ifdef notdef
	char xx[] = "dog\n";
	int *lp;
	lp = (int *) &xx[0];
	printf ("%s -- %08x %08x\n", xx, xx, *lp );
#endif
}

static void
ip_ntoh ( struct netpacket *pktptr )
{
        pktptr->net_iplen = ntohs(pktptr->net_iplen);
        pktptr->net_ipid = ntohs(pktptr->net_ipid);
        pktptr->net_ipfrag = ntohs(pktptr->net_ipfrag);
        pktptr->net_ipsrc = ntohl(pktptr->net_ipsrc);
        pktptr->net_ipdst = ntohl(pktptr->net_ipdst);
}

/* This is where arriving packets get delivered by Kyu
 *  to the Xinu tcp code.
 *  From ip_rcv() in net/net_ip.c
 * Be sure and let Kyu free the packet.
 */
void
tcp_xinu_rcv ( struct netbuf *nbp )
{
	struct netpacket *pkt;

	// printf ( "TCP: xinu_rcv %d\n", ntohs(nbp->iptr->len) );

	/* Must give eptr since we may have PREPAD */
	pkt = (struct netpacket *) nbp->eptr;
	ip_ntoh ( pkt );
	tcp_ntoh ( pkt );

	tcp_in ( pkt );
	/* do NOT free the packet here */
}

/* A packet allocator for when Xinu needs to start
 * from scratch to send a packet.
 * This happens in 3 places, typically triggered
 * from tcp_out():
 *  1 - tcpsendseg() - to send data
 *  2 - tcpack() - to send ACK
 *  3 - tcpreset() - to send RST
 * In each of these cases, the packet is handed to
 *  ip_enqueue above, which passes it to ip_send() in Kyu code.
 *  this calls the driver "send" routine, then frees the netbuf
 *  once the driver has done whatever it wants to do.
 *
 * On 6-21-2018, this replaced get_netpacket()
 * As of 6-24-2018, we directly call netbuf_alloc()
 */

#ifdef notdef
struct netbuf *
net_alloc ( void )
{
	struct netbuf *nbp;

	nbp = netbuf_alloc ();
	return nbp;
}
#endif

/* This is where Xinu tcp code sends packets to hand them off to Kyu */
void
net_enqueue ( struct netbuf *nbp ) 
{
	unsigned short cksum;
	struct netpacket *pkt;

	pkt = (struct netpacket *) nbp->eptr;

	nbp->ilen = pkt->net_iplen;
	nbp->plen = pkt->net_iplen - sizeof(struct ip_hdr);

	// printf ( "TCP:  ip_enqueue %d (%d)\n", nbp->ilen, sizeof(struct eth_hdr) );
	// printf ( "TCP:  ip_enqueue sport, dport = %d %d\n", 
	//     pkt->net_tcpsport, pkt->net_tcpdport );

	tcp_hton ( pkt );
	cksum = tcpcksum ( pkt );
	pkt->net_tcpcksum = htons(cksum) & 0xffff;

	// printf ( "TCP:  ip_enqueue sport, dport = %d %d\n", 
	//     pkt->net_tcpsport, pkt->net_tcpdport );

	ip_send ( nbp, htonl(pkt->net_ipdst) );
}

#ifdef notdef
/* This is where Xinu tcp code sends packets to hand them off to Kyu */
void
ip_enqueue_OLD ( struct netpacket *pkt ) 
{
	struct netbuf *nbp;
	unsigned short cksum;

	nbp = * (struct netbuf **) ((char *)pkt - sizeof(struct netbuf *));

	/* XXX - should just be able to set ilen */
	nbp->ilen = pkt->net_iplen;
	nbp->plen = pkt->net_iplen - sizeof(struct ip_hdr);

	// printf ( "TCP:  ip_enqueue %d (%d)\n", nbp->ilen, sizeof(struct eth_hdr) );
	// printf ( "TCP:  ip_enqueue sport, dport = %d %d\n", 
	//     pkt->net_tcpsport, pkt->net_tcpdport );

	tcp_hton ( pkt );
	cksum = tcpcksum ( pkt );
	pkt->net_tcpcksum = htons(cksum) & 0xffff;

	// printf ( "TCP:  ip_enqueue sport, dport = %d %d\n", 
	//     pkt->net_tcpsport, pkt->net_tcpdport );

	ip_send ( nbp, htonl(pkt->net_ipdst) );
}

/* A packet allocator for when Xinu needs to start
 * from scratch to send a packet.
 * This happens in 3 places, typically triggered
 * from tcp_out():
 *  1 - tcpsendseg() - to send data
 *  2 - tcpack() - to send ACK
 *  3 - tcpreset() - to send RST
 * In each of these cases, the packet is handed to
 *  ip_enqueue above, which passes it to ip_send() in Kyu code.
 *  this calls the driver "send" routine, then frees the netbuf
 *  once the driver has done whatever it wants to do.
 */
struct netpacket *
get_netpacket_OLD ( void )
{
	struct netbuf *nbp;

	nbp = netbuf_alloc ();
	if ( nbp ) {
	    // printf ( "TCP: alloc %08x, %08x\n", nbp, nbp->eptr );
	    printf ( "XGET netpacket: %08x\n", nbp->eptr );
	    return (struct netpacket *) nbp->eptr;
	}
	return NULL;
}

/* This whole idea was broken anyway.
 * This is called only in tcp_in() and we now have all
 * those calls commented out.  Kyu will free any input packets
 * after we return from tcp_in().
 */
void
free_netpacket_OLD ( struct netpacket *pkt )
{
	struct netbuf *nbp;

	printf ( "XFREE netpacket: %08x\n", pkt );
	printf ( ".... leaks\n" );

	nbp = * (struct netbuf **) ((char *)pkt - sizeof(struct netbuf *));
	// printf ( "TCP: free  %08x, %08x\n", nbp, pkt );
	printf ( "XFREE netbuf: %08x\n", nbp );
	netbuf_free ( nbp );
}
#endif

int
semcreate ( int arg )
{
	if ( arg == 1 )
	    sem_mutex_new ( SEM_FIFO );
	else
	    sem_signal_new ( SEM_FIFO );
}

/* getmem and freemem were macros in kyu_glue.h up to 6-20-2018
 * I got a data abort from within malloc, and thought
 * perhaps my macros are to blame.  That was not the issue.
 * Next I added INT_lock/unlock to malloc/free.  That was not it.
 * XXX - still under investigation ....
 */

#ifdef notdef
#define	getmem(n)	malloc((n))
#define	freemem(x,s)	free((x))
#endif

/* getmem / freemem are used in the Xinu TCP code to allocate buffers
 * of a fixed size (65535).  I just hand out blocks of 65536
 * since that is a nice round number.
 * We get 16 of these per megabyte.
 * We need two per TCB control block, so with Ntcp = 100, we need 200
 *  which consumes 200/16 = 12.5 megabytes.
 */

#define XINU_BSIZE 65536

static char *memb_head;
static int memb_count;
static int memb_limit;

static void xinu_memb_show ( int );

static void
xinu_memb_init ( void )
{
	char * buf;
	int i;
	int bcount = Ntcp * 2 + 2;

	buf = (char *) ram_alloc ( bcount * XINU_BSIZE );

	/* Create linked list */
	memb_head = (char *) 0;
	memb_count = 0;
	for ( i=0; i < bcount; i++ ) {
	    * (char **) buf = memb_head;
	    memb_head = buf;
	    buf += XINU_BSIZE;
	    memb_count++;
	}
	memb_limit = memb_count;

	// xinu_memb_show ( 8 );
}

/* Called from test menu */
void
xinu_memb_stats ( void )
{
	int count;
	char * next;

	count = 0;
	next = memb_head;
	while ( next ) {
	    count++;
	    next = * (char **) next;
	}
	printf ( "MEMB: %d available of %d\n", count, memb_limit );
}

static void
xinu_memb_show ( int limit )
{
	int count;
	char * next;

	count = 0;
	next = memb_head;
	while ( count++ < limit ) {
	    printf ( "MEM %d: %08x\n", count, next );
	    next = * (char **) next;
	}
}

/* XXX - the following need locks */
char *
kyu_getmem ( uint32 nbytes )
{
	char *rv;

	// printf ( "GETMEM -- %d bytes (%d available: %08x)\n", nbytes, memb_count, memb_head );
	// xinu_memb_show ( 8 );

	if ( nbytes > XINU_BSIZE )
	    panic ( "Xinu TCP getmem allocator block too small" );
	if ( ! memb_head )
	    panic ( "Xinu TCP getmem allocator out of memory" );

	rv = memb_head;
	memb_head = * (char **) memb_head;
	memb_count--;
	// printf ( "GETMEM:: %08x\n", rv );
	return rv;
}

int
kyu_freemem ( char *buf, uint32 xxx_nbytes )
{
	// printf ( "FREEMEM -- %08x\n", buf );

	* (char **) buf = memb_head;
	memb_head = buf;
	memb_count++;
	return OK;
}

/* THE END */
