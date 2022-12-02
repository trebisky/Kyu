/* mbuf.c */

/* support code for BSD 4.4 TCP code in Kyu
 *
 * new set of mbuf routines
 */

#include <bsd.h>

/* See mbuf.h */
int max_linkhdr;
int max_protohdr;
int max_hdr;
// int max_datalen;

void mb_clget ( struct mbuf * );

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
	bpf3 ( "kyu_mbuf_alloc: %d %08x\n", MSIZE, rv );
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
/* socket, in_pcb, and tcpcb just imitate the above.
 * XXX - they don't really belong here and should be merged
 *  with the above into a general allocator (along with statistics and limits).
 */

static void *sock_list = NULL;
static void *inpcb_list = NULL;
static void *tcpcb_list = NULL;

void *
k_sock_alloc ( void )
{
	void *rv;
	int n = sizeof ( struct socket );

	if ( sock_list ) {
	    rv = sock_list;
	    sock_list = ((struct my_list *) rv) -> next;
	    return rv;
	}

	rv = kyu_malloc ( n );
	bpf3 ( "kyu_sock_alloc: %d %08x\n", n, rv );
	// memset ( rv, 0, n );
	return rv;
}

void
k_sock_free ( void *m )
{
	((struct my_list *) m) -> next = (struct my_list *) sock_list;
	sock_list = (void *) m;
}

/* -- */

void *
k_inpcb_alloc ( void )
{
	void *rv;
	int n = sizeof ( struct inpcb );

	if ( inpcb_list ) {
	    rv = inpcb_list;
	    inpcb_list = ((struct my_list *) rv) -> next;
	    return rv;
	}

	rv = kyu_malloc ( n );
	bpf3 ( "kyu_inpcb_alloc: %d %08x\n", n, rv );
	// memset ( rv, 0, n );
	return rv;
}

void
k_inpcb_free ( void *m )
{
	((struct my_list *) m) -> next = (struct my_list *) inpcb_list;
	inpcb_list = (void *) m;
}

/* -- */

void *
k_tcpcb_alloc ( void )
{
	void *rv;
	int n = sizeof ( struct tcpcb );

	if ( tcpcb_list ) {
	    rv = tcpcb_list;
	    tcpcb_list = ((struct my_list *) rv) -> next;
	    return rv;
	}

	rv = kyu_malloc ( n );
	bpf3 ( "kyu_tcpcb_alloc: %d %08x\n", n, rv );
	// memset ( rv, 0, n );
	return rv;
}

void
k_tcpcb_free ( void *m )
{
	((struct my_list *) m) -> next = (struct my_list *) tcpcb_list;
	tcpcb_list = (void *) m;
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
	    panic ( "mb_cl_alloc: clusters exhausted"); \

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

	if ( ! bpf_debug ( 3 ) )
	    return;

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

	/* The following stuff is all about leaving space at the start
	 * of an mbuf for link (i.e. ethernet) and protocol headers
	 * to avoid copies.  Given the way we handle things under Kyu,
	 * I think these could all be zero.
	 */
	/* From netiso/tuba_subr.c */
	/* 80 bytes ! */
	#define TUBAHDRSIZE (3 /*LLC*/ + 9 /*CLNP Fixed*/ + 42 /*Addresses*/ \
	     + 6 /*CLNP Segment*/ + 20 /*TCP*/)

	/* Enough room for ethernet header */
	max_linkhdr = 16;

	/* From kern/uipc_domain.c */
	max_protohdr = TUBAHDRSIZE;

	/* This may sometimes be used to leave space at the start of an mbuf
	 * for an ethernet header to be prepended, avoiding a copy to move
	 * the packet data.
	 */
	max_hdr = max_linkhdr + max_protohdr;

	/* MHLEN is defined in mbuf.h */
	// Never used
	// max_datalen = MHLEN - max_hdr;

	/* maximum chars per socket buffer, from socketvar.h */
	// sb_max = SB_MAX;

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
	m->m_data = m->m_pktdat;
	m->m_flags = M_PKTHDR;

	return m;
}

/* Add a cluster to an mbuf.
 * - used in mb_devget() below
 * - also used in sosend ()
 */
void
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
	struct mbuf *top = 0;
	struct mbuf **mp;
	int off = off0;
	int len;
	char *cp;
	char *epkt;

	mp = &top;

	cp = buf;
	epkt = cp + totlen;
	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}

	// MGETHDR(m, M_DONTWAIT, MT_DATA);
	m = mb_gethdr ( MT_DATA );
	// mbuf_game ( m, "mb_devget 1" );

	if (m == 0)
		return (0);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	while (totlen > 0) {
		if (top) {
			// MGET(m, M_DONTWAIT, MT_DATA);
			m = mb_get ( MT_DATA );
			// mbuf_game ( m, "mb_devget 2" );
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
			// mbuf_game ( m, "mb_devget 2b" );

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

	// mbuf_game ( top, "mb_devget 3" );
	return (top);
}

/* THE END */
