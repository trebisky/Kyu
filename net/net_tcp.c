/* net_tcp.c
 * Handle a TCP packet.
 * T. Trebisky  4-20-2005
 */

#include <kyu.h>
#include <kyulib.h>
#include <malloc.h>

#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

extern unsigned long my_ip;

typedef void (*tcp_fptr) ( char *, int );

static void tcp_reply_rst ( struct netbuf * );
struct tcp_io * tcp_connect ( unsigned long, int, tcp_fptr );
void tcp_send ( struct tcp_io *, char *, int );
void tcp_shutdown ( struct tcp_io * );

static void tcp_send_flags ( struct tcp_io *, int );

/* This structure manages TCP connections */
enum tcp_state { FREE, BORASCO, CWAIT, CONNECT, FWAIT };

struct tcp_io {
	struct tcp_io *next;
	unsigned long remote_ip;
	int remote_port;
	int local_port;
	unsigned long local_seq;
	unsigned long remote_seq;
	tcp_fptr rcv_func;
	enum tcp_state state;
	struct sem *sem;
};

static struct tcp_io *tcp_io_head;
static struct tcp_io *tcp_io_free_list;

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
#ifdef notdef
	unsigned char hlen:4,	/* header length (in words) (off in BSD) */
			_pad0:4;
#endif
	unsigned char flags;	/* 6 bits defined */
	unsigned short win;	/* advertised window */
	/* - */
	unsigned short sum;
	unsigned short urg;	/* urgent pointer */
};

#define DEF_WIN	2000

/* Here's the flags */
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* XXX - Someday slab allocator will handle this */
struct tcp_io *
tcp_io_alloc ( void )
{
	struct tcp_io *rv;

	if ( tcp_io_free_list ) {
	    rv = tcp_io_free_list;   
	    tcp_io_free_list = rv->next;
	} else {
	    rv = (struct tcp_io *) malloc ( sizeof(struct tcp_io) );
	}

	return rv;
}

void
tcp_io_free ( struct tcp_io *tp )
{
	tp->next = tcp_io_free_list;
	tp->state = FREE;
	tcp_io_free_list = tp;
}

void
tcp_tester ( char *buf, int count )
{
	char zoot[32];

	printf ( "%d byes of data delivered !!\n", count );

	/* for Daytime */
	strncpy ( zoot, buf, 24 );
	zoot[24] = '\0';
	printf ( "Received: %s\n", zoot );
}

#define	TEST_SERVER	"192.168.0.5"
#define	ECHO_PORT	7	/* echo */
#define	DAYTIME_PORT	13	/* daytime */
#define	TEST_PORT	DAYTIME_PORT

static void
test_tcp_daytime ( int dummy )
{
	unsigned long ip;
	struct tcp_io *tio;

	(void) net_dots ( TEST_SERVER, &ip );
	tio = tcp_connect ( ip, TEST_PORT, tcp_tester );
}

static void
test_tcp_echo ( int dummy )
{
	unsigned long ip;
	struct tcp_io *tio;
	char buf[16];

	(void) net_dots ( TEST_SERVER, &ip );
	tio = tcp_connect ( ip, TEST_PORT, tcp_tester );

	strcpy ( buf, "test" );
	tcp_send ( tio, buf, 4 );

	tcp_shutdown ( tio );
}

/* This gets called from the test console */
void
test_tcp ( int dummy )
{
	test_tcp_daytime ( dummy );
}

/* XXX - this gets called, but has no purpose yet.
 * Use for debug and ultimately initialization.
 */
void
tcp_init ( void )
{
	tcp_io_head = NULL;
	tcp_io_free_list = NULL;
}

static struct tcp_io *
tcp_io_lookup ( struct netbuf *nbp )
{
	struct tcp_io *tp;
	struct tcp_hdr *tcp;
	int sport, dport;

#ifdef notdef
	if ( ! tcp_io_head )
	    return NULL;
#endif

	tcp = (struct tcp_hdr *) nbp->pptr;
	sport = ntohs(tcp->sport);
	dport = ntohs(tcp->dport);

	for ( tp =  tcp_io_head; tp; tp = tp->next ) {
	    if ( tp->remote_ip != nbp->iptr->src )
		continue;
	    if ( tp->remote_port != sport )
		continue;
	    if ( tp->local_port != dport )
		continue;
	    return tp;
	}

	return NULL;
}

/* We are here when we have identified a receive packet
 * with a tcp_io object we know about.
 */
