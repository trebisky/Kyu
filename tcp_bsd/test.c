/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

#include <bsd.h>

// gets us htonl and such
#include <kyu.h>
#include <thread.h>
#include <tests.h>

#include <stdarg.h>

#define PRI_TCP_TEST	30

/* place these lower than the shell (which used to be at 50) to get some
 * decent response from the shell while this is running a big test.
 * Ha!  This does not work.  The shell is running at 40, but it spins
 * in a uart polling loop.
 * To make this debug work, we redid the uart driver to allow interrupt
 * driven console input (otherwise the shell would hog the cpu polling
 * the uart), and now run the shell at priority 11.
 */
#define WANG_PRI	31
#define WANG_DOG_PRI	32
#define WANG_PRI2	35


/* Here for debugging */
extern struct thread *cur_thread;

void tcb_show ( void );

struct socket * tcp_bind ( int );
struct socket * tcp_accept ( struct socket * );

void bsd_test_show ( long );
void bsd_test_server ( long );
void bsd_test_blab ( long );
void bsd_test_echo ( long );
void bsd_test_wangdoodle ( long );
void bsd_test_doney ( long );
void bsd_test_connect ( long );
void bsd_test_lots ( long );
void bsd_test_pig ( long );
void bsd_test_info ( long );
void bsd_test_debug ( long );

/* XXX - stolen from tcp_xinu, needs rework */
/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
        bsd_test_show,          "Show TCP info",                        0,
        bsd_test_server,        "Start pirate server",                  0,
        bsd_test_blab,          "Start blab server",                    0,
        bsd_test_echo,          "Start echo server",                    0,
        bsd_test_wangdoodle,    "Start wangdoodle server",              0,
        bsd_test_doney,         "Start doney server",                   0,
	bsd_test_connect,	"Connect (client) test",		0,
	bsd_test_lots,		"Connect (client) with lots",		0,
	bsd_test_info,		"Show WANG info",			0,
	bsd_test_debug,		"Set debug 0",				0,
	bsd_test_debug,		"Set debug 1",				1,
#ifdef notdef
	bsd_test_pig,		"CPU pig",				0,
	bsd_test_debug,		"Set debug 2",				2,
	bsd_test_debug,		"Set debug 3",				3,
	bsd_test_debug,		"Set debug 4",				4,
        xtest_client_echo,      "Echo client [n]",                      0,
        xtest_client_echo2,     "Echo client (single connection) [n]",  0,
        xtest_client_daytime,   "Daytime client [n]",                   0,
        xtest_server_daytime,   "Start daytime server",                 0,
        xtest_server_classic,   "Start classic server",                 0,
#endif
        0,                      0,                                      0
};

void
bsd_test_debug ( long val )
{
	printf ( "TCP debug level set to %d\n", val );
	bpf_setlevel ( (int) val );
}

/* ============================ */

void
bsd_test_show ( long xxx )
{
	bsd_debug_info ();
}

#ifdef notdef
/* From tcp_fsm.h */
#define	TCPS_CLOSED		0	/* closed */
#define	TCPS_LISTEN		1	/* listening for connection */
#define	TCPS_SYN_SENT		2	/* active, have sent syn */
#define	TCPS_SYN_RECEIVED	3	/* have send and received syn */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define	TCPS_ESTABLISHED	4	/* established */
#define	TCPS_CLOSE_WAIT		5	/* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define	TCPS_FIN_WAIT_1		6	/* have closed, sent fin */
#define	TCPS_CLOSING		7	/* closed xchd FIN; await FIN ACK */
#define	TCPS_LAST_ACK		8	/* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define	TCPS_FIN_WAIT_2		9	/* have closed, fin is acked */
#define	TCPS_TIME_WAIT		10	/* in 2*msl quiet wait after close */
#endif

static char *
tcb_state ( int val )
{
	static char buf[24];

	switch ( val ) {
	    case TCPS_CLOSED:
		return "Closed";
	    case TCPS_LISTEN:
		return "Listen";
	    case TCPS_SYN_SENT:
		return "SYN sent";
	    case TCPS_SYN_RECEIVED:
		return "SYN received";
	    case TCPS_ESTABLISHED:
		return "Establshed";
	    case TCPS_CLOSE_WAIT:
		return "Close-Wait";
	    case TCPS_TIME_WAIT:
		return "Time-Wait";
	    default:
		(void) sprintf ( buf, "%d", val );
		return buf;
	}
}

