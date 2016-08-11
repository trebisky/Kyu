/* dhcp.c
 * T. Trebisky  8-10-2016
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#include "dhcp.h"

#define OP_PAD		0
#define OP_END		0xff

#define OP_REQIP	50	/* Requested IP */

#define OP_TYPE		53	/* Message type */
#define OP_SERVER	54	/* Server IP */
#define OP_PARAM	55	/* Parameter request */

#define TYPE_DISCOVER	1
#define TYPE_OFFER	2
#define TYPE_REQUEST	3
#define TYPE_DECLINE	4
#define TYPE_ACK	5
#define TYPE_NACK	6
#define TYPE_RELEASE	7
#define TYPE_INFORM	8

void dhcp_rcv ( struct netbuf * );
void dhcp_discover ( void );
void dhcp_request ( void );

/* DHCP is a "refinement" of BOOTP and is indicated
 * by a magic cookie value at the start of the options area
 */
static char cookie[] = { 0x63, 0x82, 0x53, 0x63 };

/* The game goes like this.
 * 4 packets get exchanged in the simplest case.
 * 2 are sent (broadcase) 2 are received.
 *
 * First the client (us) broadcasts a DHCP discover.
 * Then it waits a while for servers to respond.
 * One or more servers send a DHCP offer.
 * The client (us) picks one, perhaps the first one.
 * The client broadcasts a DHCP request that contains the chosen address.
 * The server should then send a DHCP ACK.
 */

static char *
option_add ( char *op, int val, int size, char *data )
{
	*op++ = val;
	*op++ = size;
	memcpy ( op, data, size );
	return op + size;
}

static char *
option_add_1 ( char *op, int val, int data )
{
	*op++ = val;
	*op++ = 1;
	*op++ = data;
	return op;
}

static char *
option_add_2 ( char *op, int val, int d1, int d2 )
{
	*op++ = val;
	*op++ = 2;
	*op++ = d1;
	*op++ = d2;
	return op;
}

int our_xid;

void
dhcp_discover ( void )
{
	struct bootp dhcp;
	char *op;

	memset ( (char *) &dhcp, 0, sizeof(struct bootp) );

	dhcp.op = BOOTP_REQUEST;
	dhcp.htype = BOOTP_ETHER;
	dhcp.hlen = ETH_ADDR_SIZE;
	dhcp.hops = 0;

	our_xid = 0xabcd1234;		/* XXX should be random value */

	dhcp.xid = our_xid;
	dhcp.secs = 10;			/* XXX should be how many seconds we've been trying */
	dhcp.flags = 0;			/* ignored by bootp */

	dhcp.client_ip = 0;
	dhcp.your_ip = 0;
	dhcp.server_ip = 0;
	dhcp.gateway_ip = 0;

	net_addr_get ( dhcp.chaddr );	/* client MAC address (allows server to bypass ARP to reply */

	op = dhcp.options;
	memcpy ( dhcp.options, cookie, 4 );
	op += 4;
	op = option_add_1 ( op, OP_TYPE, TYPE_DISCOVER );
	op = option_add_2 ( op, OP_PARAM, 1, 3 );
	*op++ = OP_END;

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &dhcp, sizeof(struct bootp) );
}

/* Stuff collected from OFFER packet */
static unsigned int server_ip1;
static unsigned int your_ip;
static unsigned int server_ip2;

void
dhcp_request ( void )
{
	struct bootp dhcp;
	char *op;

	memset ( (char *) &dhcp, 0, sizeof(struct bootp) );

	dhcp.op = BOOTP_REQUEST;
	dhcp.htype = BOOTP_ETHER;
	dhcp.hlen = ETH_ADDR_SIZE;
	dhcp.hops = 0;

	dhcp.xid = our_xid;
	dhcp.secs = 10;			/* XXX should be how many seconds we've been trying */
	dhcp.flags = 0;			/* ignored by bootp */

	dhcp.client_ip = 0;
	dhcp.your_ip = 0;
	dhcp.server_ip = server_ip1;	/* *** */
	dhcp.gateway_ip = 0;

	net_addr_get ( dhcp.chaddr );	/* client MAC address (allows server to bypass ARP to reply */

	op = dhcp.options;
	memcpy ( dhcp.options, cookie, 4 );
	op += 4;
	op = option_add_1 ( op, OP_TYPE, TYPE_REQUEST );
	op = option_add ( op, OP_REQIP, 4, (char *) &your_ip );
	op = option_add ( op, OP_SERVER, 4, (char *) &server_ip2 );
	*op++ = OP_END;

	udp_broadcast ( BOOTP_CLIENT, BOOTP_SERVER, (char *) &dhcp, sizeof(struct bootp) );
}

