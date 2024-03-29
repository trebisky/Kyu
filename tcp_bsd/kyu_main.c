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

#include <stdarg.h>

/* Too many debug messages are getting out of control, hence this
 * taken from kyu: prf.c
 * Levels are defined in kyu_compat.h
 */

#define PRINTF_BUF_SIZE 128

static int bpf_level = 0;

void
bpf_setlevel ( int arg )
{
	bpf_level = arg;
}

/* To debug or not to debug */
int
bpf_debug ( int level )
{
	if ( bpf_level < level )
	    return 0;
	return 1;
}

void
bpf_dump ( int level, char *buf, int n )
{
	if ( bpf_level < level )
	    return;
	dump_buf ( buf, n );
}

void
// printf ( const char *fmt, ... )
bpf ( int level, const char *fmt, ... )
{
        char buf[PRINTF_BUF_SIZE];
        va_list args;

	if ( bpf_level < level )
	    return;

        va_start ( args, fmt );
        (void) vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
        va_end ( args );

        console_puts ( buf );
}

void
bpf1 ( const char *fmt, ... )
{
        char buf[PRINTF_BUF_SIZE];
        va_list args;

	if ( bpf_level < 1 )
	    return;

        va_start ( args, fmt );
        (void) vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
        va_end ( args );

        console_puts ( buf );
}

void
bpf2 ( const char *fmt, ... )
{
        char buf[PRINTF_BUF_SIZE];
        va_list args;

	if ( bpf_level < 2 )
	    return;

        va_start ( args, fmt );
        (void) vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
        va_end ( args );

        console_puts ( buf );
}

