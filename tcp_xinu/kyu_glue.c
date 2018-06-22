/* kyu_glue.c */

/* Provide Xinu functionality in terms of
 *  Kyu function calls.
 */

#include <xinu.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"

static void xinu_memb_init ( void );

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
	char xx[] = "dog\n";
	int *lp;

	xinu_memb_init ();

	net_timer_hookup ( xinu_timer );

	tcp_init ();

	lp = (int *) &xx[0];
	printf ("%s -- %08x %08x\n", xx, xx, *lp );
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
 */

struct netbuf *
net_alloc ( void )
{
	struct netbuf *nbp;

	nbp = netbuf_alloc ();
	return nbp;
}

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
ip_enqueue ( struct netpacket *pkt ) 
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
get_netpacket ( void )
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
#endif

#ifdef notdef
/* This whole idea was broken anyway.
 * This is called only in tcp_in() and we now have all
 * those calls commented out.  Kyu will free any input packets
 * after we return from tcp_in().
 */
void
free_netpacket ( struct netpacket *pkt )
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
 */

#define XINU_BSIZE 65536
#define XINU_BCOUNT 4*16

static char *memb_head;
static int memb_count;

void xinu_memb_show ( int );

static void
xinu_memb_init ( void )
{
	char * buf;
	int i;

	buf = (char *) ram_alloc ( XINU_BCOUNT * XINU_BSIZE );

	/* Create linked list */
	memb_head = (char *) 0;
	memb_count = 0;
	for ( i=0; i < XINU_BCOUNT; i++ ) {
	    * (char **) buf = memb_head;
	    memb_head = buf;
	    buf += XINU_BSIZE;
	    memb_count++;
	}

	xinu_memb_show ( 8 );
}

void
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

char *
getmem ( uint32 nbytes )
{
	char *rv;

	printf ( "GETMEM -- %d bytes (%d available: %08x)\n", nbytes, memb_count, memb_head );
	xinu_memb_show ( 8 );

	if ( nbytes > XINU_BSIZE )
	    panic ( "Xinu TCP getmem allocator block too small" );
	if ( ! memb_head )
	    panic ( "Xinu TCP getmem allocator out of memory" );

	rv = memb_head;
	memb_head = * (char **) memb_head;
	memb_count--;
	printf ( "GETMEM:: %08x\n", rv );
	return rv;
}

int
freemem ( char *buf, uint32 xxx_nbytes )
{
	printf ( "FREEMEM -- %08x\n", buf );

	* (char **) buf = memb_head;
	memb_head = buf;
	memb_count++;
	return OK;
}

#ifdef notdef
char *
getmem ( uint32 nbytes )
{
	char *rv;

	printf ( "Xinu: getmem  %5d bytes requested\n", nbytes );

	rv =  (char *) malloc ( nbytes );

	printf ( "Xinu: getmem  %5d: %08x\n", nbytes, rv );

	return rv;
	//return (char *) malloc ( nbytes );
}

// syscall
int
freemem ( char *blkaddr, uint32 nbytes )
{
	printf ( "Xinu: freemem %5d: %08x\n", nbytes, blkaddr );
	free ( blkaddr );

	return OK;
}
#endif

void
xinu_show ( void )
{
	int i;
	struct tcb *tp;

	for (i = 0; i < Ntcp; i++) {
	    if (tcbtab[i].tcb_state == TCB_FREE)
		continue;

	    tp = &tcbtab[i];
	    if ( tp->tcb_state == TCB_LISTEN )
		printf ( "TCB slot %d: Listen on port %d\n", i, tp->tcb_lport );
	    else if ( tp->tcb_state == TCB_ESTD )
		printf ( "TCB slot %d: Established %d (%d)\n", i, tp->tcb_lport, tp->tcb_rport );
	    else if ( tp->tcb_state == TCB_CLOSED )
		printf ( "TCB slot %d: Closed\n", i );
	    else if ( tp->tcb_state == TCB_CWAIT )
		printf ( "TCB slot %d: Close wait\n", i );
	    else if ( tp->tcb_state == TCB_TWAIT )
		printf ( "TCB slot %d: TWAIT\n", i );
	    else if ( tp->tcb_state == TCB_SYNSENT )
		printf ( "TCB slot %d: SYNSENT\n", i );
	    else
		printf ( "TCB slot %d: state = %d\n", i, tp->tcb_state );
        }
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* Test stuff follows. */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */


#define TEST_SERVER     "192.168.0.5"
#define ECHO_PORT       7       /* echo */
#define DAYTIME_PORT    13      /* daytime */

static unsigned long
dots2ip ( char *s )
{
	int nip;

	(void) net_dots ( s, &nip );
	return ntohl ( nip );
}

/* This requires a daytime server running on port 13
 * ( on TEST_SERVER -- likely the boot host).
 * To run this on fedora:
 *  dnf install xinetd
 *  edit /etc/xinet.d/daytime-stream to enable
 *  systemctl start  xinetd.service
 */
static void
client_daytime ( void )
{
	int port = DAYTIME_PORT;
	int slot;
	char buf[100];
	int n;

	// printf ( "Begin making client connection\n" );
	slot = tcp_register ( dots2ip(TEST_SERVER), port, 1 );
	printf ( "Client connection on port %d, slot = %d\n", port, slot );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );
	// terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );
	tcp_close ( slot );
}

