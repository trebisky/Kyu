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

#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "malloc.h"
#include "arch/cpu.h"

// #include "netbuf.h"

#define TEST_SERVER     "192.168.0.5"
#define ECHO_PORT       7       /* echo */
#define DAYTIME_PORT    13      /* daytime */

struct test {
	tfptr	func;
	char	*desc;
	int	arg;
};

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* TCP tests */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

static unsigned long
dots2ip ( char *s )
{
	int nip;

	(void) net_dots ( s, &nip );
	return ntohl ( nip );
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

static void xtest_client_echo ( int );
static void xtest_client_daytime ( int );
static void xtest_tcp_echo ( int );

/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
	xtest_client_echo,	"Echo client [n]",	0,
	xtest_client_daytime,	"Daytime client [n]",	0,
	xtest_tcp_echo,		"Endless TCP echo",	0,
	0,		0,			0
};

static void
xtest_tcp_echo ( int test )
{
	printf ( "Not ready yet\n" );
}

/* This requires a daytime server running on port 13
 * ( on TEST_SERVER -- likely the boot host).
 * To run this on fedora:
 *  dnf install xinetd
 *  edit /etc/xinet.d/daytime-stream to enable
 *  systemctl start  xinetd.service
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

	// printf ( "Client recv got %s\n", buf );
	// terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );
	tcp_close ( slot );
	return buf;
}

static void
xtest_client_daytime ( int count )
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

	printf ( "Client connection on port %d, slot = %d\n", port, slot );
	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	tcp_send ( slot, "Duck!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	tcp_send ( slot, "Goose!\r\n", 6 );
	n = tcp_recv ( slot, buf, 100 );
	printf ( "Client recv returns %d bytes\n", n );
	buf[n] = '\0';
	printf ( "Client recv got %s\n", buf );

	// replies are terminated with \r\n
	// printf ( "Client %02x\n", buf[n-2] );
	// printf ( "Client %02x\n", buf[n-1] );

	tcp_close ( slot );
}

static void
xtest_client_echo ( int count )
{
	int i;

	for ( i=0; i<count; i++ ) {
	    client_echo ();
	}
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/* Callback function to handle the following */

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
test_daytime ( void )
{
	int port = DAYTIME_PORT;
	int rv;

	rv = tcp_server ( port, daytime_func );
	printf ( "Listening on port %d via callback (%d)\n", port, rv );
}

/* Original style passive connection */
/* This stays around, blocked in tcp_recv() */
/* Just use telnet to talk to this, once the connection is made
 * the two strings "cat and dog" should be received.
 *
 * Seems to be working fine as of 6-21-2018
 *
 *  "classic server"
 */

static void
test_server ( int bogus )
{
	int lslot;
	int cslot;
	int rv;
	int port = 1234;

	lslot = tcp_register ( 0, port, 0 );
	printf ( "Server listening on port %d (slot %d)\n", port, lslot );

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
	    tcp_send ( cslot, "dog\n", 4 );
	    tcp_send ( cslot, "cat\n", 4 );
	    tcp_close ( cslot );
	}
	printf ( "Server on port %d exiting !!\n", port );
}

static void
launch_classic_server ( void )
{
	(void) thr_new ( "xinu_tester", test_server, NULL, 30, 0 );
}

static int servers_running = 0;

#ifdef notdef

/* Test - active connection to Daytime server */
/* Sometimes this just locks up in SYNSENT ??!! */
/* Not anymore ??? */

void
test_xinu_tcp ( void )
{
	/* Test an active client to port 13 */
	client_daytime ();
	client_echo ();

	if ( ! servers_running ) {
	    servers_running = 1;

	    /* This starts a daytime server on port 13 */
	    test_daytime ();

	    /* This starts a server on port 1234 */
	    /* This needs to run in a thread */
	    launch_classic_server ();

	    thr_delay ( 10 );
	}
}
#endif
/* THE END */
