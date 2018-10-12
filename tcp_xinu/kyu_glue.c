/* kyu_glue.c */

/* Provide Xinu functionality in terms of
 *  Kyu function calls.
 */

#include <xinu.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"
// #include "../arch/cpu.h"

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
 * The specification is that FINWAIT is 2 * MSL.
 * This is why the original calls were shifting it by 1,
 * making the actual timeout 30 seconds.
 * The original 120 second MSL dates from original
 *  ARPAnet days where packets were indeed delayed
 *  by a minute or more.
 * I have been setting a FINWAIT of 5 seconds, which works
 *  just fine on a LAN for an IoT endpoint device,
 *  but would not be appropriate for a gateway and could
 *  have security implications.
 */

// int finwait_ms = TCP_MSL<<1;
int finwait_ms = TCP_MSL * 2;

/* Another tunable parameter */
/* We only have a 16 bit field in the TCP header to
 * hold the window size, so bufsize should be 65535 or less
 */
#ifdef notdef
int tcp_bufsize = 65535;
int tcp_allocsize = 65536;
#endif

int tcp_bufsize = 4096;
int tcp_allocsize = 4096;

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
 * It appears this is/was due to multiple calls to freemem for
 * the same block of memory.  The original Xinu code was immune
 * (idempotent) under this, but free() definitely is not.
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
 *
 * If we turn this around, we could start with saying that we want
 * to allow up to 16M to be used for TCP buffers,
 * With 2K (2048 byte) buffers, this would be 512 buffers per M,
 * so 16M would allow 16*512 buffers or 16*512/2 = 4096 TCB.
 *
 * This is now governed by a pair of global parameters,
 * tcp_bufsize and tcp_allocsize.  6-27-2018
 */

#define MEMB_INIT	(Ntcp * 2 + 2)

static char *memb_head;
static int memb_count;
static int memb_limit;

static void xinu_memb_show ( int );

static void
xinu_memb_init ( void )
{
	char * buf;
	int i;
	int bcount = MEMB_INIT;
	int nalloc = bcount * tcp_allocsize;

	buf = (char *) ram_alloc ( nalloc );
	printf ( " %d bytes allocated for TCP buffers\n", nalloc );

	/* Create linked list */
	memb_head = (char *) 0;
	memb_count = 0;
	for ( i=0; i < bcount; i++ ) {
	    * (char **) buf = memb_head;
	    memb_head = buf;
	    buf += tcp_allocsize;
	    memb_count++;
	}
	memb_limit = memb_count;

	if ( memb_limit != MEMB_INIT )
	    panic ( "Crazy business in xinu_memb_init" );

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

	if ( memb_limit != MEMB_INIT ) {
	    printf ( " Corruption in xinu_memb" );
	    memb_limit = MEMB_INIT;
	}

	while ( next ) {
	    count++;
	    next = * (char **) next;
	    if ( count > memb_limit + 10 )
		break;
	}

	printf ( "%d TCP buffers (of %d bytes) available of %d\n", count, tcp_bufsize, memb_limit );
}

static void
xinu_memb_show ( int limit )
{
	int count;
	char * next;

	if ( limit < 1 || limit > MEMB_INIT )
	    limit = 10;

	count = 0;
	next = memb_head;
	while ( count++ < limit ) {
	    printf ( "MEM %d: %08x\n", count, next );
	    next = * (char **) next;
	}
}

/* XXX - the following need locks */
char *
kyu_getmem ( int nbytes )
{
	char *rv;

	// printf ( "GETMEM -- %d bytes (%d available: %08x)\n", nbytes, memb_count, memb_head );
	// xinu_memb_show ( 8 );

	if ( nbytes > tcp_allocsize )
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
kyu_freemem ( char *buf, int xxx_nbytes )
{
	// printf ( "FREEMEM -- %08x\n", buf );

	* (char **) buf = memb_head;
	memb_head = buf;
	memb_count++;
	return OK;
}

/* THE END */