/* For debugging */
void
tcb_show ( void )
{
        struct inpcb *inp;
	struct tcpcb *tp;
	char *tstate;

        inp = &tcb;

        if ( inp->inp_next == &tcb )
	    printf ( "INPCB: empty\n" );

        while ( inp->inp_next != &tcb ) {
            inp = inp->inp_next;
	    tp = (struct tcpcb *) inp->inp_ppcb;
	    tstate = tcb_state ( tp->t_state );
            printf ( "INPCB: %08x %15s -- local, foreign: ", inp, tstate );
	    printf ( "%s %d .. ", ip2str32(inp->inp_laddr.s_addr), ntohs(inp->inp_lport) );
	    printf ( "%s %d\n", ip2str32(inp->inp_faddr.s_addr), ntohs(inp->inp_fport) );
        }
}

/* ============================ */
/* ============================ */

static void
so_puts ( struct socket *so, char *buf )
{
	tcp_send ( so, buf, strlen(buf) );
}

/* remove any trailing combination of \r\n
 *  and add terminating null
 */
static void
net_trim ( char *msg, int len )
{
	while ( len ) {
	    if ( msg[len-1] != '\r' && msg[len-1] != '\n' )
		break;
	    --len;
	}

	msg[len] = '\0';
}

#define PRINTF_BUF_SIZE 128

void
so_printf ( struct socket *so, const char *fmt, ... )
{
        char buf[PRINTF_BUF_SIZE];
        va_list args;
        int rv;

        va_start ( args, fmt );
        rv = vsnprintf ( buf, PRINTF_BUF_SIZE, fmt, args );
        va_end ( args );

	// printf ( "soprintf --- %d %d\n", rv, strlen(buf) );
	// tcp_send ( so, buf, strlen(buf) );
	tcp_send ( so, buf, rv );
}

/* ============================ */
/* ============================ */

#define DONEY_PORT	115

/* This listens, then when it gets a connection, reads and
 * counts bytes.
 *
 * I use this on linux for testing:
 *
 *   cat xinu.bin | ncat levi 115
 *
 * I ran this at first with the XBUF_SIZE set to 2048.
 * it worked, but I saw a mixed size of reads, with values
 * as big as the entire 2048.
 *
 * So I got curious and increased XBUF_SIZE to 16385.
 * Now I see most reads returning 8000 bytes.
 * Two things explain this value.
 *
 * 1 - Wireshark shows me that each network packet
 *   holds a 1000 byte payload (something linux decided
 *   on that I am not going to worry about).
 * 2 - Our BSD socketbuffers have a size of 8192.
 *
 * So the BSD code is loading 8 of the packet payloads
 *  into the socketbuffer, then passing that to us.
 *
 * Look at socket_sb.c for this code:
 *
 *   static u_long  tcp_sendspace = 1024*8;
 *   static u_long  tcp_recvspace = 1024*8;
 */

// #define XBUF_SIZE	8*2048
#define XBUF_SIZE	8*1024

char xmsg[XBUF_SIZE];

static void
run_doney ( struct socket *so )
{
	int n;
	int total = 0;

	printf ( "start doney connection handler: so = %08x\n", so );

	for ( ;; ) {

	    n = tcp_recv ( so, xmsg, XBUF_SIZE );

	    /* Here we discover connection is closed.
	     * Note that when the other end closes
	     * the connection, it stays "connected",
	     * but is marked as no longer receiving.
	     */
	    if ( n == 0 && ! is_socket_receiving ( so ) ) {
		printf ( "Bail out, not receiving\n" );
		break;
	    }

	    total += n;
	    printf ( "Doney got: %d, %d\n", n, total );
	}

	tcp_close ( so );

	printf ( "total bytes seen = %d\n", total );
	printf ( "doney connection finished\n" );
}

