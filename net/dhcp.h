/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * dhcp.h
 * Used by both bootp.c and dhcp.c
 *
 * T. Trebisky  4-11-2005
 * T. Trebisky  6-2-2015 8-10-2016
 */
#include <arch/types.h>

#define BOOTP_SERVER	67	/* receive on this */
#define BOOTP_CLIENT	68	/* send on this */

#define DHCP_OPTION_SIZE	312

struct bootp {
    	unsigned char op;
    	unsigned char htype;
    	unsigned char hlen;
    	unsigned char hops;
	/* -- */
	u32 xid;
	/* -- */
	unsigned short secs;
	unsigned short flags;	/* only for DHCP */ 
	/* -- */
	u32 client_ip;
	u32 your_ip;
	u32 server_ip;
	u32 gateway_ip;
	/* -- */
	char chaddr[16];
	char server_name[64];
	char bootfile[128];
#ifdef notdef
	char options[64];	/* BOOTP */
#endif
	char options[DHCP_OPTION_SIZE];	/* DHCP */
};

/* dhcp expands options (vendor) field to 312 from 64 bytes.
 * so a bootp packet is 300 bytes, DHCP is 548
 * This is the minimum size (with headers, 576 bytes)
 */

/* flag to tell server to reply via broadcast
 * (else replies unicast)
 */
#define	F_BROAD		0x8000

#define BOOTP_SIZE	300
#define DHCP_SIZE	548

#define BOOTP_REQUEST   1
#define BOOTP_REPLY     2

#define BOOTP_ETHER	1

/* THE END */
