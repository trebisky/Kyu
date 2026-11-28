/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * tcp.h -- Kyu
 *
 * The game here is to get telnet.c to compile without dragging
 *  in all of the include files in ../tcp_bsd
 *
 * Tom Trebisky  11-27-2026
 */

#include <arch/types.h>
#include "netbuf.h"

struct socket {
};

struct socket * tcp_bind ( int );
struct socket * tcp_accept ( struct socket * );

/*  THE END */
