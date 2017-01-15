/* kyu_glue.c */

/* Provide Xinu functionality in terms of
 *  Kyu function calls.
 */

#include <xinu.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"

struct host_info  host_info;

int
semcreate ( int arg )
{
	if ( arg == 1 )
	    sem_mutex_new ( SEM_FIFO );
	else
	    sem_signal_new ( SEM_FIFO );
}

#ifdef notdef
typedef	int	bpid32;
bpid32  netbufpool;

/* from include/udp.h */
#define UDP_SLOTS       6               /* Number of open UDP endpoints */
#define UDP_QSIZ        8               /* Packets enqueued per endpoint*/

/* from include/icmp.h */
#define ICMP_SLOTS      10              /* num. of open ICMP endpoints  */
#define ICMP_QSIZ       8               /* incoming packets per slot    */
#endif

void
tcp_xinu_init ( void )
{

#ifdef notdef
	int nbufs;

	/* From Xinu net/net.c */
	bufinit ();
	nbufs = UDP_SLOTS * UDP_QSIZ + ICMP_SLOTS * ICMP_QSIZ + 1;
	netbufpool = mkbufpool(PACKLEN, nbufs);
#endif

	tcp_init ();
}

void
tcp_xinu_rcv ( struct netbuf *nbp )
{
	/* Must give eptr since we may have PREPAD */
	tcp_in ( (struct netpacket *) nbp->eptr );
	netbuf_free ( nbp );
}

/* XXX - just a stub so we can link for now */
void
ip_enqueue ( struct netpacket *pkt ) 
{
	struct netbuf **nbpt;

	nbpt = (struct netbuf **) ((char *)pkt - sizeof(struct netbuf *));
	ip_send ( *nbpt, pkt->net_ipdst );
}

/* We now hide a pointer to the netbuf inside the netbuf
 * in the 4 bytes just in front of the packet itself.
 */
struct netpacket *
get_netpacket ( void )
{
	struct netbuf *nbp;

	nbp = netbuf_alloc ();
	if ( nbp )
	    return (struct netpacket *) nbp->eptr;
	return NULL;
}

void
free_netpacket ( struct netpacket *pkt )
{
	struct netbuf **nbpt;

	nbpt = (struct netbuf **) ((char *)pkt - sizeof(struct netbuf *));
	netbuf_free ( *nbpt );
}

#ifdef notdef
/* Without the pragma, this is 1516 bytes in size
 * with it, the size is 1514.
 * The pragma (in this case) only affects the overall size of
 *  the structure.  The packing of elements inside is the same
 *  with or without it.
 * Notice that 1516 = 379 * 4
 */

#pragma pack(2)
// struct	netpacket	{
struct	netpk	{
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
	// uint32		zzz;
};
#pragma pack()

void
net_bozo ( void )
{
	struct netpk *np;

	np = (struct netpk *) 0;

	printf ( "netpacket is %d bytes\n", sizeof(struct netpk) );
	printf ( " start: %08x\n", np->net_ethdst );
	printf ( " ethtype: %08x\n", &np->net_ethtype );
	printf ( " ipvh: %08x\n", &np->net_ipvh );
	printf ( " iptos: %08x\n", &np->net_iptos );
	printf ( " iplen: %08x\n", &np->net_iplen );
	printf ( " ipdst: %08x\n", &np->net_ipdst );
	//printf ( " zzz: %08x %d\n", &np->zzz, &np->zzz );
}
#endif

/* THE END */
