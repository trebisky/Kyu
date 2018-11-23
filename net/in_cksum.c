/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

#include <arch/types.h>

/* portable, naive, but simple */
unsigned short
in_cksum ( unsigned short *data, int len )
{
	i32 sum = 0;

	while ( len > 1 ) {
	    /* sum += * ((unsigned short *) data)++; */
	    sum += *data++;
	    if ( sum & 0x80000000 )
		sum = (sum & 0xffff) + (sum >> 16);
	    len -= 2;
	}

	if ( len )
	    sum += (unsigned short) * (unsigned char *) data;

	while ( sum >> 16 )
	    sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/* Does not return ones complement */
unsigned short
in_cksum_i ( unsigned short *data, int len, unsigned short init )
{
	i32 sum = init;

	while ( len > 1 ) {
	    /* sum += * ((unsigned short *) data)++; */
	    sum += *data++;
	    if ( sum & 0x80000000 )
		sum = (sum & 0xffff) + (sum >> 16);
	    len -= 2;
	}

	if ( len )
	    sum += (unsigned short) * (unsigned char *) data;

	while ( sum >> 16 )
	    sum = (sum & 0xffff) + (sum >> 16);

	return sum;
}

/* THE END */
