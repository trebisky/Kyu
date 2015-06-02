/* net_udp.c
 * Handle an UDP packet.
 * T. Trebisky  4-11-2005
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#define BOOTP_PORT2	68	/* receive on this port */
#define DNS_PORT	53

void bootp_rcv ( struct netbuf * );

/* 8 bytes */
struct udp_hdr {
	unsigned short	sport;
	unsigned short	dport;
	/* - */
	unsigned short len;
	unsigned short sum;
};

extern unsigned long my_ip;

void
udp_rcv ( struct netbuf *nbp )
{
	struct udp_hdr *udp;
	struct udp_phdr *psp;
	int sum;
	char *cptr;

	udp = (struct udp_hdr *) nbp->pptr;
	nbp->dlen = nbp->plen - sizeof(struct udp_hdr);
	nbp->dptr = nbp->pptr + sizeof(struct udp_hdr);

#ifdef DEBUG_UDP
	printf ( "UDP from %s: src/dst = %d/%d\n",
		ip2strl ( nbp->iptr->src ), ntohs(udp->sport), ntohs(udp->dport) );
#endif

	/* XXX - validate checksum of received packets
	 */

	/* XXX - Generalize this with port lookup and hookup table */
	if ( BOOTP_PORT2 == ntohs(udp->dport) ) {
	    bootp_rcv ( nbp );
	}

	if ( DNS_PORT == ntohs(udp->dport) ) {
	    dns_rcv ( nbp );
	}

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

/* THE END */