void
bpf3 ( const char *fmt, ... )
{
        char buf[PRINTF_BUF_SIZE];
        va_list args;

	if ( bpf_level < 3 )
	    return;

        va_start ( args, fmt );
        (void) vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
        va_end ( args );

        console_puts ( buf );
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

#ifdef notdef
/* From net/radix.c
 * -- something to do with routing.
 * should never get called.
 * referenced in the structure below.
 */
static int
rn_inithead ( void **head, int off )
{
	bsd_panic ( "rn_inithead called" );
}
#endif

/* Both of these from in_proto.c
 *  In the original there is an array "inetsw" of which the tcp_proto
 *  entry below is just one of many entries.
 */

// extern struct domain inetdomain;

#ifdef notdef
struct protosw tcp_proto =
{
    // SOCK_STREAM,
    // &inetdomain,
    // IPPROTO_TCP,
  tcp_input,    0,              tcp_ctlinput,   tcp_ctloutput,
  tcp_usrreq,
  tcp_init,     tcp_fasttimo,   tcp_slowtimo,   tcp_drain
};
#endif

#ifdef notdef
struct	domain {
	int	dom_family;		/* AF_xxx */
	char	*dom_name;
	void	(*dom_init)		/* initialize domain data structures */
		__P((void));
	int	(*dom_externalize)	/* externalize access rights */
		__P((struct mbuf *));
	int	(*dom_dispose)		/* dispose of internalized rights */
		__P((struct mbuf *));
	struct	protosw *dom_protosw, *dom_protoswNPROTOSW;
	struct	domain *dom_next;
	int	(*dom_rtattach)		/* initialize routing table */
		__P((void **, int));
	int	dom_rtoffset;		/* an arg to rtattach, in bits */
	int	dom_maxrtkey;		/* for routing layer */
};

#ifdef KERNEL
// struct	domain *domains;
#endif
#endif

#ifdef notdef
struct domain inetdomain =
    { AF_INET,		// dom_family
      "internet",		// dom+name
      0,			// fptr - dom_init
      0,			// fptr - dom_externalize
      0,			// fptr - dom_dispose
      // inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
      &tcp_proto,
      &tcp_proto,
      0,		// dom_next
      rn_inithead,	// dom_rtattach
      32,		// troffset
      sizeof(struct sockaddr_in)	// maxrtkey
      };
#endif

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

static void locker_init ( void );
void locker_show ( void );

/* Used by "t 1" command */
void
bsd_debug_info ( void )
{
	tcb_show ();
	locker_show ();
	tcp_statistics ();
	printf ( "Kyu input queue size: %d\n", net_get_inq_count() );
	printf ( "TCP input queue size: %d\n", tcp_get_inq_count() );
	printf ( "Kyu output queue size: %d\n", net_get_outq_count() );
	netbuf_show ();
	printf ( "Clock: %d\n", get_timer_count_s () );
}

/* Wrapper on Kyu panic - gives stack traceback */
void
bsd_panic ( char *msg )
{
	printf ( "BSD panic: %s\n", msg );
	unroll_cur_short ();
	bsd_debug_info ();
	panic ( "BSD tcp ----- end" );
}

#define	PRI_TCP_MAIN	24
#define	PRI_TCP_TIMER	25

static void
bsd_init ( void )
{
	bpf_setlevel ( 2 );
	bpf3 ( "BSD tcp init called\n" );

	tcp_globals_init ();

	locker_init ();

	// in mbuf.c
	mb_init ();

	// in tcp_subr.c
	tcp_init ();

	bpf3 ( "timer rate: %d\n", timer_rate_get() );
	if ( timer_rate_get() != 1000 ) {
	    printf ( "Unexpected timer rate: %d\n", timer_rate_get() );
	    bsd_panic ( "TCP timer rate" );
	}

	(void) safe_thr_new ( "tcp-input", tcp_thread, (void *) 0, PRI_TCP_MAIN, 0 );
	(void) thr_new_repeat ( "tcp-timer", tcp_timer_func, (void *) 0, PRI_TCP_TIMER, 0, 100 );
}

/* These replace splnet, splimp, splx */

// Set in kyu_compat.h
// #define BIG_LOCKS

extern struct thread *cur_thread;

struct locker {
	struct thread *thread;
	struct sem *sem;
	int count;
	/* -- */
	int user_busy;
	int timer_busy;
	int input_busy;
};

static struct sem *net_lock_sem;

static struct locker master_lock;

#ifdef BIG_LOCKS
/* The idea here it to do away with all of this internal fine grained locking
 * and just put locks around the big pieces, namely:
 * 
 *  -- tcp_input()
 *  -- timer calls
 *  -- user calls
 *  ---  send
 *  ---  receive
 *  ---  bind
 *  ---  accept
 *  ---  connect
 *  ---  close
 */
void
net_lock ( void )
{
}

void
net_unlock ( void )
{
}

void
big_lock ( void )
{
	sem_block ( master_lock.sem );
}

void
big_unlock ( void )
{
	sem_unblock ( master_lock.sem );
}

void
user_lock ( void )
{
	master_lock.user_busy = 1;
	sem_block ( master_lock.sem );
}

void
user_unlock ( void )
{
	sem_unblock ( master_lock.sem );
	master_lock.user_busy = 0;
}

/* Same as unlock, but mark different state */
void
user_waiting ( void )
{
	sem_unblock ( master_lock.sem );
	master_lock.user_busy = 2;
}


#else

void
big_lock ( void )
{
}

void
big_unlock ( void )
{
}
/* Logic here allows for nested splnet
 * which is absolutely necessary
 */
void
net_lock ( void )
{
	if ( master_lock.count && master_lock.thread == cur_thread ) {
	    printf ( "$$$ net_lock, deadlock avoided\n" );
	    unroll_cur_short ();
	    // studying the above unroll traceback might be a way to
	    // someday cleanup the code and avoid the nesting (maybe).
	    ++master_lock.count;
	    return;
	}

	sem_block ( master_lock.sem );
	master_lock.thread = cur_thread;
	master_lock.count = 1;
}

void
net_unlock ( void )
{
	if ( master_lock.count < 1 ) {
	    printf ( "$$$ net_unlock, count = %d ???\n", master_lock.count );
	    unroll_cur_short ();
	    return;
	}
	--master_lock.count;
	if ( master_lock.count == 0 )
	    sem_unblock ( master_lock.sem );
}
#endif

char *
locker_state ( int st )
{
	if ( st == 0 )
	    return "idle";
	if ( st == 1 )
	    return "busy";
	if ( st == 2 )
	    return "wait (idle)";
	return "unknown??";
}

void
locker_show ( void )
{
	printf ( "locker count: %d\n", master_lock.count );
	if ( master_lock.count > 0 )
	    printf ( "locker thread: %s (%08x)\n",
		master_lock.thread->name, master_lock.thread );
	printf ( "Input thread: %s\n", locker_state ( master_lock.input_busy ) );
	printf ( "Timer thread: %s\n", locker_state ( master_lock.timer_busy ) );
	printf ( "User lock: %s\n", locker_state ( master_lock.user_busy ) );
}

int
get_lock_count ( void )
{
	return master_lock.count;
}

static void
locker_init ( void )
{
	master_lock.count = 0;
	master_lock.thread = NULL;
	master_lock.sem = sem_mutex_new ( SEM_FIFO );
	sem_set_name ( master_lock.sem, "tcp-main" );
}

static int fast_count = 0;
static int slow_count = 0;

#ifdef BIG_LOCKS
static void
tcp_timer_func ( long xxx )
{
	++fast_count;
	if ( (fast_count % 2) == 0 ) {
	    sem_block ( master_lock.sem );
	    master_lock.timer_busy = 1;
	    tcp_fasttimo();
	    master_lock.timer_busy = 0;
	    sem_unblock ( master_lock.sem );
	}

	++slow_count;
	if ( (slow_count % 5) == 0 ) {
	    sem_block ( master_lock.sem );
	    master_lock.timer_busy = 1;
	    tcp_slowtimo();
	    master_lock.timer_busy = 0;
	    sem_unblock ( master_lock.sem );
	}
}

#else
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
#endif

static struct netbuf *tcp_q_head;
static struct netbuf *tcp_q_tail;

// static struct sem *tcp_q_sem;
static struct cv *tcp_queue_cv;

/* We might be better off using a condition variable here.
 * If we get deadlocks we will need to change things.
 */
// static struct sem *tcp_queue_lock_sem;

extern int wang_debug;

static int tcp_inq_count = 0;

int
tcp_get_inq_count ( void )
{
	return tcp_inq_count;
}

static void
tcp_thread ( long xxx )
{
	struct netbuf *nbp;
	// int npk;

	tcp_q_head = (struct netbuf *) 0;
	tcp_q_tail = (struct netbuf *) 0;

	// tcp_queue_lock_sem = sem_mutex_new ( SEM_FIFO );
	// if ( ! tcp_queue_lock_sem )
	//     bsd_panic ("Cannot get tcp queue lock semaphore");
	// sem_set_name ( tcp_queue_lock_sem, "tcp-q-lk" );

	// tcp_queue_cv = cv_new ( tcp_queue_lock_sem );
	tcp_queue_cv = cv_new ();
	cv_set_sname ( tcp_queue_cv, "tcp-inq" );
	cv_set_mname ( tcp_queue_cv, "tcp-q-lk" );

#ifdef notdef
	tcp_q_sem = sem_signal_new ( SEM_FIFO );
	if ( ! tcp_q_sem )
	    bsd_panic ("Cannot get tcp queue signal semaphore");
	sem_set_name ( tcp_q_sem, "tcp-inq" );

#endif

	for ( ;; ) {
	    /* Do we have a packet to process ? */

	    // sem_block ( tcp_queue_lock_sem );
	    cv_lock ( tcp_queue_cv );

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

            if ( nbp ) {
		tcp_inq_count--;
		// sem_unblock ( tcp_queue_lock_sem );
		cv_unlock ( tcp_queue_cv );
		// bpf2 ( "bsd_pull %08x, %d\n", nbp, nbp->ilen );
                tcp_bsd_process ( nbp );
		// npk++;
                continue;
            }

            /* Wait for another packet.
             */
	    // bpf2 ( "TCP thread waiting\n" );
	    // if ( wang_debug )
	    // 	printf ( "tcp-input - processed %d packets on this wakeup\n", npk );

            // sem_block_cpu ( tcp_q_sem );
	    cv_wait ( tcp_queue_cv );

	    // npk = 0;
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
	// bpf2 ( "bsd_rcv %08x, %d\n", nbp, nbp->ilen );

	// XXX
	// for debugging orange Pi
	// emac_show_times ();

	// sem_block ( tcp_queue_lock_sem );
	cv_lock ( tcp_queue_cv );

	    // printf ( " --- LIST++1: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );
        if ( tcp_q_tail ) {
            tcp_q_tail->next = nbp;
            tcp_q_tail = nbp;
        } else {
            tcp_q_tail = nbp;
            tcp_q_head = nbp;
        }
	tcp_inq_count++;
	    // printf ( " --- LIST++2: H, T = %08x %08x %08x\n", tcp_q_head, tcp_q_tail, nbp->next );

	// sem_unblock ( tcp_queue_lock_sem );
	cv_unlock ( tcp_queue_cv );

	/* Announce packet arrival */
        // sem_unblock ( tcp_q_sem );
	cv_signal ( tcp_queue_cv );
}

#ifdef notdef
void
tcp_bsd_show ( void )
{
    printf ( "Status for BSD tcp code:\n" );
    // ...
}
#endif

/* Here is where the TCP thread processes incoming packets
 */
static void
tcp_bsd_process ( struct netbuf *nbp )
{
	// struct netpacket *pkt;
	struct mbuf *m;
	struct ip *iip;
	int len;

	// pointless - any bad IP checksums would have already been dropped.
	// printf ( "IP checksum before swapping: %04x\n", in_cksum ( xx, xx );

	/* byte swap fields in IP header */
	nbp->iptr->len = ntohs ( nbp->iptr->len );
	nbp->iptr->id = ntohs ( nbp->iptr->id );
	nbp->iptr->offset = ntohs ( nbp->iptr->offset );

	// printf ( "TCP: bsd_process %d bytes\n", ntohs(nbp->iptr->len) );
	// printf ( "TCP: bsd_process %d, %d bytes\n", nbp->ilen, nbp->iptr->len );
	// dump_buf ( (char *) nbp->iptr, nbp->ilen );

#ifdef BIG_LOCKS
	sem_block ( master_lock.sem );
	master_lock.input_busy = 1;

	len = nbp->ilen;
	m = mb_devget ( (char *) nbp->iptr, len, 0, NULL, 0 );
	if ( ! m ) {
	    printf ( "** mb_devget fails\n" );
	    return;
	}

	// mbuf_game ( m, "tcp_bsd_process" );

	/* We can free it now since we have copied everything into
	 * an mbuf.
	 */
	netbuf_free ( nbp );

	// printf ( "M_devget: %08x %d\n", m, nbp->ilen );
	// dump_buf ( (char *) m, 128 );

	/* Ah, but what goes on during IP processing by the BSD code */
	/* For one thing, it subtracts the size of the IP header
	 * off of the length field in the IP header itself.
	 * So we should do this to "emulate" that behavior.
	 * This caused me no end of confusion later since the len field
	 * in the IP datagram that tcp_input has is then "wrong"
	 */
	iip = mtod(m, struct ip *);
	iip->ip_len -= sizeof(struct ip);

	/* Given the way I implemented net_lock/unlock, I cannot just
	 * call those here -- I must for sure get the lock, whereas
	 * net_lock will breeze right past getting the lock if it has
	 * already been acquired.
	 */

	// XXX - hook for timing test
	et_rtcp();

	tcp_input ( m, len );
	master_lock.input_busy = 0;
	sem_unblock ( master_lock.sem );
#else
	// XXX will never work
	tcp_input ( m, len );
#endif
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
        int size;
	char *buf;
	// int zz;

	// XXX - hook for timing test
	et_stcp ();

#ifdef notdef
	bpf3 ( "TCP(bsd): ip_output\n" );
	bpf3 ( " ip_output A = %08x\n", A );
	bpf3 ( " ip_output B = %08x\n", B );
	bpf3 ( " ip_output R = %08x\n", R );
	bpf3 ( " ip_output N = %d\n", N );
	bpf3 ( " ip_output O = %08x\n", O );

	mbuf_show ( A, "ip_output" );
	// dump_buf ( (char *) A, 128 );
	bpf_dump ( 3, (char *) A, 128 );
#endif

        nbp = netbuf_alloc ();
        if ( ! nbp )
            return 1;

	// XXX - delay below here
	// printf ( "IP output 1\n" );
	buf = (char *) nbp->iptr;
	size = 0;

	/* During some experiments we find that this loop makes 2 passes.
	 * First it copies 52 bytes (headers) then 500 bytes (payload).
	 */
	// zz = 0;
	for (mp = A; mp; mp = mp->m_next) {
                len = mp->m_len;
                if (len == 0)
                        continue;
		
		if ( ! valid_ram_address ( buf ) ) {
		    printf ( "ip_output, buf = %08x\n", buf );
		    netbuf_show ();
		    panic ( "ip_output = bad netbuf addr\n" );
		}

	et_A();
                // bcopy ( mtod(mp, char *), buf, len );
                memcpy ( buf, mtod(mp, char *), len );
		// zz++;
		// printf ( "memcpy(%d) %d bytes\n", zz, len );
	et_B();

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

	// XXX - delay above here
	// bpf2 ( "-IP output (ip_send) to %08x %d\n", ipp->dst, size );
	// Hand it to the Kyu IP layer
        ip_send ( nbp, ipp->dst );

	// bpf3 ( "IP output 4\n" );
	return 0;
}

/* Called when TCP wants to send a control message.
 */
int
ip_ctloutput ( int i, struct socket *s, int j, int k, struct mbuf **mm )
{
	// bpf1 ( "TCP(bsd): ctl output\n" );
	bsd_panic ( "TCP(bsd): ctl output\n" );
}

/* THE END */
