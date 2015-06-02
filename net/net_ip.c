/* net_ip
 * Handle an IP packet.
 * T. Trebisky  3-21-2005
 */

#include "net.h"
#include "netbuf.h"
#include "cpu.h"

extern unsigned long my_ip;

void
ip_rcv ( struct netbuf *nbp )
{
	struct ip_hdr *ipp;
	int cksum;

	cksum = in_cksum ( nbp->iptr, sizeof(struct ip_hdr) );
	ipp = nbp->iptr;

	if ( cksum ) {
	    printf ( "bad IP packet from %s (%d) proto = %d, sum= %04x\n",
		    ip2strl ( ipp->src ), nbp->ilen, ipp->proto, cksum );
	    netbuf_free ( nbp );
	    return;
	}

	if ( ipp->offset & IP_OFFMASK_SWAP ) {
	    printf ( "Fragmented (%04x) IP packet from %s (%d) proto = %d, sum= %04x\n",
		    ipp->offset, ip2strl ( ipp->src ), nbp->ilen, ipp->proto, cksum );
	    netbuf_free ( nbp );
	    return;
	}

	nbp->pptr = (char *) nbp->iptr + sizeof(struct ip_hdr);
	nbp->plen = nbp->ilen - sizeof(struct ip_hdr);

	/* XXX - verify that it is addressed to us, or to a broadcast */

	printf ( "IP packet: Source: %s", ip2strl ( ipp->src ) );
	printf ( "   " );
	printf ( "Dest: %s", ip2strl ( ipp->dst ) );
	printf ( "\n" );

	if ( ipp->proto == IPPROTO_ICMP ) {
	    /*
	    printf ( "ip_rcv: ilen, plen, len = %d %d %d\n", nbp->ilen, nbp->plen, ntohs(ipp->len) );
	    dump_buf ( nbp->iptr, nbp->ilen );
	    */
	    icmp_rcv ( nbp );
	} else if ( ipp->proto == IPPROTO_UDP ) {
	    udp_rcv ( nbp );
	    netbuf_free ( nbp );
	} else if ( ipp->proto == IPPROTO_TCP ) {
	    tcp_rcv ( nbp );
	} else {
	    printf ( "IP from %s (size:%d) proto = %d, sum= %04x\n",
		    ip2strl ( ipp->src ), nbp->plen, ipp->proto, ipp->sum );
	    netbuf_free ( nbp );
	}

#ifdef DEBUG_IP
	if ( ipp->proto == IPPROTO_ICMP ) {
	    printf ( "ICMP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else if ( ipp->proto == IPPROTO_UDP ) {
	    printf ( "UDP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else if ( ipp->proto == IPPROTO_TCP ) {
	    printf ( "TCP packet, size = %d, sum= %04x %04x", nbp->plen, ipp->sum, cksum );
	} else {
	    printf ( "IP packet (size:%d) proto = %d, sum= %04x", nbp->plen, ipp->proto, ipp->sum );
	}

	printf ( " Source: %s", ip2strl ( ipp->src ) );
	printf ( "   " );
	printf ( "Dest: %s", ip2strl ( ipp->dst ) );
	printf ( "\n" );
#endif
}

/* typically called to return an icmp packet.
 * (also to do a RST/ACK on a tcp connection.)
 * We take some significant short cuts here,
 *  recycling the netbuf already received.
 */
void
ip_reply ( struct netbuf *nbp )
{
	struct ip_hdr *ipp;
	unsigned long tmp;

	ipp = (struct ip_hdr *) nbp->iptr;

	tmp = ipp->src;
	ipp->src = ipp->dst;
	ipp->dst = tmp;

	ip_arp_send ( nbp );
}

static int ip_id = 1;

void
ip_send ( struct netbuf *nbp, unsigned long dest_ip )
{
	struct ip_hdr *ipp;

	ipp = (struct ip_hdr *) nbp->iptr;
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);

	ipp->hl = sizeof(struct ip_hdr) / sizeof(long);
	ipp->ver = 4;

	ipp->tos = 0;
	ipp->len = ntohs(nbp->ilen);

	ipp->id = ip_id++;
	ipp->offset = 0;

	ipp->ttl = 64;

#ifdef notdef
	/* Let the udp or tcp layer fill this in for us */
	ipp->proto = IPPROTO_UDP;
#endif

	ipp->src = my_ip;
	ipp->dst = dest_ip;

	ipp->sum = 0;
	ipp->sum = in_cksum ( (char *) ipp, sizeof ( struct ip_hdr ) );

	ip_arp_send ( nbp );
}

/* THE END */