/* This requires an echo server running on port 7
 *  on the TEST_SERVER host ( likely the boot host).
 * To run this on fedora:
 *  dnf install xinetd
 *  edit /etc/xinet.dd/echo-stream to enable
 *  (change disable to no)
 *  systemctl restart  xinetd.service
 *  test using "telnet localhost 7"
 */
static void
client_echo ( void )
{
	int port = ECHO_PORT;
	int slot;
	char buf[100];
	int n;

	// printf ( "Begin making client connection\n" );
	slot = tcp_register ( dots2ip(TEST_SERVER), port, 1 );
	printf ( "Client connection on port %d, slot = %d\n", port, slot );
	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	tcp_send ( slot, "Goose!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	// replies are terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );

	tcp_close ( slot );
}

/* Test - active connection to Daytime server */
/* Sometimes this just locks up in SYNSENT ??!! */

static void
test_client ( void )
{
	client_daytime ();
	client_echo ();
}

/* Callback function to handle the following */

static void
daytime_func ( int slot )
{
	static char reply[] = "4 JUL 1776 08:00:00 EST\r\n";
	int n;

	n = sizeof(reply);
	printf ( "In daytime callback, slot %d\n", slot );
	printf ( "In daytime callback, sending %d bytes\n", n );
	tcp_send ( slot, reply, n );
	tcp_close ( slot );
}

/* New style passive connection with listener callback */
/* Note that this just sets up a callback and evaporates */
/* So traffic is just handled by the network thread and we
 *  have no thread associated with this server.
 */

static void
test_daytime ( void )
{
	int port = DAYTIME_PORT;
	int rv;

	rv = tcp_server ( port, daytime_func );
	printf ( "Listening on port %d via callback (%d)\n", port, rv );
}

/* Original style passive connection */
/* This stays around, blocked in tcp_recv() */
/* Just use telnet to talk to this, once the connection is made
 * the two strings "cat and dog" should be received.
 *
 * XXX
 * This needs further debug, it works once,
 * but that single time it works looks like so:
 * Listening on port 1234 (slot 0)
 * Waiting for connection
 * Connection!! 1, 2
 * Waiting for connection
 * recv on port 1234 fails - maybe no more slots?
 */

static void
test_server ( int bogus )
{
	int lslot;
	int cslot;
	int rv;
	int port = 1234;

	lslot = tcp_register ( 0, port, 0 );
	printf ( "Server listening on port %d (slot %d)\n", port, lslot );

	for ( ;; ) {
	    printf ( "Waiting for connection on port %d\n", port );
	    rv = tcp_recv ( lslot, (char *) &cslot, 4 );
	    if ( rv < 0 ) {
		/* Without this check, once we tie up all slots in
		 * TWAIT, this will yield a runaway loop
		 */
		printf ( "recv on port %d fails - maybe no more slots?\n", port );
		break;
	    }
	    printf ( "Connection on port %d!! rv= %d, slot= %d\n", port, rv, cslot );
	    tcp_send ( cslot, "dog\n", 4 );
	    tcp_send ( cslot, "cat\n", 4 );
	    tcp_close ( cslot );
	}
	printf ( "Server on port %d exiting !!\n", port );
}

static void
launch_classic_server ( void )
{
	(void) thr_new ( "xinu_tester", test_server, NULL, 30, 0 );
}

static int servers_running = 0;

void
test_xinu_tcp ( void )
{
	/* Test an active client to port 13 */
	test_client ();

	if ( ! servers_running ) {
	    servers_running = 1;

	    /* This starts a daytime server on port 13 */
	    test_daytime ();

	    /* This starts a server on port 1234 */
	    /* This needs to run in a thread */
	    launch_classic_server ();

	    thr_delay ( 10 );

	    /* Start a new style server on port 13 */
	    /* No thread needed */
	    /* BUG - if we run this alongside the classic,
	     * the classic fails */
	    // test_daytime ();

	}
}

/* Available from test menu
 *  Test 13: Endless TCP echo
 */
void
tcp_echo_test ( void )
{
	/* Your ad here */
	printf ( "Not ready yet\n" );
}

#ifdef notdef
/* Without the pragma, this is 1516 bytes in size
 * with it, the size is 1514.
 * The pragma (in this case) only affects the overall size of
 *  the structure.  The packing of elements inside is the same
 *  with or without it.
 * Notice that 1516 = 379 * 4
 */

#pragma pack(2)
// struct	netpacket	{
struct	netpk	{
	byte	net_ethdst[ETH_ADDR_LEN];/* Ethernet dest. MAC address	*/
	byte	net_ethsrc[ETH_ADDR_LEN];/* Ethernet source MAC address	*/
	uint16	net_ethtype;		/* Ethernet type field		*/
	byte	net_ipvh;		/* IP version and hdr length	*/
	byte	net_iptos;		/* IP type of service		*/
	uint16	net_iplen;		/* IP total packet length	*/
	uint16	net_ipid;		/* IP datagram ID		*/
	uint16	net_ipfrag;		/* IP flags & fragment offset	*/
	byte	net_ipttl;		/* IP time-to-live		*/
	byte	net_ipproto;		/* IP protocol (actually type)	*/
	uint16	net_ipcksum;		/* IP checksum			*/
	uint32	net_ipsrc;		/* IP source address		*/
	uint32	net_ipdst;		/* IP destination address	*/
	union {
	 struct {
	  uint16 	net_udpsport;	/* UDP source protocol port	*/
	  uint16	net_udpdport;	/* UDP destination protocol port*/
	  uint16	net_udplen;	/* UDP total length		*/
	  uint16	net_udpcksum;	/* UDP checksum			*/
	  byte		net_udpdata[1500-28];/* UDP payload (1500-above)*/
	 };
	 struct {
	  byte		net_ictype;	/* ICMP message type		*/
	  byte		net_iccode;	/* ICMP code field (0 for ping)	*/
	  uint16	net_iccksum;	/* ICMP message checksum	*/
	  uint16	net_icident; 	/* ICMP identifier		*/
	  uint16	net_icseq;	/* ICMP sequence number		*/
	  byte		net_icdata[1500-28];/* ICMP payload (1500-above)*/
	 };
	 struct {
	  uint16	net_tcpsport;	/* TCP Source port		*/
	  uint16	net_tcpdport;	/* TCP destination port		*/
	  int32		net_tcpseq;	/* TCP sequence no.		*/
	  int32		net_tcpack;	/* TCP acknowledgement sequence	*/
	  uint16	net_tcpcode;	/* TCP flags			*/
	  uint16	net_tcpwindow;	/* TCP receiver window		*/
	  uint16	net_tcpcksum;	/* TCP checksum			*/
	  uint16	net_tcpurgptr;	/* TCP urgent pointer		*/
	  byte		net_tcpdata[1500-40];/* TCP payload		*/
	 };
	};
	// uint32		zzz;
};
#pragma pack()

void
net_inspect ( void )
{
	struct netpk *np;

	np = (struct netpk *) 0;

	printf ( "netpacket is %d bytes\n", sizeof(struct netpk) );
	printf ( " start: %08x\n", np->net_ethdst );
	printf ( " ethtype: %08x\n", &np->net_ethtype );
	printf ( " ipvh: %08x\n", &np->net_ipvh );
	printf ( " iptos: %08x\n", &np->net_iptos );
	printf ( " iplen: %08x\n", &np->net_iplen );
	printf ( " ipdst: %08x\n", &np->net_ipdst );
	//printf ( " zzz: %08x %d\n", &np->zzz, &np->zzz );
}
#endif

/* THE END */
