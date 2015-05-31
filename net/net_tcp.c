/* net_tcp.c
 * Handle a TCP packet.
 * T. Trebisky  4-20-2005
 */

#include "net.h"
#include "netbuf.h"

static void tcp_reply_rst ( struct netbuf * );

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

extern unsigned long my_ip;

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
	printf ( "TCP from %s: src/dst = %d/%d, size = %d\n",
		ip2str ( nbp->iptr->src ), ntohs(tcp->sport), ntohs(tcp->dport), nbp->plen );
#endif

	if ( tcp->flags == TH_SYN ) {
	    printf ( "Sending TCP rst/ack\n");
	    tcp_reply_rst ( nbp );
	} else
	    netbuf_free ( nbp );
}

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

	tcp = (struct tcp_hdr *) nbp->pptr;

	/* turn the packet around */
	tmp = tcp->sport;
	tcp->sport = tcp->dport;
	tcp->dport = tmp;

	tcp->flags = TH_RST | TH_ACK;

	tcp->ack = tcp->seq + 1;
	tcp->seq = 0;

	tcp->win = 0;
	tcp->urg = 0;

	bip = (struct bogus_ip *) nbp->iptr;

	/* Save the entire IP header */
	save = *bip;

	memset ( (char *) bip, 0, sizeof(struct bogus_ip) );

	bip->proto = IPPROTO_TCP;
	bip->len = save.len;

	bip->dst = save.src;
	bip->src = save.dst;

	tcp->sum = 0;
	tcp->sum = in_cksum ( nbp->iptr, nbp->ilen );

	bip->x1 = save.x1;
	bip->x2 = save.x2;
	bip->x3 = save.x3;

	ip_reply ( nbp );
}

/* XXX actually this is the UDP routine waiting for hackery */
void
tcp_send ( unsigned long dest_ip, int sport, int dport, char *buf, int size )
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

/* THE END */
