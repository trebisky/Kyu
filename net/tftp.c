/* tftp.c
 * T. Trebisky  9-12-2015
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#define TFTP_PORT	69
#define TFTP_SERVER	"192.168.0.5"

/* Note that the TFTP codes fit neatly in a byte,
 * even though the protocol gives them 2 bytes to fit in.
 * I take advantage of this to about byte swapping calls.
 */
#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERR	5

extern unsigned long my_ip;
static unsigned long tftp_ip;

void tftp_rcv ( struct netbuf * );
void tftp_fetch ( char *, char *, int );
void tftp_start ( char * );

/* ------------- */

#define TEST_SIZE	200000
static char *test_file = "kyu.bin";
static char test_buf[TEST_SIZE];

/* Called from test.c */
void
test_tftp ( int arg )
{
	tftp_fetch ( test_file, test_buf, TEST_SIZE );
}

/* ------------- */

static int local_port;		/* my ephemeral port */
static int server_port;		/* servers ephemeral port */

void
tftp_ack ( int rblock )
{
	char ack_buf[16];
	char *p;
	unsigned short rb;

	p = ack_buf;
	*p++ = 0;
	*p++ = TFTP_ACK;

	rb = rblock;
	memcpy ( p, (char *) &rb, 2 );

	/*
	printf ( "TFTP sending Ack: " );
	dump_buf ( ack_buf, 4 );
	*/

	udp_send ( tftp_ip, local_port, server_port, ack_buf, 4 );
}

void
tftp_rcv ( struct netbuf *nbp )
{
	int code;
	int block;
	unsigned short rblock;

	/*
    	printf ("Received TFTP packet (%d bytes)\n", nbp->dlen );
	*/

	code = nbp->dptr[1];
	if ( code == TFTP_ERR ) {
	    dump_buf ( nbp->dptr, nbp->dlen );
	    printf ( "TFTP error: %s\n", &nbp->dptr[4] );
	    return;
	}

	if ( code == TFTP_DATA ) {
	    memcpy ( &rblock, &nbp->dptr[2], 2 );
	    block = ntohs ( rblock );
	    if ( block == 1 ) {
		server_port = udp_get_sport ( nbp );
		printf ( "TFTP server at port %d\n", server_port );
	    }

	    printf ( "TFTP data block %d (%d bytes)\n", block, nbp->dlen - 4 );
	    tftp_ack ( rblock );
	    return;
	}

	printf ("TFTP unexpected: %d\n", code );
}

void
tftp_init ( void )
{
	 if ( ! net_dots ( TFTP_SERVER, &tftp_ip ) )
	     panic ( "tftp netdots" );
}

void
tftp_fetch ( char *file, char *buf, int limit )
{
	local_port = get_ephem_port ();

	printf ( "TFTP listening on port %d\n", local_port );
	udp_hookup ( local_port, tftp_rcv );
	tftp_start ( file );
}

/* Set up and send RRQ packet to start a request */
void
tftp_start ( char *file )
{
	char rrq_buf[64];
	char *p;

	p = rrq_buf;
	*p++ = 0;
	*p++ = TFTP_RRQ;
	strcpy ( p, file );
	p += strlen ( file ) + 1;
	strcpy ( p, "octet" );
	p += strlen ( "octet" ) + 1;

	udp_send ( tftp_ip, local_port, TFTP_PORT, rrq_buf, p - rrq_buf );
}

/* THE END */
