/* mbuf.c */

/* support code for BSD 4.4 TCP code in Kyu
 *
 * new set of mbuf routines
 */

#include "kyu.h"
#include "mbuf.h"

// #include <sys/mbuf.h>

// For MCLBYTES
#include <sys/param.h>
#include <sys/types.h>

// for min
#include <sys/systm.h>

// typedef	char *		caddr_t;

#ifdef notdef
#include "thread.h"
#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

#include <sys/param.h>
// #include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
// #include <sys/protosw.h>
// #include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <in.h>
#include <in_systm.h>
#include <ip.h>
#include <in_pcb.h>
#include <ip_var.h>

#include <tcp.h>
#include <tcp_seq.h>
#include <tcp_timer.h>
#include <tcpip.h>
#include <tcp_var.h>
#include <tcp_debug.h>
#endif

#define	MSIZE		128		/* XXX */
#define	MLEN		(MSIZE - sizeof(struct m_hdr))	/* normal data len */
#define	MHLEN		(MLEN - sizeof(struct pkthdr))	/* data len w/pkthdr */
#define	MINCLSIZE	(MHLEN + MLEN)	/* smallest amount to put in cluster */

/* header at beginning of each mbuf: */
struct m_hdr {		/* 20 bytes */
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	int	mh_len;			/* amount of data in this mbuf */
	caddr_t	mh_data;		/* location of data */
	short	mh_type;		/* type of data in this mbuf */
	short	mh_flags;		/* flags; see below */
};

/* record/packet header in first mbuf of chain; valid if M_PKTHDR set */
struct	pkthdr {
	int	len;		/* total packet length */
	struct	ifnet *rcvif;	/* rcv interface */
};

/* description of external storage mapped into mbuf, valid if M_EXT set */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)();		/* free routine if not the usual */
	unsigned int	ext_size;	/* size of buffer, for ext_free */
};

