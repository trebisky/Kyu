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

static void xinu_memb_show ( int );

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
