/* net_tcp.c
 * Handle a TCP packet.
 * T. Trebisky  4-20-2005
 */

#include <kyu.h>
#include <kyulib.h>

#include "net.h"
#include "netbuf.h"
#include "cpu.h"

static void tcp_reply_rst ( struct netbuf * );
void tcp_demo ( void );

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
tcp_init ( void )
{
	/* stub */
}

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

	net_show_packet ( "tcp_reply_rst", nbp );

	net_debug_arm ();
	ip_send ( nbp, xipp->src );
}

#ifdef not_any_more

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
	printf ( "my tcp checksum is %08x\n", htons(sum) );
}

#endif

#ifdef notdef

void
udp_send ( unsigned long dest_ip, int sport, int dport, char *buf, int size )
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
	bip->src = my_ip;	/* should be zero for BOOTP */

	udp->sum = in_cksum ( nbp->iptr, nbp->ilen );

	ip_send ( nbp, dest_ip );
}

extern unsigned long my_ip;

struct bogus_ip {
    	unsigned long x1;
    	unsigned long x2;
	/* */
	unsigned char x3;
	unsigned char proto;
	unsigned short len;
	/* */
	unsigned long src;
	unsigned long dst;
};

static void
tcp_reply_rst ( struct netbuf *nbp )
{
	struct bogus_ip save;
	struct tcp_hdr *tcp;
    	unsigned long tmp;
	struct bogus_ip *bip;
	unsigned long seq;

	tcp = (struct tcp_hdr *) nbp->pptr;

	/* turn the ports in the packet around */
	tmp = tcp->sport;
	tcp->sport = tcp->dport;
	tcp->dport = tmp;

	tcp->flags = TH_RST | TH_ACK;

	seq = ntohl(tcp->seq) + 1;
	tcp->ack = htonl(seq);
	tcp->seq = htonl(seq_start);

	tcp->win = 0;
	tcp->urg = 0;

	bip = (struct bogus_ip *) nbp->iptr;

	/* Save the entire IP header */
	save = *bip;

	memset ( (char *) bip, 0, sizeof(struct bogus_ip) );

	bip->proto = IPPROTO_TCP;
	bip->len = save.len;

	bip->dst = save.dst;
	bip->src = save.src;

	tcp->sum = 0;
	tcp->sum = in_cksum ( nbp->iptr, nbp->ilen );

	bip->x1 = save.x1;
	bip->x2 = save.x2;
	bip->x3 = save.x3;

	/* This will flip src/dst IP around */
	ip_reply ( nbp );
}
#endif

#ifdef notyet
/* XXX actually this is the UDP routine waiting for hackery */
void
udp_send_hacked ( unsigned long dest_ip, int sport, int dport, char *buf, int size )
{
	struct tcp_hdr *udp;
	struct netbuf *nbp;
	struct bogus_ip *bip;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	memcpy ( nbp->dptr, buf, size );

	size += sizeof(struct tcp_hdr);
	nbp->plen = size;
	nbp->ilen = size + sizeof(struct ip_hdr);

	/* The rest of this junk is just the UDP code.
	 * waiting to be converted to UDP
	 */
	udp = (struct tcp_hdr *) nbp->pptr;
	udp->sport = htons(sport);
	udp->dport = htons(dport);
	/*
	udp->len = htons(nbp->plen);
	*/
	udp->sum = 0;

	/* Fill out a bogus IP header just to do
	 * checksum computation.  It will pick up
	 * the proto field from this however.
	 */
	bip = (struct bogus_ip *) nbp->iptr;
	memset ( (char *) bip, 0, sizeof(struct bogus_ip) );
	bip->proto = IPPROTO_UDP;
	/*
	bip->len = udp->len;
	*/
	bip->dst = dest_ip;
	bip->src = my_ip;

	udp->sum = in_cksum ( nbp->iptr, nbp->ilen );

	ip_send ( nbp, dest_ip );
}
#endif

/* THE END */
