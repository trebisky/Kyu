/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * test_net.c
 * Tom Trebisky
 *
 * began as user.c -- 11/17/2001
 * made into tests.c  9/15/2002
 * added to kyu 5/11/2015
 * split into test_net.c  --  6/23/2018
 */

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"
#include "arch/cpu.h"

#include "tests.h"
#include "netbuf.h"

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* NET tests */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

void pkt_arm ( void );
void pkt_arrive ( void );

#ifdef WANT_NET
static void test_netshow ( long );
static void test_netarp ( long );
static void test_bootp ( long );
static void test_dhcp ( long );
static void test_icmp ( long );
static void test_dns ( long );
static void test_arp ( long );
 void test_tftp ( long );
static void test_udp ( long );
static void test_udp_echo ( long );

#endif

static void test_netdebug ( long );
static void test_fast ( long );

#ifdef notdef
void
test_tcp ( long xxx )
{
#ifdef WANT_TCP_XINU
	printf ( "Testing Xinu TCP\n" );
	/* Not yet implemented ??? */
	test_xinu_tcp ();
#endif
#ifdef WANT_TCP_BSD
	printf ( "Testing BSD TCP\n" );
	test_bsd_tcp ();
#endif
#ifdef WANT_TCP_KYU
	printf ( "Testing Kyu TCP\n" );
	test_kyu_tcp ();
#endif
}
#endif


/* Exported to main test code */
/* Arguments are now ignored */
struct test net_test_list[] = {

#ifdef WANT_NET
	test_netshow,	"Net show",		0,
	test_netarp,	"ARP ping",		0,
	test_bootp,	"test BOOTP",		0,
	test_dhcp,	"test DHCP",		0,
	test_icmp,	"Test ICMP",		0,
	test_dns,	"Test DNS",		0,
	test_arp,	"gratu arp [n]",	0,
	test_tftp,	"Test TFTP",		0,
	test_udp,	"Test UDP",		0,
	test_udp_echo,	"Endless UDP echo",	0,
	// test_tcp,	"Test TCP",		0,
#endif
	test_netdebug,	"Debug interface",	0,
	test_fast,	"Test network speed",	0,
	0,		0,			0
};

#ifdef WANT_NET

static void
test_netshow ( long test )
{
	net_show ();
}

extern unsigned long test_ip;

static void
test_netarp ( long test )
{
	show_arp_ping ( test_ip );
}

static void
test_icmp ( long test )
{
	ping ( test_ip );
}

static void
test_bootp ( long test )
{
	bootp_test();
}

static void
test_dhcp ( long test )
{
	dhcp_test();
}

static void
test_dns ( long test )
{
	dns_test();
}

static void
test_arp ( long count )
{
	int i;

	pkt_arm ();

	for ( i=0; i < count; i++ ) {
	    // thr_delay ( 10 );
	    pkt_arrive ();
	    arp_announce ();
	}
}

/* The following expects the UDP echo service to
 * be running on the selected host.
 * To get this going on a unix system:
 *   su
 *   yum install xinetd
 *   cd /etc/xinetd.d
 *   edit echo-dgram and enable it
 *   service xinetd restart
 * After this,
 *   "n 9" should run this test
 * I tried sending 1000 packets with a 10 ms delay, but this triggered some safeguard
 * on my linux server after 50 packets with the following messages:
 * Kyu, ready> n 9
 * sending 1000 UDP messages
 * 50 responses to 1000 messages
Aug  2 12:17:43 localhost xinetd[14745]: START: echo-dgram pid=0 from=::ffff:192.168.0.11
Aug  2 12:17:43 localhost xinetd[14745]: Deactivating service echo due to excessive incoming connections.  Restarting in 10 seconds.
Aug  2 12:17:43 localhost xinetd[14745]: FAIL: echo-dgram connections per second from=::ffff:192.168.0.11
Aug  2 12:17:53 localhost xinetd[14745]: Activating service echo
 *
 * In many ways the xinetd echo service is unfortunate.
 * Among other things it puts a line in the log for every packet it echos !!
 * And it throttles traffic.  So we found a simple UDP echo server online
 * and use it instead.
 */

#define UTEST_SERVER	"192.168.0.5"
#define UTEST_PORT	7