void
doney_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( DONEY_PORT );
	if ( ! so ) {
	    printf ( "bind fails\n" );
	    return;
	}

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );

	    printf ( "doney server got a connection from: %s\n", ip2str32_h(fip) );

	    (void) safe_thr_new ( "doney_thr", (tfptr) run_doney, (void *) cso, PRI_TCP_TEST, 0 );
	}
}

void
bsd_test_doney ( long xxx )
{
	(void) safe_thr_new ( "doney", doney_thread, (void *) 0, PRI_TCP_TEST, 0 );
}


/* ============================ */

#define WANGDOODLE_PORT	114

int wang_debug = 0;

static int wang_state = 0;
static int wang_count = 0;
static int wang_dog_count = 0;
static struct thread *wang_thread;
static int wang_unblock_count = 0;
static int wang_blocked = 7;

static int wang_first = 1;

/* Currently "t 9" does this */
static void
wang_show ( void )
{
	printf ( "Wang state, count = %d, %d\n", wang_state, wang_count );
	printf ( "Wang dog count = %d\n", wang_dog_count );
	printf ( "Wang unblock count = %d\n", wang_unblock_count );
	printf ( "Wang blocked = %d\n", wang_blocked );

	/* Toggle */
	if ( wang_debug )
	    wang_debug = 0;
	else
	    wang_debug = 1;

#ifdef notdef
	/* open the floodgates */
	if ( wang_first ) {
	    wang_debug = 1;
	    wang_first = 0;
	} else {
	    wang_debug = 0;
	}
#endif
}

/* A different idea.  Now I want to send recognizable data in each
 * 2000 byte packet.
 */

#define CHUNK	2000
#define CREP	(2000/5/4)

/* Don't put this on the stack */
char wang_buf[CHUNK+1];

/* Generate a 2000 byte buffer with 4 pieces,
 * each 500 bytes and full of identical stuff
 */
static void
load_buf ( char *buf, char *proto, int num )
{
	char pbuf[10];
	int i;
	char *p;

	p = proto + 2;

	sprintf ( p, "%03d", num++ );
	for ( i=0; i<CREP; i++ ) {
	    strncpy ( buf, proto, 5 );
	    buf += 5;
	}
	sprintf ( p, "%03d", num++ );
	for ( i=0; i<CREP; i++ ) {
	    strncpy ( buf, proto, 5 );
	    buf += 5;
	}
	sprintf ( p, "%03d", num++ );
	for ( i=0; i<CREP; i++ ) {
	    strncpy ( buf, proto, 5 );
	    buf += 5;
	}
	sprintf ( p, "%03d", num );
	for ( i=0; i<CREP; i++ ) {
	    strncpy ( buf, proto, 5 );
	    buf += 5;
	}
}

static void
big_wangdoodle ( struct socket *so, int nchunks )
{
	// int total;
	int i;
	int ix;
	int nc;

	// #define NUM_CHUNK	100
	// total = NUM_CHUNK * CHUNK;
	// printf ( "Big wandoodle: %d bytes in chunks of %d\n", total, CHUNK );

	nc = nchunks - 2;
	if ( nc < 0 ) nc = 0;

	wang_state = 11;
	wang_count = 0;
	load_buf ( wang_buf, "ST000", 1 );
	wang_unblock_count = 0;
	tcp_send ( so, wang_buf, CHUNK );

	ix = 1;
	for ( i=0; i<nc; i++ ) {
	    wang_state = 12;
	    wang_count++;
	    load_buf ( wang_buf, "PK000", ix );
	    wang_unblock_count = 0;
	    tcp_send ( so, wang_buf, CHUNK );
	    ix += 4;
	}

	wang_state = 13;
	load_buf ( wang_buf, "EN000", 1 );
	wang_unblock_count = 0;
	tcp_send ( so, wang_buf, CHUNK );
}

/* Wrapper on the above, adds some rules, namely --
 * No less that 2 chunks and a multiple of chunk size
 */
static void
lots ( struct socket *so, int chunks )
{
	// chunks = (total + CHUNK - 1) / CHUNK;
	if ( chunks < 2 ) chunks = 2;
	big_wangdoodle ( so, chunks );
}

