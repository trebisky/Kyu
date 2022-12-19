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
void bsd_test_close ( long );
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
	// bsd_test_close,		"Close client socket",			0,
	bsd_test_debug,		"Set debug 0",				0,
	bsd_test_debug,		"Set debug 1",				1,
	bsd_test_debug,		"Set debug 2",				2,
	bsd_test_debug,		"Set debug 3",				3,
	bsd_test_debug,		"Set debug 4",				4,
#ifdef notdef
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
	tcb_show ();
	locker_show ();
	tcp_statistics ();
}

/* For debugging */
void
tcb_show ( void )
{
        struct inpcb *inp;

        inp = &tcb;
        if ( inp->inp_next == &tcb )
	    printf ( "INPCB: empty\n" );

        while ( inp->inp_next != &tcb ) {
            inp = inp->inp_next;
            printf ( "INPCB: %08x -- local, foreign: ", inp );
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

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );

	    printf ( "doney server got a connection from: %s\n", ip2str32_h(fip) );

	    (void) safe_thr_new ( "doney_thr", (tfptr) run_doney, (void *) cso, 15, 0 );
	}
}

void
bsd_test_doney ( long xxx )
{
	(void) safe_thr_new ( "doney", doney_thread, (void *) 0, 15, 0 );
}


/* ============================ */

#define WANGDOODLE_PORT	114

//#define CHUNK	2048
// #define CHUNK	5000
#define CHUNK	8000

static void
big_wangdoodle ( struct socket *so )
{
	char *buf;
	int total;

	/* As good a place as any */
	buf = (char *) tcp_input;

	/* This many bytes */
	total = 137555;
	printf ( "Big wandoodle: %d bytes in chunks of %d\n", total, CHUNK );

	while ( total > 0 ) {
	    if ( total < CHUNK ) {
		tcp_send ( so, buf, total );
		break;
	    } else {
		tcp_send ( so, buf, CHUNK );
		total -= CHUNK;
		buf += CHUNK;
	    }
	}
}

static int
one_wangdoodle ( struct socket *so, char *cmd )
{
	char resp[64];

	if ( cmd[0] == 'q' )
	    return 1;

	// printf ( "Got: %s\n", cmd );

	// printf ( "len = %d\n", strlen(cmd) );
	// dump_buf ( cmd, strlen(cmd) );

	if ( strcmp ( cmd, "stat" ) == 0 )
	    so_puts ( so, "OK\n" );
	else if ( strcmp ( cmd, "rand" ) == 0 )
	    so_printf ( so, "R %d\n", gb_next_rand() );
	else if ( strcmp ( cmd, "uni" ) == 0 )
	    so_printf ( so, "U %d\n", gb_unif_rand(100) );
	else if ( strcmp ( cmd, "big" ) == 0 ) {
	    big_wangdoodle ( so );
	    return 1 ;
	}
	else
	    so_puts ( so, "ERR\n" );

	return 0;
}

static void
run_wangdoodle ( struct socket *so )
{
	char msg[128];
	int n;

	printf ( "start wangdoodle connection handler: so = %08x\n", so );

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
		printf ( "quit by user request\n" );
		break;
	    }
	}

	tcp_close ( so );
	printf ( "wangdoodle connection finished\n" );
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

void
wangdoodle_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( WANGDOODLE_PORT );
	printf ( "Wangdoodle server running on port %d\n", WANGDOODLE_PORT );

	for ( ;; ) {
	    cso = tcp_accept ( so );
	    fip = tcp_getpeer_ip ( cso );

	    printf ( "wangdoodle server got a connection from: %s\n", ip2str32_h(fip) );
	    // printf ( "wangdoodle socket: %08x\n", cso );

	    (void) safe_thr_new ( "wang_thr", (tfptr) run_wangdoodle, (void *) cso, 15, 0 );
	}
}

void
bsd_test_wangdoodle ( long xxx )
{
	(void) safe_thr_new ( "wangdoodle", wangdoodle_thread, (void *) 0, 15, 0 );
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
	(void) safe_thr_new ( "echo-server", echo_thread, (void *) 0, 15, 0 );
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
	(void) safe_thr_new ( "blab-server", blab_thread, (void *) 0, 15, 0 );
}

/* ============================ */

// static void bind_test ( int );

static void
bind_test ( int port )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( port );

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
server_thread ( long xxx )
{
	bind_test ( 111 );
}

void
bsd_test_server ( long xxx )
{
	(void) safe_thr_new ( "tcp-server", server_thread, (void *) 0, 15, 0 );
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
	(void) safe_thr_new ( "tcp-client", client_thread, (void *) 0, 15, 0 );
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
	(void) safe_thr_new ( "tcp-lots", lots_thread, (void *) 0, 15, 0 );
}

#ifdef notdef
void
bsd_test_close ( long xxx )
{
	tcp_close ( client_socket );
}
#endif

/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */

/* THE END */
