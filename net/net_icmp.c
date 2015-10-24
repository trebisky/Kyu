/* net_icmp
 * Handle an ICMP packet.
 * T. Trebisky  4-8-2005
 */

#include <kyu.h>
#include <kyulib.h>

#include "net.h"
#include "netbuf.h"
#include "cpu.h"

/* bit fields work on little endian */
struct icmp_hdr {
	unsigned char	type;
	unsigned char	code;
	unsigned short sum;
	/* - */
	unsigned short id;
	unsigned short seq;
};

#define TY_ECHO_REPLY	0
#define TY_ECHO_REQ	8

static void icmp_reply ( struct netbuf *, int );

/* Pull the arp information out of the ICMP
 * request packet -- nasty and shouldn't but ...
 */
static void
icmp_arp_evil ( struct netbuf *nbp )
{
    	arp_save_icmp ( nbp->eptr->src, (unsigned char *) &nbp->iptr->src );
}

void
icmp_rcv ( struct netbuf *nbp )
{
	struct icmp_hdr *icp;
	int sum;
	int len;

	icp = (struct icmp_hdr *) nbp->pptr;
	len = ntohs(nbp->iptr->len) - sizeof(struct ip_hdr);
	sum = in_cksum ( (char *) icp, len);

	/*
	printf ( "ICMP plen = %d, csum = %04x\n", nbp->plen, in_cksum ( nbp->pptr, nbp->plen ) );
	printf ( "ICMP ilen = %d, csum = %04x\n", len, in_cksum ( nbp->pptr, len ) );
	*/

#ifdef DEBUG_ICMP
	if ( icp->type == TY_ECHO_REQ ) {
	    printf ( "ICMP echo request: code = %d, id/seq = %d/%d\n", icp->code, icp->id, icp->seq );
	} else if ( icp->type == TY_ECHO_REPLY ) {
	    printf ( "ICMP echo reply: code = %d, id/seq = %d/%d\n", icp->code, icp->id, icp->seq );
	} else {
	    printf ( "ICMP packet type = %d, code = %d\n", icp->type, icp->code );
	}

	if ( sum )
	    printf ( "ICMP checksum (incorrect) = %04x\n", sum );
#endif

	if ( icp->type == TY_ECHO_REQ ) {
	    /*
	    printf ( "Sending reply\n" );
	    */
	    icmp_arp_evil ( nbp );
	    icmp_reply ( nbp, len );
	    /* XXX - special, do NOT free the netbuf,
	     * as it is being recycled to handle the reply.
	     */
	} else {
	    netbuf_free ( nbp );
	}
}

/* Funky short cut, modify the packet and return it
 */
static void
icmp_reply ( struct netbuf *nbp, int len )
{
	struct icmp_hdr *icp;

	icp = (struct icmp_hdr *) nbp->pptr;
	icp->type = TY_ECHO_REPLY;
	icp->sum = 0;
	icp->sum = in_cksum ( (char *) icp, len );
	ip_reply ( nbp );
}

static int icmp_id = 1;

void
icmp_ping ( unsigned long target_ip )
{
	struct netbuf *nbp;
	struct icmp_hdr *icp;
	int size;

	/* get a netbuf for this */
	if ( ! (nbp = netbuf_alloc ()) )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct icmp_hdr );

	nbp->iptr->proto = IPPROTO_ICMP;

	/* load with junk */
	size = 56;
	memset ( nbp->dptr, 0xab, size );

	size += sizeof(struct icmp_hdr);
	nbp->plen = size;
	nbp->ilen = size + sizeof(struct ip_hdr);

	icp = (struct icmp_hdr *) nbp->pptr;

	icp->type = TY_ECHO_REQ;
	icp->code = 0;
	icp->id = icmp_id++;
	icp->sum = 0;
	icp->sum = in_cksum ( (char *) icp, nbp->plen );
	/* XXX */

	ip_send ( nbp, target_ip );
}

/* Argument in host order */
void
ping ( unsigned long arg )
{
    	icmp_ping ( htonl(arg) );
}

/* THE END */