#ifdef notdef

// #define CHUNK	5000
// #define CHUNK	8000
// #define CHUNK	2048
#define CHUNK	2000

-static void
-big_wangdoodle ( struct socket *so )
-{
-	char *buf;
-	int total;
-
-	/* As good a place as any */
-	buf = (char *) tcp_input;
-
-	/* This many bytes */
-	// total = 137555;
-	total = 132555;
-	printf ( "Big wandoodle: %d bytes in chunks of %d\n", total, CHUNK );
-
-	while ( total > 0 ) {
-	    if ( total < CHUNK ) {
-		tcp_send ( so, buf, total );
-		break;
-	    } else {
-		tcp_send ( so, buf, CHUNK );
-		total -= CHUNK;
-		buf += CHUNK;
-	    }
-	}
-}
#endif

/* Called from thr_unblock() */
void
wang_hook1 ( struct thread *old, struct thread *new )
{
	// if ( cur_thread != old )
	//     return;
	// wang_unblock_count++;
	if ( wang_debug )
	    printf ( "Switch %s --> %s\n", old->name, new->name );
}

void
wang_hook2 ( struct thread *cur )
{
	if ( wang_debug ) {
	    printf ( " running %s %d\n", cur->name, cur->mode );
	}
}

void
wang_hook3 ( int is_blocked )
{
	wang_blocked = is_blocked;
}

#define MAXW	4

/* Called for each line sent by user */
static int
one_wangdoodle ( struct socket *so, char *cmd )
{
	char *wp[MAXW];
	int nw;
	int ntest;
	// char resp[64];

	wang_thread = cur_thread;

	if ( cmd[0] == '\0' )
	    return 0;

	if ( cmd[0] == 'q' )
	    return 1;

	nw = split ( cmd, wp, MAXW );

	// printf ( "Got: %s\n", cmd );

	// printf ( "len = %d\n", strlen(cmd) );
	// dump_buf ( cmd, strlen(cmd) );

// #define NUM_BIG	100	/* 100 * 2K = 200K */
#define NUM_BIG	500	/* 500 * 2K = 1M */

	if ( strcmp ( cmd, "stat" ) == 0 )
	    so_puts ( so, "OK\n" );
	else if ( strcmp ( cmd, "rand" ) == 0 )
	    so_printf ( so, "R %d\n", gb_next_rand() );
	else if ( strcmp ( cmd, "uni" ) == 0 )
	    so_printf ( so, "U %d\n", gb_unif_rand(100) );
	else if ( strcmp ( cmd, "big" ) == 0 ) {
	    wang_state = 10;
	    // big_wangdoodle ( so );
	    lots ( so, NUM_BIG );
	    wang_state = 99;
	    return 1 ;
	} else if ( strcmp ( wp[0], "lots" ) == 0 && nw == 1 ) {
	    lots ( so, 2 );
	} else if ( strcmp ( wp[0], "lots" ) == 0 && nw == 2 ) {
	    ntest = atoi ( wp[1] );
	    lots ( so, (ntest+CHUNK-1)/CHUNK );
	}
	else
	    so_puts ( so, "ERR\n" );

	return 0;
}

static int wang_ccount = 0;

/* Called to handle each connection */
static void
run_wangdoodle ( struct socket *so )
{
	char msg[128];
	int n;

	// printf ( "start wangdoodle connection handler: so = %08x\n", so );
	wang_state = 1;

	++wang_ccount;
	printf ( "Wangdoodle connection %d ..", wang_ccount );

#ifdef ETIMER_TCP
	etimer_arm ( 20 );
#endif

	for ( ;; ) {

	    n = tcp_recv ( so, msg, 128 );

	    /* Here we discover connection is closed.
	     * Note that when the other end closes
	     * the connection, it stays "connected",
	     * but is marked as no longer receiving.
	     */
	    // if ( ! is_socket_connected ( so ) )
	    if ( n == 0 && ! is_socket_receiving ( so ) ) {
		printf ( "Bail out, not receiving\n" );
		break;
	    }

	    /* When using telnet, the string is not sent
	     * until I hit return.  Then if I type
	     * "dog", 5 bytes get sent, "\r\n" gets
	     * appended (and no null byte).
	     */
	    // printf ( "%d bytes from tcp_recv\n", n );
	    // dump_buf ( msg, n );

	    net_trim ( msg, n );

	    if ( one_wangdoodle ( so, msg ) ) {
		// printf ( "quit by user request\n" );
		break;
	    }
	}

	wang_state = 81;
	tcp_close ( so );
	wang_state = 89;
	printf ( " finished (%d)\n", wang_ccount );

#ifdef ETIMER_TCP
	etimer_show_times ();
#endif

	// printf ( "wangdoodle connection finished\n" );
}

