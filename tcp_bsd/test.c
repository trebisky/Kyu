/* test.c */

/* test code for BSD 4.4 TCP code in Kyu
 * Tom Trebisky  11-12-2022
 */

#include <bsd.h>

// gets us htonl and such
#include <kyu.h>
#include <thread.h>
#include <tests.h>

void tcb_show ( void );

void bsd_test_show ( long );
void bsd_test_server ( long );
void bsd_test_connect ( long );
void bsd_test_debug ( long );

/* XXX - stolen from tcp_xinu, needs rework */
/* Exported to main test code */
/* Arguments are now ignored */
struct test tcp_test_list[] = {
        bsd_test_show,          "Show TCB table",                       0,
        bsd_test_server,        "Start pirate server",                  0,
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

void
connect_test ( char *host, int port )
{
	struct socket *so;

	bpf1 ( "Start connect to %s (%d)\n", host, port );
	so = tcp_connect ( host, port );
	bpf1 ( "Connect returns: %08x\n", so );

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