static int udp_echo_count;

static void
udp_test_rcv ( struct netbuf *nbp )
{
	// printf ( "UDP response\n" );
	udp_echo_count++;
}

/* With the xinetd server, we must delay at least 40 ms
 * between each packet.  Pretty miserable.
 */
// #define ECHO_COUNT 5000
// #define ECHO_DELAY 50 OK
// #define ECHO_DELAY 25 too fast
// #define ECHO_DELAY 40 OK

/* With a real echo server and 5 bytes messages,
 *  this runs in about 7 seconds.
 */
#define ECHO_COUNT 10000
#define ECHO_DELAY 1

#define UDP_TEST_SIZE	1024
static char udp_test_buf[UDP_TEST_SIZE];

#define UDP_BURST	5

/* With 1K packets and 5 packet bursts, we can run the test
 * in about 2 seconds with a decent server on the other end.
 */

static void
test_udp ( long xxx )
{
	int i;
	// char *msg = "hello";
	unsigned long test_ip;
	int local_port;
	int count;
	int len;

	// strcpy ( udp_test_buf, "hello" );
	// len = strlen ( udp_test_buf );

	len = UDP_TEST_SIZE;
	memset ( udp_test_buf, 0xaa, len );

	local_port = get_ephem_port ();
	(void) net_dots ( UTEST_SERVER, &test_ip );

	udp_hookup ( local_port, udp_test_rcv );

	count = ECHO_COUNT;
	udp_echo_count = 0;

	printf ("sending %d UDP messages\n", count );

	for ( i=0; i < count; i++ ) {
	    // printf ("sending UDP\n");
	    udp_send ( test_ip, local_port, UTEST_PORT, udp_test_buf, len );
	    if ( (i % UDP_BURST) == 0 )
		thr_delay ( ECHO_DELAY );
	}

	/* Allow time for last responses to roll in */
	thr_delay ( 100 );

	printf ( "%d responses to %d messages\n", udp_echo_count, ECHO_COUNT );
}

/* ---------------------------------------------------------- */
/* ---------------------------------------------------------- */

static struct {
	int arrive;
	int dispatch;
	int seen;
	int reply;
	int send;
	int finish;
} pk;

static int pk_limit = 0;

void pkt_arrive ()
{
	// We cannot all be using the ccnt and resetting it !!
	// This yielded some hard to debug issues with other parts
	// of the system using this for timing.  A basic lesson here.
	// reset_ccnt ();
	// pk.arrive = 0;
	pk.arrive = r_CCNT ();
	pk.dispatch = -1;
	pk.seen = -1;
	pk.reply = -1;
	pk.send = -1;
	pk.finish = -1;

}

void pkt_dispatch ()
{
	pk.dispatch = r_CCNT ();
}

void pkt_seen ()
{
	pk.seen = r_CCNT ();
}

void pkt_reply ()
{
	pk.reply = r_CCNT ();
}

void pkt_send ()
{
	pk.send = r_CCNT ();
}

// Called at interrupt level */
void pkt_finish ( void )
{
	pk.finish = r_CCNT ();

	if ( pk_limit == 0 )
	    return;
	pk_limit--;

	printf ( "arrive: %d\n", pk.arrive );
	printf ( "dispatch: %d\n", pk.dispatch );
	printf ( "seen: %d\n", pk.seen );
	printf ( "reply: %d\n", pk.reply );
	printf ( "send: %d\n", pk.send );
	printf ( "finish: %d\n", pk.finish );
}

void pkt_arm ()
{
	pk_limit = 5;
}

#define ENDLESS_SIZE	1024
static char endless_buf[ENDLESS_SIZE];
static int endless_count;
static int endless_port;
static unsigned long endless_ip;

unsigned long last_endless = 0;

/* Test this against the compiled C program in tools/udpecho.c
 * run that as ./udpecho 6789
 */
#define EECHO_PORT	6789

