/* kyu_main.c */

/* 11-14-2022  Tom Trebisky.
 *
 * This is the Kyu top end interface
 * to the BSD tcp code.
 */

#include <bsd.h>

#include "kyu.h"
#include "thread.h"
#include "../net/net.h"		/* Kyu */
#include "../net/kyu_tcp.h"

#ifdef notdef
#include "kyu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
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

#include "mbuf.h"
#endif

/* From net/radix.c
 * -- something to do with routing.
 * should never get called.
 * referenced in the structure below.
 */
static int
rn_inithead ( void **head, int off )
{
	panic ( "rn_inithead called" );
}

/* Both of these from in_proto.c
 *  In the original there is an array "inetsw" of which the tcp_proto
 *  entry below is just one of many entries.
 */

extern struct domain inetdomain;

struct protosw tcp_proto =
{ SOCK_STREAM,  &inetdomain,    IPPROTO_TCP,    PR_CONNREQUIRED|PR_WANTRCVD,
  tcp_input,    0,              tcp_ctlinput,   tcp_ctloutput,
  tcp_usrreq,
  tcp_init,     tcp_fasttimo,   tcp_slowtimo,   tcp_drain };

struct domain inetdomain =
    { AF_INET, "internet", 0, 0, 0,
      // inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
      &tcp_proto, &tcp_proto, 0,
      rn_inithead, 32, sizeof(struct sockaddr_in) };

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

static void bsd_init ( void );
static void tcp_bsd_rcv ( struct netbuf * );
static void tcp_bsd_process ( struct netbuf * );

struct tcp_ops bsd_ops = {
	bsd_init,
	tcp_bsd_rcv
};

/* Kyu calls this during initialization */
struct tcp_ops *
tcp_bsd_init ( void )
{
	return &bsd_ops;
}

static void tcp_thread ( long );
static void tcp_timer_func ( long );

static void
bsd_init ( void )
{
	printf ( "BSD tcp init called\n" );

	tcp_globals_init ();

	// in mbuf.c
	mb_init ();

	// in tcp_subr.c
	tcp_init ();

	printf ( "timer rate: %d\n", timer_rate_get() );
	if ( timer_rate_get() != 1000 ) {
	    printf ( "Unexpected timer rate: %d\n", timer_rate_get() );
	    panic ( "TCP timer rate" );
	}

	(void) safe_thr_new ( "tcp-bsd", tcp_thread, (void *) 0, 12, 0 );
	(void) thr_new_repeat ( "tcp-timer", tcp_timer_func, (void *) 0, 13, 0, 100 );
}

static int fast_count = 0;
static int slow_count = 0;

/* This runs every 100 ticks */
static void
tcp_timer_func ( long xxx )
{
	++fast_count;
	if ( (fast_count % 2) == 0 )
	    tcp_fasttimo();

	++slow_count;
	if ( (slow_count % 5) == 0 )
	    tcp_slowtimo();
}

static struct netbuf *tcp_q_head;
static struct netbuf *tcp_q_tail;

static struct sem *tcp_q_sem;

/* We might be better off using a condition variable here.
 * If we get deadlocks we will need to change things.
 */
static struct sem *tcp_lock_sem;

static void
tcp_thread ( long xxx )
{
	struct netbuf *nbp;

	tcp_q_head = (struct netbuf *) 0;
	tcp_q_tail = (struct netbuf *) 0;

	tcp_q_sem = sem_signal_new ( SEM_FIFO );
	if ( ! tcp_q_sem )
	    panic ("Cannot get tcp queue signal semaphore");

	tcp_lock_sem = sem_mutex_new ( SEM_FIFO );
	if ( ! tcp_lock_sem )
	    panic ("Cannot get tcp queue lock semaphore");

	for ( ;; ) {
	    /* Do we have a packet to process ? */

	    sem_block ( tcp_lock_sem );
            nbp = NULL;
	    // printf ( " --- LIST1: H, T = %08x %08x\n", tcp_q_head, tcp_q_tail );
            if ( tcp_q_head ) {
                nbp = tcp_q_head;
	    // printf ( " --- LIST1B: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );
                tcp_q_head = nbp->next;
                if ( ! tcp_q_head )
                    tcp_q_tail = (struct netbuf *) 0;
            }

	    //if ( nbp )
	    //	printf ( " --- LIST2: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );
	    //else
	    //	printf ( " --- LIST2: H, T = %08x %08x\n", tcp_q_head, tcp_q_tail );

	    sem_unblock ( tcp_lock_sem );

            if ( nbp ) {
		printf ( "bsd_pull %08x, %d\n", nbp, nbp->ilen );
                tcp_bsd_process ( nbp );
                continue;
            }

            /* Wait for another packet.
             */
	    printf ( "TCP thread waiting\n" );
            sem_block_cpu ( tcp_q_sem );
	}

}

