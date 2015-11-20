/* net_tcp.c
 * Handle a TCP packet.
 * T. Trebisky  4-20-2005
 */

#include <kyu.h>
#include <kyulib.h>
#include <malloc.h>

#include "net.h"
#include "netbuf.h"
#include "cpu.h"

typedef void (*tcp_fptr) ( char *, int );

static void tcp_reply_rst ( struct netbuf * );
struct tcp_io * tcp_connect ( unsigned long, int, tcp_fptr );

struct tcp_io {
	unsigned long remote_ip;
	int remote_port;
	int local_port;
	tcp_fptr rcv_func;
};

/* XXX - bit fields are little endian only.
 * 20 bytes in all, but options may follow.
 */
struct tcp_hdr {
	unsigned short	sport;
	unsigned short	dport;
	/* - */
	unsigned long seq;
	unsigned long ack;
	/* - */
	unsigned char _pad0:4,
		      hlen:4;	/* header length (in words) (off in BSD) */
#ifdef notdef
	unsigned char hlen:4,	/* header length (in words) (off in BSD) */
			_pad0:4;
#endif
	unsigned char flags;	/* 6 bits defined */
	unsigned short win;	/* advertised window */
	/* - */
	unsigned short sum;
	unsigned short urg;	/* urgent pointer */
};

/* Here's the flags */
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20

void
tcp_tester ( char *buf, int count )
{
	/* XXX - should dump packet */
}

/* XXX - this gets called, but has no purpose yet.
 * Use for debug and ultimately initialization.
 */
void
tcp_init ( void )
{
	unsigned long ip;
	struct tcp_io *tio;

	(void) net_dots ( "192.168.0.5", &ip );
	tio = tcp_connect ( ip, 17, tcp_tester );
}

/* Called with every received TCP packet */
void
tcp_rcv ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	int sum;
	char *cptr;

	tcp = (struct tcp_hdr *) nbp->pptr;
	nbp->dlen = nbp->plen - sizeof(struct tcp_hdr);
	nbp->dptr = nbp->pptr + sizeof(struct tcp_hdr);

#ifdef DEBUG_TCP
	printf ( "TCP from %s: src/dst = %d/%d, size = %d",
		ip2strl ( nbp->iptr->src ), ntohs(tcp->sport), ntohs(tcp->dport), nbp->plen );
		printf ( " hlen = %d (%d bytes), seq = %lu (%08x), ack = %lu (%08x)\n",
		    tcp->hlen, tcp->hlen * 4,
		    ntohl(tcp->seq), ntohl(tcp->seq),
		    ntohl(tcp->ack), ntohl(tcp->ack) );
#endif

	if ( tcp->flags == TH_SYN ) {
	    printf ( "Sending TCP rst/ack\n");
	    tcp_reply_rst ( nbp );
	}

	netbuf_free ( nbp );
}

struct tcp_pseudo {
	unsigned long src;
	unsigned long dst;
	unsigned short proto;
	unsigned short len;
};

static unsigned long seq_start = 99;

static void
tcp_reply_rst ( struct netbuf *xbp )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_hdr *xtcp;
	struct ip_hdr *xipp;
	unsigned long seq;
	struct tcp_pseudo pseudo;
	unsigned short sum;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->dlen = 0;
	nbp->plen = nbp->dlen + sizeof(struct tcp_hdr);
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	memset ( nbp->pptr, 0, sizeof(struct tcp_hdr) );

	nbp->iptr->proto = IPPROTO_TCP;

	tcp = (struct tcp_hdr *) nbp->pptr;

	/* flip the ports in the reply packet */
	xtcp = (struct tcp_hdr *) xbp->pptr;
	tcp->sport = xtcp->dport;
	tcp->dport = xtcp->sport;

	tcp->flags = TH_RST | TH_ACK;

	seq = ntohl(xtcp->seq) + 1;
	tcp->ack = htonl(seq);
	// tcp->seq = htonl(seq_start);
	tcp->seq = 0;

	tcp->hlen = 5;

	tcp->win = 0;
	tcp->urg = 0;

	xipp = xbp->iptr;

	/* Setup and checksum the pseudo header */
	pseudo.src = xipp->dst;
	pseudo.dst = xipp->src;

	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );

	/* Note - we do NOT byte reorder the checksum.
	 * if it was done on byte swapped data, it will be correct
	 * as is !!
	 */
	tcp->sum = ~sum;

	/*
	net_debug_arm ();
	*/
	ip_send ( nbp, xipp->src );
}

