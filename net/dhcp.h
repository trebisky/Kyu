/* dhcp.h
 * Used by both bootp.c and dhcp.c
 *
 * T. Trebisky  4-11-2005
 * T. Trebisky  6-2-2015 8-10-2016
 */

#define BOOTP_SERVER	67	/* receive on this */
#define BOOTP_CLIENT	68	/* send on this */

struct bootp {
    	unsigned char op;
    	unsigned char htype;
    	unsigned char hlen;
    	unsigned char hops;
	/* -- */
	unsigned long xid;
	/* -- */
	unsigned short secs;
	unsigned short flags;	/* only for DHCP */ 
	/* -- */
	unsigned long client_ip;
	unsigned long your_ip;
	unsigned long server_ip;
	unsigned long gateway_ip;
	/* -- */
	char chaddr[16];
	char server_name[64];
	char bootfile[128];
#ifdef notdef
	char options[64];	/* BOOTP */
#endif
	char options[312];	/* DHCP */
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