/* This is where arriving packets get delivered by Kyu
 *  From ip_rcv() in net/net_ip.c
 * Be sure and let Kyu free the packet.
 */
static void
tcp_bsd_rcv ( struct netbuf *nbp )
{
        nbp->next = (struct netbuf *) 0;
	printf ( "bsd_rcv %08x, %d\n", nbp, nbp->ilen );

	sem_block ( tcp_lock_sem );
	    // printf ( " --- LIST++1: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );
        if ( tcp_q_tail ) {
            tcp_q_tail->next = nbp;
            tcp_q_tail = nbp;
        } else {
            tcp_q_tail = nbp;
            tcp_q_head = nbp;
        }
	    // printf ( " --- LIST++2: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );
	sem_unblock ( tcp_lock_sem );

        sem_unblock ( tcp_q_sem );
}

void
tcp_bsd_show ( void )
{
    printf ( "Status for BSD tcp code:\n" );
    // ...
}

/* Here is where the TCP thread processes incoming packets
 */
static void
tcp_bsd_process ( struct netbuf *nbp )
{
	// struct netpacket *pkt;
	struct mbuf *m;
	struct ip *iip;
	int len;

	/* byte swap fields in IP header */
	nbp->iptr->len = ntohs ( nbp->iptr->len );
	nbp->iptr->id = ntohs ( nbp->iptr->id );
	nbp->iptr->offset = ntohs ( nbp->iptr->offset );

	// printf ( "TCP: bsd_process %d bytes\n", ntohs(nbp->iptr->len) );
	// printf ( "TCP: bsd_process %d, %d bytes\n", nbp->ilen, nbp->iptr->len );
	// dump_buf ( (char *) nbp->iptr, nbp->ilen );

	len = nbp->ilen;
	m = mb_devget ( (char *) nbp->iptr, len, 0, NULL, 0 );
	if ( ! m ) {
	    printf ( "** mb_devget fails\n" );
	    return;
	}

	/* We can free it now since we have copied everything into
	 * an mbuf.
	 */
	netbuf_free ( nbp );

	// printf ( "M_devget: %08x %d\n", m, nbp->ilen );
	// dump_buf ( (char *) m, 128 );

	/* Ah, but what goes on during IP processing by the BSD code */
	/* For one thing, it subtracts the size of the IP header
	 * off of the length field in the IP header itself.
	 */
	iip = mtod(m, struct ip *);
	iip->ip_len -= sizeof(struct ip);

	tcp_input ( m, len );
}

/* Called when TCP wants to hand a packet to IP for transmission
 */
int
ip_output ( struct mbuf *A, struct mbuf *B, struct route *R, int N,  struct ip_moptions *O )
{
        struct netbuf *nbp;
        struct ip_hdr *ipp;
	struct mbuf *mp;
        int len;
        int size = 0;
	char *buf;

	printf ( "TCP(bsd): ip_output\n" );
	printf ( " ip_output A = %08x\n", A );
	printf ( " ip_output B = %08x\n", B );
	printf ( " ip_output R = %08x\n", R );
	printf ( " ip_output N = %d\n", N );
	printf ( " ip_output O = %08x\n", O );
	mbuf_show ( A, "ip_output" );
	dump_buf ( (char *) A, 128 );
#ifdef notdef
#endif

        nbp = netbuf_alloc ();
        if ( ! nbp )
            return 1;

	// printf ( "IP output 1\n" );
	buf = (char *) nbp->iptr;
	for (mp = A; mp; mp = mp->m_next) {
                len = mp->m_len;
                if (len == 0)
                        continue;
                // bcopy ( mtod(mp, char *), buf, len );
                memcpy ( buf, mtod(mp, char *), len );
                buf += len;
                size += len;
        }
	// printf ( "IP output 2\n" );

        mb_freem ( A );

	// printf ( "IP output 3\n" );

        nbp->ilen = size;
        nbp->plen = size - sizeof(struct ip_hdr);

	/* BSD has given us a partially completed IP header.
	 * In particular, it contains src/dst IP numbers.
	 */

        nbp->pptr = (char *) nbp->iptr + sizeof ( struct ip_hdr );

	// not needed on output.
        // nbp->dptr = nbp->pptr + sizeof ( struct udp_hdr );

	ipp = nbp->iptr;

	printf ( "-IP output (ip_send) to %08x %d\n", ipp->dst, size );
	// Hand it to the Kyu IP layer
        ip_send ( nbp, ipp->dst );

	// printf ( "IP output 4\n" );
	return 0;
}

/* Called when TCP wants to send a control message.
 */
int
ip_ctloutput ( int i, struct socket *s, int j, int k, struct mbuf **mm )
{
	// printf ( "TCP(bsd): ctl output\n" );
	panic ( "TCP(bsd): ctl output\n" );
}

/* THE END */
