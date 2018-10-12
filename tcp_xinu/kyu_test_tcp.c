/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * test_tcp.c
 * Tom Trebisky
 */

#include "xinu.h"

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"

#include "../tests.h"

// #include "arch/cpu.h"

#define TEST_SERVER     "192.168.0.5"
#define ECHO_PORT       7       /* echo */
#define DAYTIME_PORT    13      /* daytime */

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* TCP tests */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

static void xtest_show ( long );
static void xtest_client_echo ( long );
static void xtest_client_echo2 ( long );
static void xtest_client_daytime ( long );
static void xtest_server_daytime ( long );
static void xtest_server_classic ( long );

/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
	xtest_show,		"Show TCB table",			0,
	xtest_client_echo,	"Echo client [n]",			0,
	xtest_client_echo2,	"Echo client (single connection) [n]",	0,
	xtest_client_daytime,	"Daytime client [n]",			0,
	xtest_server_daytime,	"Start daytime server",			0,
	xtest_server_classic,	"Start classic server",			0,
	0,			0,					0
};

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

static unsigned long
dots2ip ( char *s )
{
	int nip;

	(void) net_dots ( s, &nip );
	return ntohl ( nip );
}

/* Called from elsewhere (emergency debug) */
void
tcp_show ( void )
{
	struct tcb *tp;
	int i;
	int count;

	count = 0;
	for (i = 0; i < Ntcp; i++) {
	    if (tcbtab[i].tcb_state == TCB_FREE) {
		count++;
		continue;
	    }

	    tp = &tcbtab[i];
	    if ( tp->tcb_state == TCB_LISTEN )
		printf ( "TCB slot %d: Listen on port %d\n", i, tp->tcb_lport );
	    else if ( tp->tcb_state == TCB_ESTD )
		printf ( "TCB slot %d: Established %d (%d)\n", i, tp->tcb_lport, tp->tcb_rport );
	    else if ( tp->tcb_state == TCB_CLOSED )
		printf ( "TCB slot %d: Closed\n", i );
	    else if ( tp->tcb_state == TCB_CWAIT )
		printf ( "TCB slot %d: Close wait\n", i );
	    else if ( tp->tcb_state == TCB_TWAIT )
		printf ( "TCB slot %d: TWAIT\n", i );
	    else if ( tp->tcb_state == TCB_SYNSENT )
		printf ( "TCB slot %d: SYNSENT\n", i );
	    else
		printf ( "TCB slot %d: state = %d\n", i, tp->tcb_state );
        }
	printf ( "%d TCB slots free\n", count );

	netbuf_show ();
	xinu_memb_stats ();
}

