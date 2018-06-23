/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * tests.h
 *
 * Tom Trebisky
 *
 * split from tests.c  --  6/23/2018
 */

struct test {
	tfptr	func;
	char	*desc;
	int	arg;
};

#define PRI_DEBUG	5
#define PRI_TEST	10
#define PRI_WRAP	20

/* THE END */