static void
tcp_io_receive ( struct tcp_io *tp, struct netbuf *nbp )
{
	unsigned long seq;
	struct tcp_hdr *tcp;
	int len;

	printf ( "Packet received for connection\n" );

	tcp = (struct tcp_hdr *) nbp->pptr;
	len = ntohs(nbp->iptr->len) - sizeof(struct ip_hdr) - tcp->hlen * 4;

	/* should keep state in tp
	 * check if SYN/ACK for this.
	 */
	if ( tcp->flags == (TH_SYN | TH_ACK) ) {
	    if ( tp->state == CWAIT ) {
		tp->remote_seq = ntohl(tcp->seq) + 1;
		tp->local_seq += 1;
		tcp_send_flags ( tp, TH_ACK );
		printf ( "Connection established\n" );
		tp->state = CONNECT;
		sem_unblock ( tp->sem );
	    }
	    return;
	}

	if ( tcp->flags == (TH_FIN | TH_ACK) ) {
	    tp->remote_seq = ntohl(tcp->seq) + 1;
	    if ( tp->state == FWAIT ) {
		tcp_send_flags ( tp, TH_ACK );
		printf ( "Connection termination complete\n" );
		tp->state = BORASCO;
		tcp_io_free ( tp );
	    }
	    if ( tp->state == CONNECT ) {
		tcp_send_flags ( tp, TH_FIN | TH_ACK );
		printf ( "Connection terminated by peer\n" );
		tp->state = BORASCO;
		tcp_io_free ( tp );
	    }
	    return;
	}

	if ( len > 0 ) {
	    printf ( "Data packet\n" );
	    tp->rcv_func ( nbp->dptr, len );
	}
}

/* Call this to send payload data to remote */
void
tcp_send ( struct tcp_io *tp, char *buf, int count )
{
}

/* Call this when we want to shut down a connection.
 * Unlike making a connection, we don't sweat any
 * kind of synchronization.
 */
void
tcp_shutdown ( struct tcp_io *tp )
{
	printf ( " Shutdown connection\n" );
	tcp_send_flags ( tp, TH_FIN | TH_ACK );
	tp->state = FWAIT;
}

static void dump_tcp ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	int ilen;
	int len;

	tcp = (struct tcp_hdr *) nbp->pptr;

	printf ( "TCP from %s: src/dst = %d/%d, size = %d",
		ip2strl ( nbp->iptr->src ), ntohs(tcp->sport), ntohs(tcp->dport), nbp->plen );
	printf ( " hlen = %d (%d bytes), seq = %lu (%08x), ack = %lu (%08x)\n",
		tcp->hlen, tcp->hlen * 4,
		ntohl(tcp->seq), ntohl(tcp->seq),
		ntohl(tcp->ack), ntohl(tcp->ack) );

	ilen = ntohs(nbp->iptr->len);
	len = ilen - sizeof(struct ip_hdr) - tcp->hlen * 4;
	printf ( " dlen = %d, ilen = %d, len = %d\n", nbp->dlen, ilen, len );
}

struct tcp_io *
tcp_connect ( unsigned long ip, int port, tcp_fptr rfunc )
{
	struct tcp_io *tp;

	tp = tcp_io_alloc ();
	if ( ! tp )
	    return NULL;

	tp->remote_ip = ip;
	tp->remote_port = port;
	tp->local_port = get_ephem_port ();
	tp->rcv_func = rfunc;
	tp->local_seq = 0;	/* XXX */
	tp->remote_seq = 0;	/* for now */
	tp->state = CWAIT;

	tp->next = tcp_io_head;
	tp->state = CWAIT;
	tp->sem = sem_signal_new ( SEM_FIFO );
	tcp_io_head = tp;

	tcp_send_flags ( tp, TH_SYN );
	sem_block ( tp->sem );

	/* XXX - this should block till we have a connection */

	return tp;
}


/* Called with every received TCP packet */
void
tcp_rcv ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	struct tcp_io *tp;
	int tlen;

	tcp = (struct tcp_hdr *) nbp->pptr;
	tlen = tcp->hlen * 4;
	nbp->dlen = nbp->plen - tlen;
	nbp->dptr = nbp->pptr + tlen;

#ifdef DEBUG_TCP
	dump_tcp ( nbp );
#endif

	/* At this point, we aren't a gateway, so drop the packet */
	if ( nbp->iptr->dst != my_ip ) {
	    printf ("TCP packet is not for us - dropped\n" );
	    netbuf_free ( nbp );
	    return;
	}

	tp = tcp_io_lookup ( nbp );
	if ( tp ) {
	    tcp_io_receive ( tp, nbp );
	    netbuf_free ( nbp );
	    return;
	}

	if ( tcp->flags == TH_SYN ) {
	    printf ( "Sending TCP rst/ack\n");
	    tcp_reply_rst ( nbp );
	    netbuf_free ( nbp );
	    return;
	}

	/* otherwise, silently drop packet */
	netbuf_free ( nbp );
}

struct tcp_pseudo {
	unsigned long src;
	unsigned long dst;
	unsigned short proto;
	unsigned short len;
};

static unsigned long seq_start = 99;

