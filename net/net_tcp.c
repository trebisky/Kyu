/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * net_tcp.c
 * Handle a TCP packet.
 * T. Trebisky  4-20-2005
 */

#include <arch/types.h>
#include <kyu.h>
#include <kyulib.h>
#include <malloc.h>

#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

/* Priority at which connection threads are launched */
/* XXX - at this time the test thread does NOT use
 * interrupt serial IO, so it is a hog running at
 * priority 30, so we must launch these threads at
 * a lower priority or they are starved
 */
#define CONN_PRI	29

extern struct host_info host_info;

/* This structure manages TCP connections */
enum tcp_state { FREE, BORASCO, CWAIT, CONNECT, FWAIT };

/* One of these for every connection */
struct tcp_io {
	struct tcp_io *next;
	u32 remote_ip;
	int remote_port;
	int local_port;
	u32 local_seq;
	u32 remote_seq;
	void (*rcv_func) ( struct tcp_io *, char *, int);
	enum tcp_state state;
	struct sem *sem;
	struct tcp_server *server;
};

typedef void (*tcp_fptr) ( struct tcp_io *, char *, int );
typedef void (*tcp_cptr) ( struct tcp_io * );

struct tcp_server {
	struct tcp_server *next;
	int		port;
	tcp_cptr	conn_func;
	tcp_fptr 	rcv_func;
};

static void tcp_reply_rst ( struct netbuf * );
static void tcp_send_data ( struct tcp_io *, int, char *, int );

/* Public entry points */

void tcp_server ( int, tcp_cptr, tcp_fptr );
struct tcp_io * tcp_connect ( u32, int, tcp_fptr );
void tcp_send_pk ( struct tcp_io *, char *, int );
void tcp_shutdown ( struct tcp_io * );

static struct tcp_io *tcp_io_head;
static struct tcp_io *tcp_io_free_list;

static struct tcp_server *tcp_server_head;

/* XXX - bit fields are little endian only.
 * 20 bytes in all, but options may follow.
 */
struct tcp_hdr {
	unsigned short	sport;
	unsigned short	dport;
	/* - */
	u32 seq;
	u32 ack;
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

static void
tcp_tester ( struct tcp_io *tp, char *buf, int count )
{
	char zoot[32];

	printf ( "%d byes of data received !!\n", count );

	/* for Daytime */
	/*
	strncpy ( zoot, buf, 24 );
	zoot[24] = '\0';
	printf ( "Received: %s\n", zoot );
	*/
}

#define	TEST_SERVER	"192.168.0.5"
#define	ECHO_PORT	7	/* echo */
#define	DAYTIME_PORT	13	/* daytime */

static void
test_tcp_daytime ( void )
{
	u32 ip;
	struct tcp_io *tio;

	(void) net_dots ( TEST_SERVER, &ip );
	tio = tcp_connect ( ip, DAYTIME_PORT, tcp_tester );
}

static char echo_buf[1024];

static void
test_tcp_echo ( void )
{
	u32 ip;
	struct tcp_io *tio;
	int i;

	(void) net_dots ( TEST_SERVER, &ip );
	tio = tcp_connect ( ip, ECHO_PORT, tcp_tester );

	// strcpy ( echo_buf, "go\r\n" );
	memset ( echo_buf, 'x', 1024 );
	for ( i=0; i<16; i++ )
	    tcp_send_pk ( tio, echo_buf, 1024 );

	// tcp_shutdown ( tio );
}

/* This gets called when a new connection arrives on
 * whatever server we are testing.
 */
static void
test_connect ( struct tcp_io *tp )
{
	printf ( "Got a connection on port %d\n", tp->local_port );
}

/* This gets called from the test console */
void
test_kyu_tcp ( void )
{
	test_tcp_echo ();
	// test_tcp_daytime ();
}

/* For echo server */
static void
tcp_echo_rcv ( struct tcp_io *tp, char *buf, int count )
{
	char lbuf[64];

	strcpy ( lbuf, buf, count );
	lbuf[count] = '\0';
	printf ( "%d byes of data received by echo server: !!\n", count );
	printf ( "Here she is: %s\n", lbuf );
	tcp_send_pk ( tp, "ECHO\n\r", 6 );
}

void
tcp_kyu_init ( void )
{
	tcp_io_head = NULL;
	tcp_io_free_list = NULL;
	tcp_server_head = NULL;

	tcp_server ( ECHO_PORT, test_connect, tcp_echo_rcv );
}

static struct tcp_server *
tcp_server_lookup ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	struct tcp_server *sp;
	int dport;

