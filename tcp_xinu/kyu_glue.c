/* kyu_glue.c */

/* Provide Xinu functionality in terms of
 *  Kyu function calls.
 */

#include <xinu.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"

int
semcreate ( int arg )
{
	if ( arg == 1 )
	    sem_mutex_new ( SEM_FIFO );
	else
	    sem_signal_new ( SEM_FIFO );
}

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

/* Test - active connection to Daytime server */
/* Sometimes this just locks up in SYNSENT ??!! */

static void
test_client ( void )
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

/* New style passive connection with listener callback */
/* Note that this just sets up callbacks and evaporates */

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

static void
test_server ( void )
{
	int lslot;
	int cslot;
	int rv;
	int port = 1234;

	lslot = tcp_register ( 0, port, 0 );
	printf ( "Listening on port %d (slot %d)\n", port, lslot );

	for ( ;; ) {
	    printf ( "Waiting for connection\n" );
	    rv = tcp_recv ( lslot, (char *) &cslot, 4 );
	    if ( rv < 0 ) {
		/* Without this check, once we tie up all slots in
		 * TWAIT, this will yield a runaway loop
		 */
		printf ( "recv on port %d fails - maybe no more slots?\n" );
		break;
	    }
	    printf ( "Connection!! %d, %d\n", rv, cslot );
	    tcp_send ( cslot, "dog\n", 4 );
	    tcp_send ( cslot, "cat\n", 4 );
	    tcp_close ( cslot );
	}
}

static void
tcp_xinu_test ( int bogus )
{
	test_client ();

	test_daytime ();
	test_server ();
}

void
test_xinu_tcp ( void )
{
	(void) thr_new ( "xinu_tester", tcp_xinu_test, NULL, 30, 0 );
}

static void
xinu_timer ( void )
{
	if ( tmnext && (--(*tmnext)) == 0 )
	    tmfire ();
}

void
tcp_xinu_init ( void )
{
	net_timer_hookup ( xinu_timer );

	tcp_init ();
}

// static int kyu_drop = 0;

static void
ip_ntoh ( struct netpacket *pktptr )
{
        pktptr->net_iplen = ntohs(pktptr->net_iplen);
        pktptr->net_ipid = ntohs(pktptr->net_ipid);
        pktptr->net_ipfrag = ntohs(pktptr->net_ipfrag);
        pktptr->net_ipsrc = ntohl(pktptr->net_ipsrc);
        pktptr->net_ipdst = ntohl(pktptr->net_ipdst);
}

void
tcp_xinu_rcv ( struct netbuf *nbp )
{
	struct netpacket *pkt;

#ifdef notdef
	if ( kyu_drop ) {
	    printf ( "xinu_rcv - drop\n" );
	    netbuf_free ( nbp );
	    return;
	}

	// kyu_drop = 1;
	printf ( "TCP: xinu_rcv %d\n", ntohs(nbp->iptr->len) );
#endif

	/* Must give eptr since we may have PREPAD */
	pkt = (struct netpacket *) nbp->eptr;
	ip_ntoh ( pkt );
	tcp_ntoh ( pkt );

	tcp_in ( pkt );
	/* do NOT free the packet here */
}

/* XXX - just a stub so we can link for now */
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

/* We now hide a pointer to the netbuf inside the netbuf
 * in the 4 bytes just in front of the packet itself.
 */
struct netpacket *
get_netpacket ( void )
{
	struct netbuf *nbp;

	nbp = netbuf_alloc ();
	if ( nbp ) {
	    // printf ( "TCP: alloc %08x, %08x\n", nbp, nbp->eptr );
	    return (struct netpacket *) nbp->eptr;
	}
	return NULL;
}

void
free_netpacket ( struct netpacket *pkt )
{
	struct netbuf *nbp;

	nbp = * (struct netbuf **) ((char *)pkt - sizeof(struct netbuf *));
	// printf ( "TCP: free  %08x, %08x\n", nbp, pkt );
	netbuf_free ( nbp );
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
net_bozo ( void )
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
