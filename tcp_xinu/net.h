/* net.h */

#define NETSTK			8192	/* Stack size for network setup */
#define NETOUTPRIO		500		/* Network startup priority 	*/
#define NETINPRIO		600		/* Network startup priority 	*/
#define NETBOOTFILE		128		/* Size of the netboot filename	*/

/* Constants used in the networking code */

#define	ETH_ARP     0x0806		/* Ethernet type for ARP	*/
#define	ETH_IP      0x0800		/* Ethernet type for IP		*/
#define	ETH_IPv6    0x86DD		/* Ethernet type for IPv6	*/

/* Format of an Ethernet packet carrying IPv4 and UDP */

#pragma pack(2)
struct	netpacket	{
	byte	net_ethdst[ETH_ADDR_LEN];/* Ethernet dest. MAC address	*/
	byte	net_ethsrc[ETH_ADDR_LEN];/* Ethernet source MAC address	*/
	uint16	net_ethtype;		/* Ethernet type field		*/
	byte	net_ipvh;		/* IP version and hdr length	*/
	byte	net_iptos;		/* IP type of service		*/
	uint16	net_iplen;		/* IP total packet length	*/
	uint16	net_ipid;		/* IP datagram ID		*/
	uint16	net_ipfrag;		/* IP flags & fragment offset	*/
	byte	net_ipttl;		/* IP time-to-live		*/
	byte	net_ipproto;		/* IP protocol (actually type)	*/
	uint16	net_ipcksum;		/* IP checksum			*/
	uint32	net_ipsrc;		/* IP source address		*/
	uint32	net_ipdst;		/* IP destination address	*/
	union {
	 struct {
	  uint16 	net_udpsport;	/* UDP source protocol port	*/
	  uint16	net_udpdport;	/* UDP destination protocol port*/
	  uint16	net_udplen;	/* UDP total length		*/
	  uint16	net_udpcksum;	/* UDP checksum			*/
	  byte		net_udpdata[1500-28];/* UDP payload (1500-above)*/
	 };
	 struct {
	  byte		net_ictype;	/* ICMP message type		*/
	  byte		net_iccode;	/* ICMP code field (0 for ping)	*/
	  uint16	net_iccksum;	/* ICMP message checksum	*/
	  uint16	net_icident; 	/* ICMP identifier		*/
	  uint16	net_icseq;	/* ICMP sequence number		*/
	  byte		net_icdata[1500-28];/* ICMP payload (1500-above)*/
	 };
	 struct {
	  uint16	net_tcpsport;	/* TCP Source port		*/
	  uint16	net_tcpdport;	/* TCP destination port		*/
	  int32		net_tcpseq;	/* TCP sequence no.		*/
	  int32		net_tcpack;	/* TCP acknowledgement sequence	*/
	  uint16	net_tcpcode;	/* TCP flags			*/
	  uint16	net_tcpwindow;	/* TCP receiver window		*/
	  uint16	net_tcpcksum;	/* TCP checksum			*/
	  uint16	net_tcpurgptr;	/* TCP urgent pointer		*/
	  byte		net_tcpdata[1500-40];/* TCP payload		*/
	 };
	};
};
#pragma pack()

#define	PACKLEN	sizeof(struct netpacket)

extern	bpid32	netbufpool;		/* ID of net packet buffer pool	*/

struct	network	{
	uint32	ipucast;
	uint32	ipbcast;
	uint32	ipmask;
	uint32	ipprefix;
	uint32	iprouter;
	uint32	bootserver;
	bool8	ipvalid;
	byte	ethucast[ETH_ADDR_LEN];
	byte	ethbcast[ETH_ADDR_LEN];
	char	bootfile[NETBOOTFILE];
};

extern	struct	network NetData;	/* Local Network Interface	*/