static void
show_options ( struct bootp *bpp )
{
	char *op = bpp->options + 4;

	while ( *op != OP_END ) {
	    if ( *op == 0 ) {
		printf ( "Pad\n" ); op++;
		continue;
	    }
	    printf ( "Option %02x, len %d\n", *op, op[1] );
	    op += 2 + op[1];
	}
}

static char *
find_option ( struct bootp *bpp, int key )
{
	char *op = bpp->options + 4;

	while ( *op != OP_END ) {
	    if ( *op == 0 )
		op++;
	    else {
		if ( *op == key )
		    return op;
		op += 2 + op[1];
	    }
	}
}

static void
dhcp_show_pkt ( struct bootp *bpp, struct netbuf *nbp )
{
	char dst[20];
	char *p;

	p = find_option ( bpp, OP_TYPE );
	printf ("Received DHCP reply (%d bytes), type = %d\n", nbp->dlen, p[2] );

	strcpy ( dst, ether2str ( nbp->eptr->dst ) );
	printf (" Server %s (%s) to %s (xid = %08x)\n",
	    ip2strl ( bpp->server_ip ),
	    ether2str ( nbp->eptr->src ), dst,
	    bpp->xid );

	show_options ( bpp );
}

static int dhcp_debug;
static int dhcp_ip;

static int dhcp_state;

#define ST_OWAIT1	1
#define ST_OWAIT2	2
#define ST_AWAIT1	3
#define ST_AWAIT2	4

/* packet evaporates once we return,
 * so we copy things out of it as needed
 */
void
dhcp_rcv ( struct netbuf *nbp )
{
    	struct bootp *bpp;
	char *p;
	int type;

	bpp = (struct bootp *) nbp->dptr;

	dhcp_show_pkt ( bpp, nbp );

	if ( bpp->xid != our_xid ) {
	    printf ("Bad xid, ingnoring packet\n" );
	    return;
	}

	p = find_option ( bpp, OP_TYPE );
	type = p[2];

	if ( type == TYPE_OFFER && dhcp_state == ST_OWAIT1 ) {
	    /* XXX - what about byte order?? */
	    server_ip1 = bpp->server_ip;
	    your_ip = bpp->your_ip;
	    p = find_option ( bpp, OP_SERVER );
	    memcpy ( &server_ip2, &p[2], 4 );
	    printf ( "OFFER your ip = %s\n", ip2strl ( your_ip ) );
	    printf ( "OFFER server ip 1 = %s\n", ip2strl ( server_ip1 ) );
	    printf ( "OFFER server ip 2 = %s\n", ip2strl ( server_ip2 ) );

	    dhcp_state = ST_OWAIT2;
	}
	else if ( type == TYPE_ACK && dhcp_state == ST_AWAIT1 ) {
	    dhcp_ip = bpp->your_ip;		/* many places to get this now */
	    // Other good things we could harvest ...
	    // p = find_option ( bpp, 54 );	/* Server */
	    // p = find_option ( bpp, 51 );	/* Lease time (seconds) 4 bytes */
	    // p = find_option ( bpp, 1 );	/* Subnet mask 4 bytes */
	    // p = find_option ( bpp, 3 );	/* Router IP (gateway) */
	    // p = find_option ( bpp, 6 );	/* DNS IP (maybe 8 bytes for two) */
	    // p = find_option ( bpp, 12 );	/* host name (3 bytes for "kyu") */
	    // p = find_option ( bpp, 15 );	/* domain name (8 bytes for "mmto.org") */

	    dhcp_state = ST_AWAIT2;
	}
	else
	    printf ( "Ignoring packet\n" );

#ifdef notdef
	dhcp_ip = bpp->your_ip;

	if ( dhcp_debug ) {
	    printf (" gives my IP as: %s\n", ip2strl ( dhcp_ip ) );
	}
#endif
}

#define DHCP_RETRIES	4

static int
dhcp_once ( void )
{
	dhcp_state = ST_OWAIT1;
	dhcp_discover ();

	/* XXX - this would be an ideal place for a semaphore with a timeout */
	thr_delay ( 1000 );	/* wait for offers */
	if ( dhcp_state != ST_OWAIT2 )
	    return 0;

	dhcp_state = ST_AWAIT1;
	dhcp_request ();

	/* XXX - ditto on the semaphore*/
	thr_delay ( 1000 );	/* wait for ACK */
	if ( dhcp_state != ST_AWAIT2 )
	    return 0;

	return dhcp_ip;
}

static int
dhcp_get ( void )
{
	int i;

	udp_hookup ( BOOTP_CLIENT, dhcp_rcv );
	dhcp_ip = 0;

	for ( i=0; i<DHCP_RETRIES; i++ ) {
	    if ( dhcp_once () )
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