static void
tcp_reply_rst ( struct netbuf *xbp )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_hdr *xtcp;
	struct ip_hdr *xipp;
	unsigned long seq;
	struct tcp_pseudo pseudo;
	unsigned short sum;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->dlen = 0;
	nbp->plen = nbp->dlen + sizeof(struct tcp_hdr);
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	memset ( nbp->pptr, 0, sizeof(struct tcp_hdr) );

	nbp->iptr->proto = IPPROTO_TCP;

	tcp = (struct tcp_hdr *) nbp->pptr;

	/* flip the ports in the reply packet */
	xtcp = (struct tcp_hdr *) xbp->pptr;
	tcp->sport = xtcp->dport;
	tcp->dport = xtcp->sport;

	tcp->flags = TH_RST | TH_ACK;

	seq = ntohl(xtcp->seq) + 1;
	tcp->ack = htonl(seq);
	// tcp->seq = htonl(seq_start);
	tcp->seq = 0;

	tcp->hlen = 5;

	tcp->win = DEF_WIN;
	tcp->urg = 0;

	xipp = xbp->iptr;

	/* Setup and checksum the pseudo header */
	pseudo.src = xipp->dst;
	pseudo.dst = xipp->src;

	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );

	/* Note - we do NOT byte reorder the checksum.
	 * if it was done on byte swapped data, it will be correct
	 * as is !!
	 */
	tcp->sum = ~sum;

	/*
	net_debug_arm ();
	*/
	ip_send ( nbp, xipp->src );
}

static void
tcp_send_flags ( struct tcp_io *tp, int flags )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_pseudo pseudo;
	unsigned short sum;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->dlen = 0;
	nbp->plen = nbp->dlen + sizeof(struct tcp_hdr);
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	memset ( nbp->pptr, 0, sizeof(struct tcp_hdr) );

	nbp->iptr->proto = IPPROTO_TCP;

	tcp = (struct tcp_hdr *) nbp->pptr;

	tcp->sport = htons(tp->local_port);
	tcp->dport = htons(tp->remote_port);

	tcp->flags = flags;

	tcp->ack = htonl(tp->remote_seq);

	tcp->seq = htonl(tp->local_seq + 1);

	tcp->hlen = 5;

	tcp->win = DEF_WIN;
	tcp->urg = 0;

	/* Setup and checksum the pseudo header */
	pseudo.src = my_ip;
	pseudo.dst = tp->remote_ip;

	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );

	/* Note - we do NOT byte reorder the checksum.
	 * if it was done on byte swapped data, it will be correct
	 * as is !!
	 */
	tcp->sum = ~sum;

	/*
	net_debug_arm ();
	*/
	ip_send ( nbp, tp->remote_ip );
}

#ifdef no_way
void tcp_demo ( void );

/* Here is a TCP packet with a valid checksum.
 * 54 bytes
 */

static char tcp_demo_b[] = {
    0xc8,0x60,0x00,0x36,0xc7,0xdb,
    0x90,0x2b,0x34,0x37,0x55,0xe2,
    0x08,0x00,
    0x45,0x00,0x00,0x28,0xc2,0x6d,0x40,0x00,0x40,0x06,0xf7,0x06,
    0xc0,0xa8,0x00,0x05,
    0xc0,0xa8,0x00,0x06,
    0x00,0x21, 0xc3,0xd7,
    0x00,0x00,0x00,0x00,
    0xa8,0xc3,0x02,0x52,
    0x50,0x14, 0x00,0x00,
    0xbf,0x66,		/* <--- checksum */
    0x00,0x00
};

void
tcp_demo ( void )
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_pseudo pseudo;
	unsigned short sum;

	printf ( "Demo packet is %d bytes\n", sizeof(tcp_demo_b) );

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	memcpy ( nbp->eptr, tcp_demo_b, sizeof(tcp_demo_b) );
	nbp->elen = sizeof(tcp_demo_b);

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->ilen = nbp->elen - sizeof(struct eth_hdr);
	nbp->plen = nbp->ilen - sizeof(struct ip_hdr);
	nbp->dlen = nbp->plen - sizeof(struct tcp_hdr);

	printf ( "Demo packet has %d bytes of payload\n", nbp->dlen );

	tcp = (struct tcp_hdr *) nbp->pptr;
	printf ( "good tcp checksum is %04x\n", ntohs ( tcp->sum ) );

	/* Setup and checksum the pseudo header */
	pseudo.src = nbp->iptr->src;
	pseudo.dst = nbp->iptr->dst;

	/* shorts -- should byte swap */
	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	tcp->sum = 0;

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );
	sum = ~sum;
	printf ( "my tcp checksum is %04x\n", htons(sum) );

	tcp->sum = sum;

	ip_arp_send ( nbp );
}
#endif

/* THE END */
