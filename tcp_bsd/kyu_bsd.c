/* kyu_bsd.c */

/* support code for BSD 4.4 TCP code in Kyu
 * BSD global variables are here.
 */

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"

// #include "../arch/cpu.h"

/* Kyu calls this during initialization */
void
tcp_bsd_init ( void )
{
}

void
tcp_show ( void )
{
    printf ( "Status for BSD tcp code:\n" );
    // ...
}

/* This is where arriving packets get delivered by Kyu
 *  From ip_rcv() in net/net_ip.c
 * Be sure and let Kyu free the packet.
 */
void
tcp_bsd_rcv ( struct netbuf *nbp )
{
	struct netpacket *pkt;

	// printf ( "TCP: bsd_rcv %d\n", ntohs(nbp->iptr->len) );

	/* Must give eptr since we may have PREPAD */
	pkt = (struct netpacket *) nbp->eptr;
	// ip_ntoh ( pkt );
	// tcp_ntoh ( pkt );

	// tcp_in ( pkt );
	/* do NOT free the packet here */
}

/* THE END */
