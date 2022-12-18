/*
 * Copyright (C) 202022  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* kyu_compat.h
 *	Tom Trebisky 11/1/2022
 */

/* This is a hook to pull in necessary kyu header files for
 *  the BSD TCP subsystem.
 */

/* for ntohl() and such */
#include <arch/cpu.h>

/* Just do locking around big pieces of the TCP subsystem
 * Introduced 12-18-2022 when I got frustrated with emulating
 * the splx() family of calls with their nesting.
 */
#define BIG_LOCKS

/* THE END */
