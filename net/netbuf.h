/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * Kyu netbuf.
 *
 * Tom Trebisky  4/9/2005 1/14/2017
 *
 * This structure is used to hold a network packet
 * as it rattles around the system.
 *
 * Originally, I thought I would just make this a nice
 * even 2048 bytes, but there is really no advantage to
 * that (and it could even cause excess cache collisions).
 * All that is really necessary is that the data area will
 * hold an entire ethernet frame.  Some drivers (like the
 * ee100) will allocate these and set them up so the chip
 * deposits data right into them.  Such drivers will need
 * some extra space in the data area to hold their specific
 * control information.  Other drivers (like the realtek 8139)
 * are forced to copy out of their dedicated ring into this
 * buffer and need no extra space, but we need to be flexible.
 */

/* Each board sets a macro NETBUF_PREPAD
 *
 * On the x86, the ee100 controllers needs 16 bytes
 *  for its Tx descriptor.
 * On the ARM, having 2 bytes of prepad will put
 *  IP addresses on 4 byte boundaries.
 *  (this is now an optimization rather than a necessity)
 * The prepad is handled by skipping over the specified
 *  number of bytes in "data" when setting the various
 *  header pointers, in particular eptr.
 */

/* Strictly speaking 1518 would do,
 * there are alignment issues on the ARM to get
 * the IP fields aligned (prepad by 2) and there are
 * more subtle issues to get the buffer aligned on
 * cache lines to get optimal DMA, but we don't
 * fret over that kind of stuff as yet.
 */

/* Note that all the pointers elen, .... may seem to
 * imply some kind of scatter/gather is going on.
 * This is emphatically NOT the case, these are simply
 * convenience pointers into the data buffer, which must
 * hold a contiguous packet.
 */

#ifndef __NETBUF_H__
#define __NETBUF_H__

#define NETBUF_MAX	1600

struct netbuf {
	struct netbuf *next;
	int flags;
	int refcount;
	char *bptr;		/* base pointer */
	// char *cptr;		/* controller info pointer */
	/* */
	struct eth_hdr *eptr;	/* pointer to ether header */
	struct ip_hdr *iptr;	/* pointer to IP or ARP header */
	char *pptr;		/* pointer to UDP, ICMP or other protocol header */
	char *dptr;		/* data payload */
	/* */
	int elen;		/* full packet size in buffer */
	int ilen;		/* size from IP header and on */
	int plen;		/* size from proto header (UDP) and on */
	int dlen;		/* size of payload */
	/* */
	char data[NETBUF_MAX];
};

struct netbuf * netbuf_alloc ( void );
struct netbuf * netbuf_alloc_i ( void );
void netbuf_free ( struct netbuf * );

#endif /* __NETBUF_H__ */
/* THE END */