	tcp = (struct tcp_hdr *) nbp->pptr;
	dport = ntohs(tcp->dport);
	printf ( "Doing server lookup on port %d\n", dport );

	for ( sp = tcp_server_head; sp; sp = sp->next ) {
	    if ( sp->port == dport )
		return sp;
	}

	return NULL;
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

/* Call this to send payload data to remote */
void
tcp_send_pk ( struct tcp_io *tp, char *buf, int count )
{
	tcp_send_data ( tp, TH_PUSH | TH_ACK, buf, count );
	tp->local_seq += count;
}

/* We are here when we have identified a receive packet
 * with a tcp_io object we know about.
 */
static void
tcp_io_receive ( struct tcp_io *tp, struct netbuf *nbp )
{
	u32 seq;
	struct tcp_hdr *tcp;
	int len;

	printf ( "Packet received for connection\n" );

	tcp = (struct tcp_hdr *) nbp->pptr;
	len = ntohs(nbp->iptr->len) - sizeof(struct ip_hdr) - tcp->hlen * 4;

	/* Get this in response to our sending a SYN to set up
	 * a client connection.
	 */
	if ( tcp->flags == (TH_SYN | TH_ACK) ) {
	    if ( tp->state == CWAIT ) {
		tp->remote_seq = ntohl(tcp->seq);
		tp->local_seq = ntohl(tcp->ack);
		tcp_send_data ( tp, TH_ACK, NULL, 0 );
		printf ( "Connection established\n" );
		tp->state = CONNECT;
		sem_unblock ( tp->sem );
	    }
	    return;
	}

	/* Somebody is shutting down the connection.
	 */
	if ( tcp->flags == (TH_FIN | TH_ACK) ) {
	    tp->remote_seq = ntohl(tcp->seq);
	    if ( tp->state == FWAIT ) {
		tcp_send_data ( tp, TH_ACK, NULL, 0 );
		printf ( "Connection termination complete\n" );
		tp->state = BORASCO;
		sem_destroy ( tp->sem );
		/* XXX - remove from list */
		tcp_io_free ( tp );
	    }
	    if ( tp->state == CONNECT ) {
		tcp_send_data ( tp, TH_FIN | TH_ACK, NULL, 0 );
		printf ( "Connection terminated by peer\n" );
		tp->state = BORASCO;
		sem_destroy ( tp->sem );
		/* XXX - remove from list */
		tcp_io_free ( tp );
	    }
	    return;
	}

	/* Somebody is shutting things down another way */
	if ( tcp->flags == TH_RST ) {
	    if ( tp->state == FWAIT ) {
		printf ( "Connection terminated by RST\n" );
		tp->state = BORASCO;
		sem_destroy ( tp->sem );
		tcp_io_free ( tp );
	    }
	    return;
	}

	/* Some kind of traffic */
	if ( tcp->flags & TH_ACK ) {
	    tp->remote_seq = ntohl(tcp->seq);
	    tp->local_seq = ntohl(tcp->ack);
	    if ( tp->state == CWAIT ) {
		printf ( "Connection established\n" );
		tp->state = CONNECT;
		sem_unblock ( tp->sem );
	    }
	    if ( len > 0 ) {
		tp->remote_seq += len - 1;
		printf ( "Data received ***\n" );
		tcp_send_data ( tp, TH_ACK, NULL, 0 );
		tp->rcv_func ( tp, nbp->dptr, len );
	    }
	    return;
	}

	printf ( "************** unexpected flags: %08x\n", tcp->flags );
}

/* Call this when we want to shut down a connection.
 * Unlike making a connection, we don't sweat any
 * kind of synchronization.
 */
void
tcp_shutdown ( struct tcp_io *tp )
{
	printf ( " Shutdown connection\n" );
	tcp_send_data ( tp, TH_FIN | TH_ACK, NULL, 0 );
	tp->state = FWAIT;
}

static void dump_tcp ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	int ilen;
	int len;

	tcp = (struct tcp_hdr *) nbp->pptr;