#ifdef notdef
static void
run_wangdoodle ( struct socket *so )
{
	char msg[128];
	int n;
	int s;
	int idle = 0;

	printf ( "start wangdoodle connection handler: so = %08x\n", so );

	for ( ;; ) {
	    /* All of this idle business is a hack until I figure
	     * out a nice way to learn that a connection has
	     * been closed from the other end.
	     */
	    if ( idle == 0 ) {
		s = get_socket_state ( so );
		// printf ( "Socket state: %08x\n", s );
	    }
	    if ( idle > 2000 ) {
		s = get_socket_state ( so );
		// printf ( "Socket state: %08x\n", s );
		printf ( "Timeout -- exit thread\n" );
		break;
	    }

	    n = tcp_recv ( so, msg, 128 );

	    /* Hack 12-5-2022 */
	    /* Here we discover connection is closed.
	     * Note that when the other end closes
	     * the connection, it stays "connected",
	     * but is marked as no longer receiving.
	     */
	    // if ( ! is_socket_connected ( so ) )
	    if ( n == 0 && ! is_socket_receiving ( so ) ) {
		printf ( "Bail out, not receiving\n" );
		tcp_close ( so );
		break;
	    }

	    /* more idle hackery */
	    if ( n == 0 ) {
		thr_delay ( 1 );
		idle++;
		continue;
	    }

	    idle = 0;

	    /* When using telnet, the string is not sent
	     * until I hit return.  Then if I type
	     * "dog", 5 bytes get sent, "\r\n" gets
	     * appended (and no null byte).
	     */
	    // printf ( "%d bytes from tcp_recv\n", n );
	    // dump_buf ( msg, n );

	    net_trim ( msg, n );

	    if ( one_wangdoodle ( so, msg ) ) {
		/* Here we close the connection */
		printf ( "quit by user request\n" );
		tcp_close ( so );
		break;
	    }
	}

	printf ( "wangdoodle done\n" );

	// strcpy ( msg, "All Done\n" );
	// tcp_send ( so, msg, strlen(msg) );
}
#endif

/* A different sort of watchdog 12-29-2022.
 * This runs at a priority just above the connection thread
 * to see if our thread is simply getting starved.
 */
void
wang_dog_thread ( long xxx )
{
	wang_dog_count = 0;

	for ( ;; ) {
	    wang_dog_count++;
	    thr_delay ( 500 );
	}
}

void
wangdoodle_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	printf ( "Wangdoodle server starting\n" );

	so = tcp_bind ( WANGDOODLE_PORT );
	if ( ! so ) {
	    printf ( "bind fails\n" );
	    return;
	}

	printf ( "Wangdoodle server running on port %d, priority %d\n", WANGDOODLE_PORT, WANG_PRI );

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );

	    // printf ( "wangdoodle server got a connection from: %s\n", ip2str32_h(fip) );
	    // printf ( "wangdoodle socket: %08x\n", cso );

	    (void) safe_thr_new ( "wang_thr", (tfptr) run_wangdoodle, (void *) cso, WANG_PRI2, 0 );
	}
}

#ifdef notdef
/* I used these when I had a mysterious hang and wanted to know if it was just the TCP
 * subsystem, or Kyu overall (it was TCP).  After this I made changes to Kyu so that I
 * can run the Kyu shell at priority 11 and thus have it to investigate when the next hang
 * takes place.
 */
static int dog_count = 0;

static void
dog_func ( long xxx )
{
	++dog_count;
	printf ( "Alive and kicking (thread) %d\n", dog_count );
}