struct mbuf {
	struct	m_hdr m_hdr;
	union {
		struct {
			struct	pkthdr MH_pkthdr;	/* M_PKTHDR set */
			union {
				struct	m_ext MH_ext;	/* M_EXT set */
				char	MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char	M_databuf[MLEN];		/* !M_PKTHDR, !M_EXT */
	} M_dat;
};

#define	m_next		m_hdr.mh_next
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags

#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

/* mbuf flags */
#define	M_EXT		0x0001	/* has associated external storage */
#define	M_PKTHDR	0x0002	/* start of record */
#define	M_EOR		0x0004	/* end of record */

/* mbuf pkthdr flags, also in m_flags */
#define	M_BCAST		0x0100	/* send/received as link-level broadcast */
#define	M_MCAST		0x0200	/* send/received as link-level multicast */

/* flags copied when copying m_pkthdr */
#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_BCAST|M_MCAST)

/* mbuf types */
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#define	MT_SONAME	8	/* socket name */
#define	MT_SOOPTS	10	/* socket options */
#define	MT_FTABLE	11	/* fragment reassembly header */
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* interface address */
#define MT_CONTROL	14	/* extra-data protocol message */
#define MT_OOBDATA	15	/* expedited data  */

#define mtod(m,t)	((t)((m)->m_data))

extern int max_linkhdr;

static void mb_clget ( struct mbuf * );

/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */

/* Here is our mbuf allocation scheme.
 */

void * kyu_malloc ( unsigned long );

/* ---- */

/* TODO -- add statistics
 *
 * "alloc" - how many in use.
 * "free" - how many on free list.
 * "max" - high water of in use.
 * -- also impose limit on alloc to avoid runaway bugs
 */

struct my_list {
	struct my_list *next;
};

static void *mbuf_list = NULL;

void *
k_mbuf_alloc ( void )
{
	void *rv;

	if ( mbuf_list ) {
	    rv = mbuf_list;
	    mbuf_list = ((struct my_list *) rv) -> next;
	    return rv;
	}

	rv = kyu_malloc ( MSIZE );	/* 128 */
	printf ( "kyu_mbuf_alloc: %d %08x\n", MSIZE, rv );
	memset ( rv, 0xab, MSIZE );
	return rv;
}

void
k_mbuf_free ( void *m )
{
	((struct my_list *) m) -> next = (struct my_list *) mbuf_list;
	mbuf_list = (void *) m;
}

/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */
/* Cluster stuff */

/* This is a tangled business in the original code as follows.
 * Where do the reference counts go?
 * They are kept in a separate array (mclrefcnt} of bytes.
 * The index for this needs to be determined given just
 * the address of the cluster.  So the clusters are kept
 * in a contiguous block of memory.
 * The macro mtocl(p) generates the index.
 * The reference count is only needed because copies are performed
 * not by copying, but by copying a pointer and bumping the refcnt.
 *
 * My desire is to first of all, capture all of this in one place,
 * namely right here.  The original code fiddled with the VM system
 * to get a block of pages, but I don't care about that.
 *
 * As I think of this a bit, I am tempted to get rid of the refcnt
 * business altogether and just copy the cluster contents into
 * a new cluster.  But I probably won't.
 */

static void *mbufcl_list = NULL;

#ifdef notdef
	/* From i386/i386/machdep.c */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
                                   M_MBUF, M_NOWAIT);
        bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
        mb_map = kmem_suballoc(kernel_map, (vm_offset_t)&mbutl, &maxaddr,
                               VM_MBUF_SIZE, FALSE);
#endif

static unsigned char mclrefcnt[NMBCLUSTERS];
static char clusters[NMBCLUSTERS*MCLBYTES];

// #define	mtocl(x)	(((u_long)(x) - (u_long)mbutl) >> MCLSHIFT)
// #define	mtocl(x)	(((u_long)(x) - (u_long)clusters) / MCLBYTES)
#define	mtocl(x)	(((u_long)(x) - (u_long)clusters) >> MCLSHIFT)

void
mb_cl_init ( void )
{
	int i;
	char *cl;

	bzero ( mclrefcnt, NMBCLUSTERS );

	mbufcl_list = NULL;

	cl = clusters;
	for ( i=0; i<NMBCLUSTERS; i++ ) {
	    ((struct my_list *) cl) -> next = (struct my_list *) mbufcl_list;
	    mbufcl_list = (void *) cl;
	    cl += MCLBYTES;
	}
}

void *
mbufcl_alloc ( void )
{
	void *rv;

	if ( ! mbufcl_list )
	    panic ( "mb_clget: clusters exhausted"); \

	rv = mbufcl_list;
	mbufcl_list = ( (struct my_list *) rv) -> next;

	mclrefcnt[mtocl(rv)] = 1;

	return rv;
}

void
mbufcl_free ( void *cl )
{
	if ( --mclrefcnt[mtocl(cl)] )
	    return;

	((struct my_list *) cl) -> next = (struct my_list *) mbufcl_list;
	mbufcl_list = (void *) cl;
}

/* bump a cluster reference count */
void
mb_clref_bump ( void *p )
{
	++mclrefcnt[mtocl(p)];
}

/* -------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------- */

void
mbuf_show ( struct mbuf *mp, char *msg )
{
	int off;

	printf ( "mbuf_show (%s): %08x\n", msg, mp );
	printf ( " size(hdr): %d\n", sizeof(struct m_hdr) );
	printf ( " size(int): %d\n", sizeof(int) );
	printf ( " size(*): %d\n", sizeof(caddr_t) );
	printf ( " next:    %08x\n", mp->m_next );
	printf ( " nextpkt: %08x\n", mp->m_nextpkt );
	printf ( " len: %d\n", mp->m_len );
	  off = mp->m_data - mp->m_dat;
	printf ( " data: %08x (%d)\n", mp->m_data, off );
	printf ( " type: %d\n", mp->m_type );
	printf ( " flags: %08x\n", mp->m_flags );
}

/* I am moving routines from uipc_mbuf.c to this file.
 * in the process, I change the names from m_ to mb_
 * My goal is simplification and centralization.
 * I am willing to sacrifice efficiency.
 * In particular, I am trying to do away with the
 *  macros in sys/mbuf.h that do yield nice inline code.
 */

void
mb_init ( void )
{
	struct mbuf *m;

	mb_cl_init ();

	/* More of a diagnostic than anything else */
	m = mb_get ( MT_DATA );
	mb_freem ( m );
	m = mb_get ( MT_DATA );
	mb_clget ( m );
	mb_freem ( m );
}

struct mbuf *
mb_free ( struct mbuf *m )
{
        struct mbuf *n;

        if (m == NULL)
	    return NULL;

	if ( m->m_flags & M_EXT)
	    mbufcl_free ( m->m_ext.ext_buf );
	n = m->m_next;
	k_mbuf_free ( m );

        return (n);
}

void
mb_freem ( struct mbuf *m )
{
        struct mbuf *n;

        if (m == NULL)
	    return;

        do {
	    if ( m->m_flags & M_EXT)
		mbufcl_free ( m->m_ext.ext_buf );
	    n = m->m_next;
	    k_mbuf_free ( m );
        } while (m = n);
}

/* replaces MGET
 */
struct mbuf *
mb_get ( int type )
{
	struct mbuf *m;

        m = (struct mbuf *) k_mbuf_alloc ();
	if ( ! m )
	    panic ( "mbuf mb_get exhausted");

	m->m_type = type;
	m->m_next = (struct mbuf *) NULL;
	m->m_nextpkt = (struct mbuf *) NULL;
	m->m_data = m->m_dat;
	m->m_flags = 0;

	return m;
}

/* replaces MGETHDR
 */
struct mbuf *
mb_gethdr ( int type )
{
	struct mbuf *m;

        m = (struct mbuf *) k_mbuf_alloc ();
	if ( ! m )
	    panic ( "mbuf mb_gethdr exhausted");

	m->m_type = type;
	m->m_next = (struct mbuf *) NULL;
	m->m_nextpkt = (struct mbuf *) NULL;
	m->m_data = m->m_dat;
	m->m_flags = M_PKTHDR;

	return m;
}

/* Add a cluster to an mbuf.
 * - one and only use in mb_devget() below
 */
static void
mb_clget ( struct mbuf *m )
{
	void *p;

	p = mbufcl_alloc ();

        m->m_ext.ext_buf = p;
	m->m_data = m->m_ext.ext_buf;
	m->m_flags |= M_EXT;
	m->m_ext.ext_size = MCLBYTES;
}

/*
 * Routine to copy from device local memory into mbufs.
 * offset parameter might be used for hardware that
 * puts the ethernet header at the end (trailing packet).
 * I have yet to encounter one of these.
 */
struct mbuf *
mb_devget ( char *buf, int totlen, int off0,
	struct ifnet *ifp, void (*copy)()  )
{
	struct mbuf *m;
	struct mbuf *top = 0, **mp = &top;
	int off = off0, len;
	char *cp;
	char *epkt;

	cp = buf;
	epkt = cp + totlen;
	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}

	// MGETHDR(m, M_DONTWAIT, MT_DATA);
	m = mb_gethdr ( MT_DATA );

	if (m == 0)
		return (0);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	while (totlen > 0) {
		if (top) {
			// MGET(m, M_DONTWAIT, MT_DATA);
			m = mb_get ( MT_DATA );
			if (m == 0) {
				// m_freem(top);
				mb_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			// MCLGET(m, M_DONTWAIT);
			mb_clget ( m );

			/* ?? won't this always be true? */
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}

		if (copy)
			copy(cp, mtod(m, caddr_t), (unsigned)len);
		else
			bcopy(cp, mtod(m, caddr_t), (unsigned)len);

		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = buf;
	}

	return (top);
}

/* THE END */