static void
xtest_show ( long xxx )
{
	tcp_show ();
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* Clients */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/* There are examples of Xinu clients and servers in
 *  the shell directory:
 *
 *   xsh_tcpclient_big.c
 *   xsh_tcpclient.c
 *   xsh_tcpserver_big.c
 *   xsh_tcpserver.c
 */

/* This requires a daytime server running on port 13
 * ( on TEST_SERVER -- likely the boot host).
 * To run this on fedora:
 *  dnf install xinetd
 *  edit /etc/xinet.d/daytime-stream to enable
 *  systemctl start  xinetd.service
 *
 * This consumes slots.  6-24-2018
 * This ends up in TCB_CLOSED state and stays
 * there forever.
 */
static char *
client_daytime ( void )
{
	int port = DAYTIME_PORT;
	int slot;
	static char buf[80];
	int n;

	// printf ( "Begin making client connection\n" );
	slot = tcp_register ( dots2ip(TEST_SERVER), port, 1 );
	if ( slot < 0 )
	    return "no slot";
	// printf ( "Client connection on port %d, slot = %d\n", port, slot );
	n = tcp_recv ( slot, buf, 100 );
	// printf ( "Client recv returns %d bytes\n", n );
	if ( n >= 2 )
	    buf[n-2] = '\0';

	n = tcp_recv ( slot, buf, 100 );
	printf ( "Bogus client recv got %d\n", n );

	// printf ( "Client recv got %s\n", buf );
	// terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );
	tcp_close ( slot );
	return buf;
}

static void
xtest_client_daytime ( long count )
{
	char *msg;
	int i;

	for ( i=0; i<count; i++ ) {
	    msg = client_daytime ();
	    printf ( "%3d: %s\n", i, msg );
	}
}

/* This requires an echo server running on port 7
 *  on the TEST_SERVER host ( likely the boot host).
 * To run this on fedora:
 *  dnf install xinetd
 *  edit /etc/xinet.dd/echo-stream to enable
 *  (change disable to no)
 *  systemctl restart  xinetd.service
 *  test using "telnet localhost 7"
 *
 * This seems to work perfectly.  6-24-2018
 * The TCB ends up in TWAIT, then after the finwait
 * timeout expires, the TCB becomes free.
 */
static void
client_echo ( void )
{
	int port = ECHO_PORT;
	int slot;
	char buf[100];
	int n;

	// printf ( "Begin making client connection\n" );
	slot = tcp_register ( dots2ip(TEST_SERVER), port, 1 );
	if ( slot < 0 ) {
	    printf ( "No slot\n" );
	    return;
	}

	// printf ( "Client connection on port %d, slot = %d\n", port, slot );
	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	//printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	// printf ( "Client recv got %s\n", buf );
	printf ( "%s\n", buf );

	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	//printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	// printf ( "Client recv got %s\n", buf );
	printf ( "%s\n", buf );

	tcp_send ( slot, "Goose!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	//printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	// printf ( "Client recv got %s\n", buf );
	printf ( "%s\n", buf );

	// replies are terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );

	tcp_close ( slot );
}

/* When in a loop, this makes 4 connections per second.
 * with a 5 second finwait, this will yield 20 connections
 * in FINWAIT, which should work.
 * Bumping it up to 10 per second, will yield 50 in FINWAIT.
 */
static void
xtest_client_echo ( long count )
{
	int i;

	for ( i=0; i<count; i++ ) {
	    if ( count > 1 )
		printf ( "Echo %d\n", i );
	    client_echo ();
	    // thr_delay ( 250 );
	    // thr_delay ( 200 );
	    thr_delay ( 100 );
	}
}

/* Like the above, but makes one connection,
 * then loops using it.
 * This was a success 4-25-2018 on the Orange Pi
 * running at 20 Hz for 99,999 repetitions.
 */
static void
xtest_client_echo2 ( long repeat )
{
	int port = ECHO_PORT;
	int slot;
	char buf[100];
	int n;
	int i;

	// printf ( "Begin making client connection\n" );
	slot = tcp_register ( dots2ip(TEST_SERVER), port, 1 );
	if ( slot < 0 ) {
	    printf ( "No slot\n" );
	    return;
	}

	for ( i=0; i<repeat; i++ ) {
	    // printf ( "Client connection on port %d, slot = %d\n", port, slot );
	    tcp_send ( slot, "Salami\r\n", 6 );
	    n = tcp_recv ( slot, buf, 100 );
	    //printf ( "Client recv returns %d bytes\n", n );
	    buf[n] = '\0';
	    // printf ( "Client recv got %s\n", buf );
	    printf ( "%d: %s\n", i, buf );

	    // thr_delay ( 100 );
	    thr_delay ( 50 );
	}

	tcp_close ( slot );
	printf ( " Done with echo test\n" );
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* Servers */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

static int daytime_is_running = 0;

/* Handle each connection to my event style daytime server.
 */
static void
daytime_func ( int slot )
{
	static char reply[] = "4 JUL 1776 08:00:00 EST\r\n";
	int n;

	n = sizeof(reply);
	printf ( "In daytime callback, slot %d\n", slot );
	printf ( "In daytime callback, sending %d bytes\n", n );
	tcp_send ( slot, reply, n );
	tcp_close ( slot );
}

/* New style passive connection with listener callback */
/* Note that this just sets up a callback and evaporates */
/* So traffic is just handled by the network thread and we
 *  have no thread associated with this server.
 */
static void
xtest_server_daytime ( long xxx )
{
	int port = DAYTIME_PORT;
	int rv;

	if ( daytime_is_running ) {
	    printf ( "Daytime server is already registered on port %d\n", port );
	    return;
	}

	rv = tcp_server ( port, daytime_func );
	printf ( "Listening on port %d via callback (%d)\n", port, rv );
	daytime_is_running = 1;
}

/* Original style ( "classic" ) passive connection */
/* This stays around, blocked in tcp_recv() */
/* Just use telnet to talk to this, once the connection is made
 * the two strings "cat and dog" should be received.
 *
 * Seems to be working fine as of 6-21-2018
 */

static int classic_is_running = 0;

#define CLASSIC_PORT	1234

/* XXX - would be nice to append newline here ?? */
static void
tcp_send_string ( int slot, char *msg )
{
	int len = strlen ( msg );

	tcp_send ( slot, msg, len );
}

static void
classic_server ( long bogus )
{
	int lslot;
	int cslot;
	int rv;
	int port = CLASSIC_PORT;

	lslot = tcp_register ( 0, port, 0 );
	printf ( "Server listening on port %d (slot %d)\n", port, lslot );
	classic_is_running = 1;

	for ( ;; ) {
	    printf ( "Waiting for connection on port %d\n", port );
	    rv = tcp_recv ( lslot, (char *) &cslot, 4 );
	    if ( rv < 0 ) {
		/* Without this check, once we tie up all slots in
		 * TWAIT, this will yield a runaway loop
		 */
		printf ( "recv on port %d fails - maybe no more slots?\n", port );
		break;
	    }
	    printf ( "Connection on port %d!! rv= %d, slot= %d\n", port, rv, cslot );
	    tcp_send_string ( cslot, "Have a nice day.\n" );
	    tcp_send_string ( cslot, "And to all a good night\n" );
	    tcp_close ( cslot );
	}

	/* Why would it exit */
	printf ( "Server on port %d exiting !!\n", port );
	classic_is_running = 0;
}

static void
xtest_server_classic ( long xxx )
{
	if ( classic_is_running ) {
	    printf ( "Classic server is already running on port %d\n", CLASSIC_PORT );
	    return;
	}

	(void) thr_new ( "tcp_1234", classic_server, NULL, 30, 0 );
}

/* THE END */
