/* dhcp.c
 * T. Trebisky  8-10-2016
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#include "dhcp.h"

void dhcp_rcv ( struct netbuf * );
void dhcp_send ( void );

/* DHCP is a "refinement" of BOOTP and is triggered
 * by a magic cookie value at the start of the options area
 */

void
dhcp_send ( void )
{
	struct bootp dhcp;

	memset ( (char *) &dhcp, 0, sizeof(struct bootp) );

	dhcp.op = BOOTP_REQUEST;
	dhcp.htype = BOOTP_ETHER;
	dhcp.hlen = ETH_ADDR_SIZE;
	dhcp.hops = 0;

	dhcp.xid = 0xabcd1234;		/* XXX should be random value */
	dhcp.secs = 10;		/* XXX should be how many seconds we've been trying */
	dhcp.flags = 0;		/* ignored by bootp */

	dhcp.client_ip = 0;
	dhcp.your_ip = 0;
	dhcp.server_ip = 0;
	dhcp.gateway_ip = 0;

	net_addr_get ( dhcp.chaddr );	/* client MAC address (allows server to bypass ARP to reply */

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &dhcp, sizeof(struct bootp) );

}

static int dhcp_debug;
static int dhcp_ip;

void
dhcp_rcv ( struct netbuf *nbp )
{
    	struct bootp *bpp;
	char dst[20];

	if ( dhcp_debug )
	    printf ("Received DHCP reply (%d bytes)\n", nbp->dlen );

	bpp = (struct bootp *) nbp->dptr;
	dhcp_ip = bpp->your_ip;

	if ( dhcp_debug ) {
	    strcpy ( dst, ether2str ( nbp->eptr->dst ) );
	    printf (" Server %s (%s) to %s (xid = %08x)\n",
		ip2strl ( bpp->server_ip ),
		ether2str ( nbp->eptr->src ), dst,
		bpp->xid );
	    printf (" gives my IP as: %s\n", ip2strl ( dhcp_ip ) );
	}
}

#define DHCP_RETRIES	5

static int
dhcp_get ( void )
{
	int i;

	udp_hookup ( BOOTP_CLIENT, dhcp_rcv );
	dhcp_ip = 0;

	for ( i=0; i<DHCP_RETRIES; i++ ) {
	    dhcp_send ();
	    /* XXX - this would be an ideal place for a semaphore with a timeout */
	    thr_delay ( 1000 );
	    if ( dhcp_ip )
		break;
	}

	udp_unhook ( BOOTP_CLIENT );

	return dhcp_ip;
}

int
dhcp_get_ip ( void )
{
	dhcp_debug = 0;
	return dhcp_get ();
}

void
dhcp_test ( int arg )
{
	int ip;

	dhcp_debug = 1;

	ip = dhcp_get ();

	if ( ip == 0 )
	    printf ( "DHCP test failed\n" );
	else
	    printf ( "DHCP got IP of %s\n", ip2strl ( ip ) );
}

/* THE END */
