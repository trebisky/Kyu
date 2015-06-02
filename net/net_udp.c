/* net_udp.c
 * Handle an UDP packet.
 * T. Trebisky  4-11-2005
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#define BOOTP_PORT	67
#define BOOTP_PORT2	68
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
	bip->src = my_ip;

	udp->sum = in_cksum ( nbp->iptr, nbp->ilen );

	ip_send ( nbp, dest_ip );
}

void bootp_send ( void );

void
udp_test ( int arg )
{
    	bootp_send ();
}

/* flag to tell server to reply via broadcast
 * (else replies unicast)
 */
#define	F_BROAD		0x8000

#define BOOTP_SIZE	300
#define DHCP_SIZE	548

struct bootp {
    	unsigned char op;
    	unsigned char htype;
    	unsigned char hlen;
    	unsigned char hops;
	/* -- */
	unsigned long id;
	/* -- */
	unsigned short time;
	unsigned short flags;	/* only for DHCP */ 
	/* -- */
	unsigned long client_ip;
	unsigned long your_ip;
	unsigned long server_ip;
	unsigned long gateway_ip;
	/* -- */
	char haddr[16];
	char server_name[64];
	char bootfile[128];
#ifdef notdef
	char vendor[64];	/* BOOTP */
#endif
	char vendor[312];	/* DHCP */
};

/* dhcp expands vendor field to 312 from 64 bytes.
 * so a bootp packet is 300 bytes, DHCP is 548
 */

void
bootp_send ( void )
{
	struct bootp bootp;

	memset ( (char *) &bootp, 0, BOOTP_SIZE );

	bootp.op = 1;
	bootp.htype = 1;
	bootp.hlen = ETH_ADDR_SIZE;
	bootp.hops = 0;

	bootp.id = 0;
	bootp.time = 0;
	bootp.flags = 0;	/* unicast reply */

	bootp.client_ip = 0;
	bootp.your_ip = 0;
	bootp.server_ip = 0;
	bootp.gateway_ip = 0;

	net_addr_get ( bootp.haddr );

	udp_send ( IP_BROADCAST, BOOTP_PORT2, BOOTP_PORT, (char *) &bootp, sizeof(struct bootp) );

	/*
	udp_send ( IP_BROADCAST, BOOTP_PORT2, BOOTP_PORT, (char *) &bootp, DHCP_SIZE );
	*/
}

void
bootp_rcv ( struct netbuf *nbp )
{
    	struct bootp *bpp;
	unsigned long me;
	char dst[20];

    	printf ("Received BOOTP/DHCP reply (%d bytes)\n", nbp->dlen );
	bpp = (struct bootp *) nbp->dptr;
	strcpy ( dst, ether2str ( nbp->eptr->dst ) );
	printf (" Server %s (%s) to %s\n",
		ip2strl ( bpp->server_ip ),
		ether2str ( nbp->eptr->src ), dst );
	printf (" gives my IP as: %s\n", ip2strl ( bpp->your_ip ) );
}

/* THE END */