static int fast_dog_count = 0;
static int dog_count2 = 0;

static void
dog2_func ( void )
{
	++fast_dog_count;
	if ( (fast_dog_count % 60000) == 0 ) {
	    dog_count2++;
	    printf ( "Alive and kicking (hook) %d\n", dog_count2 );
	}
}

/* This starts a thread to give periodic announcements.
 * I originally asked for priority 5, but Kyu bumps that to 10
 * I now select 12, which is just below the shell.
 */
static void
want_monitor1 ( void )
{
	(void) thr_new_repeat ( "dog", dog_func, (void *) 0, 12, 0, 60000 );
}

static void
want_monitor2 ( void )
{
	timer_hookup ( dog2_func );
}
#endif

void
bsd_test_wangdoodle ( long xxx )
{
	// want_monitor1 ();
	// want_monitor2 ();

	// (void) safe_thr_new ( "wangdog", wang_dog_thread, (void *) 0, WANG_DOG_PRI, 0 );

	(void) safe_thr_new ( "wangdoodle", wangdoodle_thread, (void *) 0, WANG_PRI, 0 );
}

/* ============================ */

#define ECHO_PORT	113

static void
run_echo ( struct socket *so )
{
	char msg[128];
	int n;

	for ( ;; ) {
	    n = tcp_recv ( so, msg, 128 );
	    /* This happens when telnet closes
	     * the connection.
	     */
	    if ( n == 0 )
		break;
	    // if ( n == 0 ) {
	    // 	thr_delay ( 1 );
	    // 	continue;
	    //  }
	    tcp_send ( so, msg, n );
	    if ( msg[0] == 'q' )
		break;
	}

	printf ( "done, closing\n" );
	tcp_close ( so );

	strcpy ( msg, "All Done\n" );
	tcp_send ( so, msg, strlen(msg) );
}

void
echo_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( ECHO_PORT );
	if ( ! so ) {
	    printf ( "bind fails\n" );
	    return;
	}

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );
	    printf ( "echo server got a connection from: %s\n", ip2str32_h(fip) );

	    /* Really shoud run this in new thread */
	    run_echo ( cso );
	}
}

void
bsd_test_echo ( long xxx )
{
	(void) safe_thr_new ( "echo-server", echo_thread, (void *) 0, PRI_TCP_TEST, 0 );
}

/* ============================ */

#define BLAB_PORT	112

static void
blabber ( struct socket *so )
{
	char msg[128];
	int i;

	for ( i=0; i < 25; i++ ) {
	    sprintf ( msg, "Blab: %d\n", i );
	    tcp_send ( so, msg, strlen(msg) );
	    thr_delay ( 1000 );
	}

	strcpy ( msg, "All Done\n" );
	tcp_send ( so, msg, strlen(msg) );
}

void
blab_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( BLAB_PORT );
	if ( ! so ) {
	    printf ( "bind fails\n" );
	    return;
	}

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );
	    printf ( "blab server got a connection from: %s\n", ip2str32_h(fip) );

	    /* Really shoud run this in new thread */
	    blabber ( cso );

	    tcp_close ( cso );
	}
}

void
bsd_test_blab ( long xxx )
{
	(void) safe_thr_new ( "blab-server", blab_thread, (void *) 0, PRI_TCP_TEST, 0 );
}

/* ============================ */

// static void bind_test ( int );

#define PIRATE_PORT	111

