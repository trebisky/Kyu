/* bootp.c
 * T. Trebisky  4-11-2005
 * T. Trebisky  6-2-2015
 */

#include "kyulib.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#define BOOTP_PORT	67	/* receive on this */
#define BOOTP_PORT2	68	/* send on this */

void bootp_rcv ( struct netbuf * );
void bootp_send ( void );

extern unsigned long my_ip;

void
bootp_test ( int arg )
{
	int i;

	for ( i=0; i<100; i++ ) {
	    bootp_send ();
	    thr_delay ( 10 );
	}
}

/* DHCP is a "refinement" of BOOTP and is triggered
 * by a magic cookie value at the start of the options area
 */

/* flag to tell server to reply via broadcast
 * (else replies unicast)
 */
#define	F_BROAD		0x8000

#define BOOTP_SIZE	300
#define DHCP_SIZE	548

#define BOOTP_REQUEST   1
#define BOOTP_REPLY     2

#define BOOTP_ETHER	1

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
	char options[64];	/* BOOTP */
#endif
	char options[312];	/* DHCP */
};

/* dhcp expands options (vendor) field to 312 from 64 bytes.
 * so a bootp packet is 300 bytes, DHCP is 548
 */

void
bootp_send ( void )
{
	struct bootp bootp;
	unsigned long save;

	memset ( (char *) &bootp, 0, sizeof(struct bootp) );

	bootp.op = BOOTP_REQUEST;
	bootp.htype = BOOTP_ETHER;
	bootp.hlen = ETH_ADDR_SIZE;
	bootp.hops = 0;

	bootp.id = 0;
	bootp.time = 0;
	bootp.flags = 0;	/* we want unicast reply */

	bootp.client_ip = 0;
	bootp.your_ip = 0;
	bootp.server_ip = 0;
	bootp.gateway_ip = 0;

	net_addr_get ( bootp.haddr );

	/* XXX - goofy hack, BOOTP works if we send our IP, but we are really
	 * supposed to set the source IP field to zero
	 */
	save = my_ip;
	my_ip = 0;

	udp_send ( IP_BROADCAST, BOOTP_PORT2, BOOTP_PORT, (char *) &bootp, sizeof(struct bootp) );

	my_ip = save;
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