/* This is where all the action is when this gets going */
static void
endless_rcv ( struct netbuf *nbp )
{
	pkt_seen ();

	last_endless = * (unsigned long *) endless_buf;
	if ( last_endless != endless_count )
	    printf ( "Endless count out of sequence: %d %d\n", endless_count, last_endless );

	++endless_count;
	* (unsigned long *) endless_buf = endless_count;

	pkt_reply ();

	udp_send ( endless_ip, endless_port, EECHO_PORT, endless_buf, ENDLESS_SIZE );

	if ( endless_count < 2 )
	    printf ( "First UDP echo seen\n" );
	if ( (endless_count % 1000) == 0 ) {
	    printf ( "%5d UDP echos\n", endless_count );
#ifdef BOARD_ORANGE_PI
	    // emac_show_last ( 0 );
#endif
	}
}

/* Currently it takes 10 seconds to send 4000 exchanges,
 * i.e. 2.5 milliseconds per exchange.
 * So, we wait 3 milliseconds and then get concerned.
 * Suprisingly, this seems to work just fine.
 */
void
endless_watch ( long xxx )
{
	int count;
	int tmo = 0;

	while ( endless_count < 2 && tmo++ < 10 )
	    thr_delay ( 1 );

	if ( tmo > 8 ) {
	    printf ( "Never started\n" );
	    return;
	}
	printf ( "Watcher started\n" );

	for ( ;; ) {
	    count = endless_count;
	    // thr_delay ( 3 );
	    thr_delay ( 6 );
	    if ( count == endless_count )
		break;
	}
	printf ( "Stalled at %d\n", count );
#ifdef BOARD_ORANGE_PI
	capture_last ( 0 );
#endif
}

/* Endless echo of UDP packets
 * Call this to start the test.
 */
static void
test_udp_echo ( long test )
{

	// check_clock ();
	pkt_arm ();

	last_endless = 0;
	memset ( endless_buf, 0xaa, ENDLESS_SIZE );
	endless_count = 1;

	* (unsigned long *) endless_buf = endless_count;

	endless_port = get_ephem_port ();
	(void) net_dots ( UTEST_SERVER, &endless_ip );

	udp_hookup ( endless_port, endless_rcv );

	printf ( "Endless UDP test from our port %d\n", endless_port );

#ifdef BOARD_ORANGE_PI
	capture_last ( 1 );
#endif

	/* Kick things off with first message */
	udp_send ( endless_ip, endless_port, EECHO_PORT, endless_buf, ENDLESS_SIZE );

	(void) safe_thr_new ( "watcher", endless_watch, NULL, 24, 0 );

	/* No thread needed, the rest happens via interrupts */
}

#else
/* Stubs to match calls in driver. */
void pkt_finish ( void ) {}
void pkt_arrive ( void ) {}
void pkt_send ( void ) {}
void pkt_dispatch ( void ) {}
#endif	/* WANT_NET */

/* Test 100 mbit network speed
 * 1-13-2023 during orangepi emac debugging
 */
static void
test_fast ( long xxx )
{
	unsigned int t1, t2;
	struct netbuf *nbp;
	int i;

#ifdef notdef
	/* This comes out exactly right */
	printf ( "Start 5 sec delay\n" );
	thr_delay ( 5000 );
	printf ( "end 5 sec delay\n" );
#endif

#ifdef BOARD_ORANGE_PI
	set_CCNT ( 0 );
	t1 = r_CCNT ();
	thr_delay ( 100 );
	t2 = r_CCNT ();

	/* This yields 100,145,548 counts in 1/10 second
	 * which is just right for a CPU at 1 Ghz
	 */
	printf ( "CCNT 0 = %d\n", t1 );
	printf ( "CCNT timer, 0.1 seconds = %d\n", t2-t1 );

	nbp = netbuf_alloc ();

	nbp->ilen = 1000;

	set_CCNT ( 0 );
	t1 = r_CCNT ();

	for ( i=0; i<10; i++ )
	    emac_send_wait ( nbp );
	    // board_net_send ( nbp );

	t2 = r_CCNT ();
	printf ( "1M data = %d\n", t2-t1 );

	netbuf_free ( nbp );
#else
	printf ( "Only works on the Orange Pi\n" );
#endif /* BOARD_ORANGE_PI */
}

/* Hook for board specific network statistics
 */
static void
test_netdebug ( long test )
{
	board_net_debug ();

	// printf ( "last endless count = %d\n", last_endless );
}


/* THE END */
