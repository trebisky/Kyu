/*
 * Copyright (C) 2022  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* mbuf.h
 *	Tom Trebisky 11/11/2022
 *
 * Note that there is also sys/mbuf.h which is the BSD file, which
 * we expect to eliminate.
 */

/* for ntohl() and such */
#include <arch/cpu.h>

struct mbuf * mb_get ( int );
struct mbuf * mb_gethdr ( int );

struct mbuf * mb_free ( struct mbuf * );
void mb_freem ( struct mbuf * );

#define mtod(m,t)	((t)((m)->m_data))

/* THE END */