void
pirate_thread ( long arg )
{
	struct socket *so;
	struct socket *cso;
	int fip;
	int port = arg;

	so = tcp_bind ( port );
	if ( ! so ) {
	    printf ( "bind fails\n" );
	    return;
	}

	/* ------------- finally do an accept */
	/* Accept is the most interesting perhaps.
	 * the unix call blocks and returns a new
	 * socket when a connection is made.
	 * The arguments to accept give a place where
	 * the address on the peer end of the connection
	 * can be returned.  This is ignored if both
	 * address and length are set to zero.
	 */
	bpf2 ( "--- Server bind OK\n" );
	bpf2 ( "--- waiting for connections\n" );
	// tcb_show ();

	for ( ;; ) {
	    char *msg = "Dead men tell no tales\r\n";
	    cso = tcp_accept ( so );
	    if ( ! cso ) {
		printf ( "Accept fails\n" );
		break;
	    }

	    fip = tcp_getpeer_ip ( cso );
	    // printf ( "kyu_accept got a connection: %08x\n", cso );
	    bpf3 ( "kyu_accept got a connection from: %s\n", ip2str32_h(fip) );
	    // tcb_show ();

	    tcp_send ( cso, msg, strlen(msg) );

	    tcp_close ( cso );

	    bpf3 ( "socket was closed\n" );
	}

	tcp_close ( so );
}

void
bsd_test_server ( long xxx )
{
	char buf[64];

	sprintf ( buf, "tcp-%d", PIRATE_PORT );
	(void) safe_thr_new ( buf, pirate_thread, (void *) PIRATE_PORT, PRI_TCP_TEST, 0 );
}

/* ============================ */

struct socket *tcp_bind ( int );
struct socket *tcp_accept ( struct socket * );
struct socket *tcp_connect ( char *, int );

// static void connect_test ( char *, int );

#define TEST_BUF_SIZE	128

struct socket *client_socket;

static void
connect_test ( char *host, int port )
{
	struct socket *so;
	char buf[TEST_BUF_SIZE];
	int i, n;

	// bpf1 ( "Start connect to %s (port %d)\n", host, port );
	so = tcp_connect ( host, port );
	// bpf1 ( "Connect returns: %08x\n", so );
	// printf ( "TCP connect done\n" );

	client_socket = so;

#ifdef notdef
	for ( i=0; i<2; i++ ) {
	    thr_delay ( 1000 );		// 1 second
	    n = tcp_recv ( so, buf, TEST_BUF_SIZE );
	    printf ( "%d bytes received\n", n );
	    if ( n > 0 ) {
		buf[n-2] = '\n';
		buf[n-1] = '\0';
		printf ( "%s", buf );
		// dump_buf ( buf, n );
	    }
	}
#endif

	// XXX this works, but why?
	// thr_delay ( 100 );

	n = tcp_recv ( so, buf, TEST_BUF_SIZE );
	// printf ( "%d bytes received\n", n );
#ifdef notdef
	/* This was helpful before I fixed a bug */
	if ( n == 0 ) {
	    printf ( "try again\n" );
	    n = tcp_recv ( so, buf, TEST_BUF_SIZE );
	    printf ( "%d bytes received\n", n );
	}
#endif

	if ( n > 0 ) {
	    buf[n-2] = '\n';
	    buf[n-1] = '\0';
	    printf ( "\n" );
	    printf ( "%s", buf );
	}

	// thr_delay ( 1000 );		// 1 second

	tcp_close ( so );

	// printf ( "Connect closed and finished\n" );
	// printf ( "Leaving connect socket open\n" );

	// bpf1 ( "TCP connect test finished\n" );
}


void
client_thread ( long xxx )
{
	// I just picked 111 at random for experimentation,
	// but it turns out to be "ONC RPC"
	// which is part of Sun NFS and is running on
	// my linux machine (for some ancient reason).

	// int port = 111;
	// int port = 2345;
	int port = 13;		// Daytime

	// char *host = "127.0.0.1";
	char *host = "192.168.0.5";

	// bpf1 ( "***\n" );
	// bpf1 ( "Start connect test *******************\n" );
	// bpf1 ( "***\n" );

	connect_test ( host, port );

	// bpf1 ( "***\n" );
	// bpf1 ( "Finished with connect test *******************\n" );
	// bpf1 ( "***\n" );
}

void
bsd_test_connect ( long xxx )
{
	(void) safe_thr_new ( "tcp-client", client_thread, (void *) 0, PRI_TCP_TEST, 0 );
}

/* ++++++++++++++++++++++++++++++++ */