/* Just a start, needs work XXX */
static void
tcp_send_syn ( struct tcp_io *tp )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;

	/*
	struct tcp_hdr *xtcp;
	struct ip_hdr *xipp;
	*/
	struct tcp_pseudo pseudo;
	unsigned short sum;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->dlen = 0;
	nbp->plen = nbp->dlen + sizeof(struct tcp_hdr);
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	memset ( nbp->pptr, 0, sizeof(struct tcp_hdr) );

	nbp->iptr->proto = IPPROTO_TCP;

	tcp = (struct tcp_hdr *) nbp->pptr;

	tcp->sport = tp->local_port;
	tcp->dport = tp->remote_port;

	tcp->flags = TH_SYN;

	tcp->ack = 0;

	// tcp->seq = htonl(seq_start);
	tcp->seq = 0;

	tcp->hlen = 5;

	tcp->win = 0;
	tcp->urg = 0;

#ifdef notdef
	xipp = xbp->iptr;

	/* Setup and checksum the pseudo header */
	pseudo.src = xipp->dst;
	pseudo.dst = xipp->src;
#endif

	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );

	/* Note - we do NOT byte reorder the checksum.
	 * if it was done on byte swapped data, it will be correct
	 * as is !!
	 */
	tcp->sum = ~sum;

	/*
	net_debug_arm ();
	*/
	ip_send ( nbp, tp->remote_ip );
}

struct tcp_io *
tcp_connect ( unsigned long ip, int port, tcp_fptr rfunc )
{
	struct tcp_io *tp;

	tp = (struct tcp_io *) malloc ( sizeof(struct tcp_io) );
	if ( ! tp )
	    return NULL;

	tp->remote_ip = ip;
	tp->remote_port = port;
	tp->local_port = get_ephem_port ();
	tp->rcv_func = rfunc;

	tcp_send_sync ( tp );

	return tp;
}

/* Call this to send payload data to remote */
void
tcp_send ( struct tcp_io *tp, char *buf, int count )
{
}

#ifdef no_way
void tcp_demo ( void );

/* Here is a TCP packet with a valid checksum.
 * 54 bytes
 */

static char tcp_demo_b[] = {
    0xc8,0x60,0x00,0x36,0xc7,0xdb,
    0x90,0x2b,0x34,0x37,0x55,0xe2,
    0x08,0x00,
    0x45,0x00,0x00,0x28,0xc2,0x6d,0x40,0x00,0x40,0x06,0xf7,0x06,
    0xc0,0xa8,0x00,0x05,
    0xc0,0xa8,0x00,0x06,
    0x00,0x21, 0xc3,0xd7,
    0x00,0x00,0x00,0x00,
    0xa8,0xc3,0x02,0x52,
    0x50,0x14, 0x00,0x00,
    0xbf,0x66,		/* <--- checksum */
    0x00,0x00
};

void
tcp_demo ( void )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_pseudo pseudo;
	unsigned short sum;

	printf ( "Demo packet is %d bytes\n", sizeof(tcp_demo_b) );

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	memcpy ( nbp->eptr, tcp_demo_b, sizeof(tcp_demo_b) );
	nbp->elen = sizeof(tcp_demo_b);

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->ilen = nbp->elen - sizeof(struct eth_hdr);
	nbp->plen = nbp->ilen - sizeof(struct ip_hdr);
	nbp->dlen = nbp->plen - sizeof(struct tcp_hdr);

	printf ( "Demo packet has %d bytes of payload\n", nbp->dlen );

	tcp = (struct tcp_hdr *) nbp->pptr;
	printf ( "good tcp checksum is %04x\n", ntohs ( tcp->sum ) );

	/* Setup and checksum the pseudo header */
	pseudo.src = nbp->iptr->src;
	pseudo.dst = nbp->iptr->dst;

	/* shorts -- should byte swap */
	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	tcp->sum = 0;

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );
	sum = ~sum;
	printf ( "my tcp checksum is %04x\n", htons(sum) );

	tcp->sum = sum;

	ip_arp_send ( nbp );
}
#endif

/* THE END */
