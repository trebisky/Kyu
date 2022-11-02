/*
 * Copyright (C) 2022  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * kyu_tcp.h -- Kyu
 *
 * Tom Trebisky  11-102022
 */

#include <arch/types.h>
#include "netbuf.h"
#include "net.h"

typedef void (*vfptr) ( void );

struct tcp_ops {
	vfptr	init;
	ufptr	rcv;
};

struct tcp_ops * tcp_bsd_init ( void );
struct tcp_ops * tcp_xinu_init ( void );
struct tcp_ops * tcp_kyu_init ( void );

/*  THE END */
