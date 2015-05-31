/* skidoo netbuf.
 * Tom Trebisky  4/9/2005
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

/* Need some extra space for prepad stuff */
#define NETBUF_MAX	1600

struct netbuf {
	struct netbuf *next;
	int flags;
	char *bptr;		/* base pointer */
	char *cptr;		/* controller info pointer */
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

/* The Tx descriptor for the ee100 driver requires
 * 16 bytes, keeping a hole at least that big at the start
 * of the buffer allows us to just put it into the netbuf.
 *
 * (This isn't necessary for other drivers, but doesn't
 * hurt anything and may let the ee100 driver do clever things.)
 *
 * If we add other drivers we want to be clever with, we may
 * need the make this the max of the requirements.
 */
#define NETBUF_PREPAD	(8 * sizeof(long))

/* THE END */