/* Something on the linux side ought to be listening.
 *
 * I use this:
 *
 *   ncat -v -v -l -p 2022 > levi.test < /dev/null
 *
 * I have to shut down my firewall to open port 2022.
 *
 * Then I get this when I try to write 4444 bytes
 *  Ncat: Version 7.93 ( https://nmap.org/ncat )
 *  Ncat: Listening on :::2022
 *  Ncat: Listening on 0.0.0.0:2022
 *  Ncat: Connection from 192.168.0.73.
 *  Ncat: Connection from 192.168.0.73:1024.
 *  NCAT DEBUG: EOF on stdin
 * The file levi.test has 4444 bytes.
 * This test worked.
 *
 * Sending 132555 bytes "just worked" and in a split second.
 * Wireshark showed all the TCP packets had
 *  500 byte payloads.  Who knows why.
 * It sure goes fast when there isn't any printf uart
 *  traffic to slow everything down.
 */

#define LOTS_SIZE	132555

// This works just fine
// #define LOTS_CHUNK	2048
// These do also
// #define LOTS_CHUNK	3000
// #define LOTS_CHUNK	4000
// #define LOTS_CHUNK	5000

/* This yields "panic m_copym 3" */
// #define LOTS_CHUNK	10000

/* This yields "panic m_copym 3" */
#define LOTS_CHUNK	8000

/* This yields "panic m_copym 3" */
// #define LOTS_CHUNK	6500

void
lots_thread ( long xxx )
{
	struct socket *so;
	char *buf;
	int i, n;

	int port = 2022;
	char *host = "192.168.0.5";
	int total;

	/* As good a place as any */
	buf = (char *) tcp_input;

	/* This many bytes */
	total = LOTS_SIZE;

	printf ( "Connect to %s (port %d)\n", host, port );
	so = tcp_connect ( host, port );
	printf ( "Connected\n" );

	/* Currently this is useless.  A valid socket is arranged and
	 * if not port is listening on the linux side, we currently
	 * get an ICMP reply, "Destination unreachable,
	 *  administratively prohibited".  Sounds like my firewall.
	 * BSD seems to eventually time out.
	 */
	if ( ! so ) {
	    printf ( "Connect failed\n" );
	    return;
	}

	while ( total > 0 ) {
	    if ( total < LOTS_CHUNK ) {
		tcp_send ( so, buf, total );
		break;
	    } else {
		tcp_send ( so, buf, LOTS_CHUNK );
		total -= LOTS_CHUNK;
		buf += LOTS_CHUNK;
	    }
	}

	tcp_close ( so );

	printf ( "TCP connect test with lots finished\n" );
}

#ifdef notdef
void
lots_thread_1 ( long xxx )
{
	struct socket *so;
	char *buf;
	int i, n;

	int port = 2022;
	char *host = "192.168.0.5";
	int total;

	/* As good a place as any */
	buf = (char *) tcp_input;

	/* This many bytes */
	total = 4444;

	printf ( "Connect to %s (port %d)\n", host, port );
	so = tcp_connect ( host, port );

	/* Currently this is useless.  A valid socket is arranged and
	 * if not port is listening on the linux side, we currently
	 * get an ICMP reply, "Destination unreachable,
	 *  administratively prohibited".  Sounds like my firewall.
	 * BSD seems to eventually time out.
	 */
	if ( ! so ) {
	    printf ( "Connect failed\n" );
	    return;
	}

	tcp_send ( so, buf, total );

	tcp_close ( so );

	printf ( "TCP connect test with lots finished\n" );
}
#endif

void
bsd_test_lots ( long xxx )
{
	(void) safe_thr_new ( "tcp-lots", lots_thread, (void *) 0, PRI_TCP_TEST, 0 );
}

#ifdef notdef
static int pig_data[1024];

static void
pig1 ( void )
{
	int i;

	for ( i=0; i<1024; i++ )
	    pig_data[i] = 99;
}

static void
pig2 ( void )
{
	int i;

	for ( i=0; i<1024; i++ )
	    pig_data[i] = 22;
}

void
bsd_test_pig ( long xxx )
{
	for ( ;; ) {
	    pig1 ();
	    pig2 ();
	}
}
#endif

/* display info about a possibly hung wangdoodle connection */
void
bsd_test_info ( long xxx )
{
	wang_show ();
}

/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */

/* THE END */