	printf ( "TCP from %s: src/dst = %d/%d, size = %d",
		ip2str32 ( nbp->iptr->src ), ntohs(tcp->sport), ntohs(tcp->dport), nbp->plen );
	printf ( " hlen = %d (%d bytes), seq = %lu (%08x), ack = %lu (%08x)\n",
		tcp->hlen, tcp->hlen * 4,
		ntohl(tcp->seq), ntohl(tcp->seq),
		ntohl(tcp->ack), ntohl(tcp->ack) );

	ilen = ntohs(nbp->iptr->len);
	len = ilen - sizeof(struct ip_hdr) - tcp->hlen * 4;
	printf ( " dlen = %d, ilen = %d, len = %d\n", nbp->dlen, ilen, len );
}

/* XXX - need a way to timeout a connection attempt.
 * currently this blocks forever if the server does not
 * respond.  There may be an ICMP message, such as from a firewall,
 * but we have nothing setup in our ICMP handler for that.
 * We may need a new primitive to block with a timeout.
 */
struct tcp_io *
tcp_connect ( u32 ip, int port, tcp_fptr rfunc )
{
	struct tcp_io *tp;

	tp = tcp_io_alloc ();
	if ( ! tp )
	    return NULL;

	tp->remote_ip = ip;
	tp->remote_port = port;
	tp->local_port = get_ephem_port ();
	tp->rcv_func = rfunc;
	tp->local_seq = 0x1234ABCD;	/* XXX */
	tp->remote_seq = 0;	/* for now */
	tp->state = CWAIT;
	tp->sem = sem_signal_new ( SEM_FIFO );

	tp->next = tcp_io_head;
	tcp_io_head = tp;

	tcp_send_data ( tp, TH_SYN, NULL, 0 );
	sem_block ( tp->sem );

	return tp;
}

void
tcp_connect_thread ( int arg )
{
	struct tcp_io *tp = (struct tcp_io *) arg;

	printf ( "Connect thread launched\n" );

	tcp_send_data ( tp, TH_SYN | TH_ACK, NULL, 0 );
	printf ( "Connect thread blocking\n" );

	sem_block ( tp->sem );

	printf ( "Connect thread running\n" );

	/*
	tcp_send_pk ( tp, "fish", 4 );
	tcp_shutdown ( tp );
	*/
	printf ( "Connect thread done\n" );
}

/* This gets called when someone tries to establish a
 * connection to one of our servers.
 * Note that this SHOULD NOT block, as it is running
 * as part of the network thread.
 */
struct tcp_io *
tcp_server_connect ( struct tcp_server *sp, struct netbuf *nbp )
{
	struct tcp_io *tp;
	struct tcp_hdr *xtcp;
	struct ip_hdr *xipp;

	tp = tcp_io_alloc ();
	if ( ! tp )
	    return NULL;

	xtcp = (struct tcp_hdr *) nbp->pptr;
	xipp = nbp->iptr;

	tp->remote_ip = xipp->src;
	tp->local_port = ntohs (xtcp->dport); 
	tp->remote_port = ntohs (xtcp->sport);

	printf ( "Server connect on port %d\n", tp->local_port );

	tp->rcv_func = sp->rcv_func;

	tp->remote_seq = ntohl(xtcp->seq);
	tp->local_seq = 0x1234ABCD;	/* XXX */
	tp->state = CWAIT;
	tp->sem = sem_signal_new ( SEM_FIFO );

	tp->next = tcp_io_head;
	tcp_io_head = tp;

	printf ( "Launching connect thread\n" );
	(void) thr_new ( "conn", tcp_connect_thread, (void *) tp, CONN_PRI, 0 );

	return tp;
}

/* declare our intent to be a TCP server on a given port.
 * This is what some people call a "passive open"
 * No network traffic is generated.
 * We just make an entry in internal datastructures
 *  that indicate how we respond we connections arrive.
 */
void
tcp_server ( int port, tcp_cptr cfunc, tcp_fptr rfunc )
{
	struct tcp_server *sp;

	/* We don't worry much about reallocating these
	 * since servers don't tend to come and go like
	 * connections do.
	 */
	sp = (struct tcp_server *) malloc ( sizeof(struct tcp_server) );
	if ( ! sp )
	    return;

	sp->port = port;
	sp->conn_func = cfunc;
	sp->rcv_func = rfunc;

	sp->next = tcp_server_head;
	tcp_server_head = sp;
}

