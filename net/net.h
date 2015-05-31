/* net.h -- Skidoo
 * Tom Trebisky  3-31-2005
 */

/*
#define DEBUG_IP
#define DEBUG_ARP
#define DEBUG_ARP_MUCHO
#define DEBUG_ICMP
#define DEBUG_UDP
#define DEBUG_TCP
*/

#define DEBUG_ICMP
#define DEBUG_TCP

#define ETH_ARP		0x0806
#define ETH_ARP_SWAP	0x0608

#define ETH_IP		0x0800
#define ETH_IP_SWAP	0x0008

#define ETH_ADDR_SIZE	6
#define ETH_MIN_SIZE	60
#define ETH_MAX_SIZE	1514

/* in netinet/in.h in bsd sources */
#define IPPROTO_ICMP	1
#define IPPROTO_TCP	6
#define IPPROTO_UDP	17

#define IP_BROADCAST	0xffffffff

char * ip2str ( unsigned long );
char * ether2str ( unsigned char * );

struct eth_hdr {
    	unsigned char dst[ETH_ADDR_SIZE];
    	unsigned char src[ETH_ADDR_SIZE];
	unsigned short type;
};

/* these bit fields work only on little endian hardware.
 */
struct ip_hdr {
	unsigned char	hl:4,	/* header length */
			ver:4;	/* version */
	unsigned char tos;	/* type of service */
	short len;		/* total length */
	/* - */
	unsigned short id;
	short offset;
	/* - */
	unsigned char ttl;	/* time to live */
	unsigned char proto;	/* protocol */
	unsigned short sum;
	/* - */
	unsigned long src;
	unsigned long dst;
};

#define IP_OFFMASK		0x1fff
#define IP_OFFMASK_SWAP		0xff1f
#define IP_DF			0x4000	/* don't fragment */
#define IP_MF			0x2000	/* more fragments */

/*  THE END */
