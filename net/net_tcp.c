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

#include "netbuf.h"
#include "kyu_tcp.h"

#include "../thread.h"
#include "../tests.h"

static struct tcp_ops *tcp_ops;

void
kyu_tcp_init ( void )
{
#ifdef WANT_TCP_XINU
	tcp_ops = tcp_xinu_init ();
#endif
#ifdef WANT_TCP_BSD
	tcp_ops = tcp_bsd_init ();
#endif
#ifdef WANT_TCP_KYU
	tcp_ops = tcp_kyu_init ();
#endif

	(*tcp_ops->init) ();
}

/* Pass a received packet on to TCP
 */
void
tcp_rcv ( struct netbuf *nbp )
{
	(*tcp_ops->rcv) (nbp);
	netbuf_free ( nbp );
}

void xtest_show ( long );

/* XXX - stolen from tcp_xinu, needs rework */
/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
        xtest_show,             "Show TCB table",                       0,
#ifdef notdef
        xtest_client_echo,      "Echo client [n]",                      0,
        xtest_client_echo2,     "Echo client (single connection) [n]",  0,
        xtest_client_daytime,   "Daytime client [n]",                   0,
        xtest_server_daytime,   "Start daytime server",                 0,
        xtest_server_classic,   "Start classic server",                 0,
#endif
        0,                      0,                                      0
};

void
xtest_show ( long xxx )
{
	printf ( "TCP show -- sorry, not ready yet\n" );
}

/* THE END */
