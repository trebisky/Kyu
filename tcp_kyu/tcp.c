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

// #include "thread.h"
// #include "netbuf.h"

#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

static void tcp_kyu_initialize ( void );
static void tcp_kyu_rcv ( struct netbuf *nbp );

struct tcp_ops kyu_ops = {
	tcp_kyu_initialize,
	tcp_kyu_rcv
};

/* Kyu calls this during initialization */
struct tcp_ops *
tcp_kyu_init ( void )
{
	return &kyu_ops;
}

static void
tcp_kyu_initialize ( void )
{
}

static void
tcp_kyu_rcv ( struct netbuf *nbp )
{
}

/* THE END */
