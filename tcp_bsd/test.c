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
void bsd_test_connect ( long );
void bsd_test_debug ( long );

/* XXX - stolen from tcp_xinu, needs rework */
/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
        bsd_test_show,          "Show TCB table",                       0,
        bsd_test_server,        "Start pirate server",                  0,
        bsd_test_blab,          "Start blab server",                    0,
        bsd_test_echo,          "Start echo server",                    0,
        bsd_test_wangdoodle,    "Start wangdoodle server",              0,
	bsd_test_connect,	"Connect test",				0,
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
}

/* For debugging */
void
tcb_show ( void )
{
        struct inpcb *inp;

        inp = &tcb;
        while ( inp->inp_next != &tcb ) {
            inp = inp->inp_next;
            printf ( "INPCB: %08x -- local, foreign: %s %d .. %s %d\n", inp,
                ip2str32(inp->inp_laddr.s_addr),
                ntohs(inp->inp_lport),
                ip2str32(inp->inp_faddr.s_addr),
                ntohs(inp->inp_fport) );
        }
}

/* ============================ */

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

#define WANGDOODLE_PORT	114

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

static int
one_wangdoodle ( struct socket *so, char *cmd )
{
	char resp[64];

	if ( cmd[0] == 'q' )
	    return 1;

	// printf ( "len = %d\n", strlen(cmd) );
	// dump_buf ( cmd, strlen(cmd) );

	if ( strcmp ( cmd, "stat" ) == 0 )
	    so_puts ( so, "OK\n" );
	else if ( strcmp ( cmd, "rand" ) == 0 )
	    so_printf ( so, "R %d\n", gb_next_rand() );
	else if ( strcmp ( cmd, "uni" ) == 0 )
	    so_printf ( so, "U %d\n", gb_unif_rand(100) );
	else
	    so_puts ( so, "ERR\n" );
	return 0;
}

static void
run_wangdoodle ( struct socket *so )
{
	char msg[128];
	int n;
	int s;
	int idle = 0;

	// printf ( "start wangdoodle: %08x\n", so );

	for ( ;; ) {
	    if ( idle == 0 ) {
		s = get_socket_state ( so );
		printf ( "Socket state: %08x\n", s );
	    }
	    if ( idle > 2000 ) {
		s = get_socket_state ( so );
		printf ( "Socket state: %08x\n", s );
		printf ( "Timeout -- exit thread\n" );
		break;
	    }
	    /* Hack 12-5-2022 */
	    /* Here we discover connection is closed.
	     * the socket is no longer valid, so
	     * calling so_close() is bad.
	     */
	    // if ( ! is_socket_connected ( so ) )
	    if ( ! is_socket_receiving ( so ) ) {
		printf ( "Bail out, not receiving\n" );
		(void) soclose ( so );
		break;
	    }

	    n = tcp_recv ( so, msg, 128 );
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
		(void) soclose ( so );
		break;
	    }

	    // tcp_send ( so, msg, n );
	    // if ( msg[0] == 'q' )
	    // 	break;
	}

	printf ( "wangdoodle done\n" );

	// strcpy ( msg, "All Done\n" );
	// tcp_send ( so, msg, strlen(msg) );
}

void
wangdoodle_thread ( long xxx )
{
	struct socket *so;
	struct socket *cso;
	int fip;

	so = tcp_bind ( WANGDOODLE_PORT );

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
	    if ( n == 0 ) {
		thr_delay ( 1 );
		continue;
	    }
	    tcp_send ( so, msg, n );
	    if ( msg[0] == 'q' )
		break;
	}

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

	    (void) soclose ( cso );
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

	    (void) soclose ( cso );
	}
}

void
bsd_test_blab ( long xxx )
{
	(void) safe_thr_new ( "blab-server", blab_thread, (void *) 0, 15, 0 );
}

/* ============================ */

static void bind_test ( int );

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

static void connect_test ( char *, int );

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

	char *host = "192.168.0.5";
	// char *host = "127.0.0.1";

	bpf1 ( "***\n" );
	bpf1 ( "Start connect test *******************\n" );
	bpf1 ( "***\n" );

	connect_test ( host, port );

	bpf1 ( "***\n" );
	bpf1 ( "Finished with connect test *******************\n" );
	bpf1 ( "***\n" );
}

void
bsd_test_connect ( long xxx )
{
	(void) safe_thr_new ( "tcp-client", client_thread, (void *) 0, 15, 0 );
}

/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */
/* ================================================================================ */

struct socket *tcp_bind ( int );
struct socket *tcp_accept ( struct socket * );
struct socket *tcp_connect ( char *, int );

#ifdef notdef
struct mbuf *mb_free ( struct mbuf * );

/* Can these all be static ?? */
void sofree ( struct socket * );
void sbflush ( struct sockbuf * );
void sbdrop ( struct sockbuf *, int );

void socantrcvmore ( struct socket * );
void socantsendmore ( struct socket * );
#endif

/* ----------------------------- */
/* ----------------------------- */

#define TEST_BUF_SIZE	128

void
connect_test ( char *host, int port )
{
	struct socket *so;
	char buf[TEST_BUF_SIZE];
	int i, n;

	bpf1 ( "Start connect to %s (port %d)\n", host, port );
	so = tcp_connect ( host, port );
	// bpf1 ( "Connect returns: %08x\n", so );

	for ( i=0; i<5; i++ ) {
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

	(void) soclose ( so );
	bpf1 ( "Connect closed and finished\n" );

	bpf1 ( "TCP connect test finished\n" );
}

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

	    fip = tcp_getpeer_ip ( cso );
	    // printf ( "kyu_accept got a connection: %08x\n", cso );
	    bpf3 ( "kyu_accept got a connection from: %s\n", ip2str32_h(fip) );
	    // tcb_show ();

	    tcp_send ( cso, msg, strlen(msg) );
	    (void) soclose ( cso );
	    bpf3 ( "socket was closed\n" );
	}
}

/* THE END */