/* Called with every received TCP packet */
void
tcp_rcv ( struct netbuf *nbp )
{
	struct tcp_hdr *tcp;
	struct tcp_io *tp;
	struct tcp_server *sp;
	int tlen;

	tcp = (struct tcp_hdr *) nbp->pptr;
	tlen = tcp->hlen * 4;
	nbp->dlen = nbp->plen - tlen;
	nbp->dptr = nbp->pptr + tlen;

#ifdef DEBUG_TCP
	dump_tcp ( nbp );
#endif

	/* At this point, we aren't a gateway, so drop the packet */
	if ( nbp->iptr->dst != host_info.my_ip ) {
	    printf ("TCP packet is not for us - dropped\n" );
	    return;
	}

	/* Somebody thinks we are a server */
	if ( tcp->flags == TH_SYN ) {
	    sp = tcp_server_lookup ( nbp );
	    if ( sp ) {
		tcp_server_connect ( sp, nbp );
		return;
	    } else {
		/* Tell them to go away */
		/* - could just silently drop packet */
		printf ( "Sending TCP rst/ack\n");
		tcp_reply_rst ( nbp );
		return;
	    }
	}

	/* Is this for a connection we know about? */
	tp = tcp_io_lookup ( nbp );
	if ( tp ) {
	    tcp_io_receive ( tp, nbp );
	    return;
	}

	/* otherwise, silently drop packet */
	return;
}

/* Send a RST/ACK packet to make somebody go away */
static void
tcp_reply_rst ( struct netbuf *xbp )
{
	struct tcp_io rst_io;
	struct tcp_hdr *xtcp;
	struct ip_hdr *xipp;

	xtcp = (struct tcp_hdr *) xbp->pptr;
	xipp = xbp->iptr;

	rst_io.local_port = ntohs (xtcp->dport); 
	rst_io.remote_port = ntohs (xtcp->sport);

	rst_io.remote_seq = ntohl(xtcp->seq);
	rst_io.local_seq = 0;

	rst_io.remote_ip = xipp->src;

	tcp_send_data ( &rst_io, TH_RST | TH_ACK, NULL, 0 );
}

struct tcp_pseudo {
	u32 src;
	u32 dst;
	unsigned short proto;
	unsigned short len;
};

static void
tcp_send_data ( struct tcp_io *tp, int flags, char *buf, int count)
{
	struct netbuf *nbp;
	struct tcp_hdr *tcp;
	struct tcp_pseudo pseudo;
	unsigned short sum;
	int sum_count;

	nbp = netbuf_alloc ();
	if ( ! nbp )
	    return;

	nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );

	/* This will need to be changed once we have options */
	nbp->dptr = nbp->pptr + sizeof ( struct tcp_hdr );

	nbp->dlen = count;
	if ( count > 0 )
	    memcpy ( nbp->dptr, buf, count );

	/* If payload size is odd, pad for checksum */
	sum_count = count;
	if ( count % 2 ) {
	    nbp->dptr[count] = 0;
	    sum_count++;
	}

	nbp->plen = nbp->dlen + sizeof(struct tcp_hdr);
	nbp->ilen = nbp->plen + sizeof(struct ip_hdr);
	nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

	printf ( "SEND DATA: ilen = %d\n", nbp->ilen );

	memset ( nbp->pptr, 0, sizeof(struct tcp_hdr) );

	nbp->iptr->proto = IPPROTO_TCP;

	tcp = (struct tcp_hdr *) nbp->pptr;

	tcp->sport = htons(tp->local_port);
	tcp->dport = htons(tp->remote_port);

	tcp->flags = flags;

	tcp->ack = htonl(tp->remote_seq + 1);	/* next byte expected */
	tcp->seq = htonl(tp->local_seq);	/* start of our data */

	tcp->hlen = 5;

	tcp->win = DEF_WIN;
	tcp->urg = 0;

	/* Setup and checksum the pseudo header */
	pseudo.src = host_info.my_ip;
	pseudo.dst = tp->remote_ip;

	pseudo.proto = htons ( IPPROTO_TCP );
	pseudo.len = htons ( sizeof(struct tcp_hdr) );

	sum = in_cksum_i ( &pseudo, sizeof(struct tcp_pseudo), 0 );
	sum = in_cksum_i ( tcp, sizeof(struct tcp_hdr), sum );
	if ( sum_count )
	    sum = in_cksum_i ( nbp->dptr, sum_count, sum );

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

/* THE END */
