/* net_main.c
 * T. Trebisky  3-24-2005
 * T. Trebisky  6-1-2015
 */

/* TODO _
 * send an icmp "ping"
 * check arp response for same IP on Grat arp.
 * send an arp request.
 * send a UDP packet.
 * Use netbuf fully and properly.
 */

/*
#include "intel.h"
*/
#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

#define NEW_SLOW_WAY

#ifdef notdef
unsigned long agate_ip = 0x0100000a;	/* agate: 10.0.0.1 */
unsigned long trona_ip = 0x3600000a;	/* trona: 10.0.0.54 */
unsigned long mmt_ip = 0x3264c480;	/* mmt: 128.196.100.50 */
unsigned long caliente_ip = 0x3364c480;	/* caliente: 128.196.100.51 */
unsigned long cholla_ip = 0x3464c480;	/* cholla: 128.196.100.52 */
#endif

/* Some machine that will respond to ping and arp */
/* Best if this is a linux machine that can run Wireshark */
unsigned long test_ip = 0xC0A80005;	/* trona: 192.168.0.5 */

static unsigned char broad[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static unsigned char our_mac[ETH_ADDR_SIZE];

static void slow_net ( int );
// static void fast_net ( void );

static void net_thread ( int );
static void netbuf_init ( void );
void netbuf_show ( void );
void net_show ( void );
static void net_handle ( struct netbuf * );
void net_addr_get ( char * );

struct netbuf * netbuf_alloc ( void );
struct netbuf * netbuf_alloc_i ( void );

static int num_ee = 0;
static int num_rtl = 0;
static int num_el3 = 0;
static int num_cpsw = 0;

static int num_eth = 0;

static int use_rtl = 0;

#define NET_IDLE	0
#define NET_INIT	1
#define NET_RUN		2

static int net_state = NET_IDLE;

/* XXX - This really should be part of an interface related
 * datastructure.
 */
unsigned long my_ip;
unsigned long gate_ip;
unsigned long my_net_mask;
unsigned long my_net;

static struct netbuf *inq_head;
static struct netbuf *inq_tail;

static struct sem *inq_sem;
/*
static struct sem *outq_sem;
*/

static int system_clock_rate;

#ifndef NEW_SLOW_WAY
static struct sem *slow_net_sem;
#endif

static int net_debug_f = 0;

void
net_debug_arm ( void )
{
	net_debug_f = 1;
}

/* This now gets run as a thread (10-4-2015)
 * on the ARM with the cpsw, sometimes the boot hangs with
 * the message "Waiting for PHY auto negotiation to complete".
 * I decided it would be much nicer to have this hung in its own
 * thread and have he Kyu shell to help debug the situation.
 */
void
net_hw_init ( int bogus )
{

#ifdef ARCH_X86
    num_ee = ee_init ();
    num_eth += num_ee;
    num_rtl = rtl_init ();
    num_eth += num_rtl;
    num_el3 = el3_init ();
    num_eth += num_el3;
#endif

    num_cpsw = cpsw_init ();
    num_eth += num_cpsw;

    cpsw_activate ();

#ifdef ARCH_X86
    /* XXX - need cleaner way to set these
     */
    if ( num_rtl > 0 ) {
	if ( ! net_dots ( "128.196.100.15", &my_ip ) )		/* ringo */
	    panic ( "netdots" );
	if ( ! net_dots ( "128.196.100.1", &gate_ip ) )
	    panic ( "netdots" );
	my_net_mask = ntohl ( 0xffffff00 );
	(void ) net_dots ( "128.196.100.0", &my_net );
	/*
	ee_activate ();
	*/
	rtl_activate ();
	use_rtl = 1;
    } else {
	if ( ! net_dots ( "10.0.0.202", &my_ip ) )		/* chorizo */
	    panic ( "netdots" );
	if ( ! net_dots ( "10.0.0.1", &gate_ip ) )
	    panic ( "netdots" );
	my_net_mask = ntohl ( 0xffffff00 );
	(void ) net_dots ( "10.0.0.0", &my_net );
	ee_activate ();
    }
#endif

    net_state = NET_RUN;
}

#define NET_STARTUP_WAIT	10

/* Called during startup if networking
 * is configured.
 */
void
net_init ( void )
{
    int count;

    /*
    unsigned long xxx;
    if ( ! net_dots ( "128.196.100.54", &xxx ) )
	printf ( "Cannot parse dotted address\n" );
    printf ( "Addr = %08x, %s\n", xxx, ip2strl ( xxx ) );
    */

    (void) net_dots ( "192.168.0.11", &my_ip );
    (void) net_dots ( "192.168.0.1", &gate_ip );
    my_net_mask = ntohl ( 0xffffff00 );
    (void ) net_dots ( "192.168.0.0", &my_net );

    system_clock_rate = timer_rate_get();

    netbuf_init ();
    tcp_init ();

    arp_init ();
    dns_init ();
    bootp_init ();
    tftp_init ();

    inq_head = (struct netbuf *) 0;
    inq_tail = (struct netbuf *) 0;

    /*
    inq_sem = cpu_new ();
    */
    inq_sem = sem_signal_new ( SEM_FIFO );
    if ( ! inq_sem )
	panic ("Cannot get net input semaphore");

#ifdef notyet
    /*
    outq_sem = cpu_new ();
    */
    outq_sem = sem_signal_new ( SEM_FIFO );
    if ( ! outq_sem )
	panic ("Cannot get net output semaphore");
#endif

    /* XXX review and revise priorities someday */
    (void) safe_thr_new ( "net", net_thread, (void *) 0, 10, 0 );

#ifdef NEW_SLOW_WAY
    /* We do indeed do the new slow way */
    (void) thr_new_repeat ( "net_slow", slow_net, (void *) 0, 11, 0, system_clock_rate );
#else
    (void) safe_thr_new ( "net_slow", slow_net, (void *) 0, 11, 0 );

    slow_net_sem = sem_signal_new ( SEM_FIFO );
    net_timer_hookup ( fast_net );
#endif

    /* Here is where we initialize hardware.
     * no network traffic until after this is done
     */
    net_state = NET_INIT;
    (void) safe_thr_new ( "net_initialize", net_hw_init, (void *) 0, 14, 0 );

    count = 0;
    while ( net_state != NET_RUN && count++ < NET_STARTUP_WAIT ) {
	printf ( "Net wait %d\n", count );
	thr_delay ( system_clock_rate );
    }

    if ( num_eth == 0 ) {
	net_state = NET_IDLE;
	return;
    }

    /* This will panic unless we are in NET_RUN */
    net_addr_get ( our_mac );

    arp_announce ();

    net_show ();

#ifdef notdef
    /* XXX - Doing this here interfered with tftp symbol table fetch */
    tcp_test ();	/* XXX XXX */
#endif

    /*
    show_arp_ping ( mmt_ip );
    show_arp_ping ( caliente_ip );
    show_arp_ping ( cholla_ip );
    ping ( cholla_ip );
    */
}

#ifdef NEW_SLOW_WAY
/* Do this now using a repeat.
 * This makes the need for a special hook
 * in the timer routine obsolete.
 */
static void
slow_net ( int xxx )
{
	arp_tick ();
	dns_tick ();
}
#else
/* Activated roughly at 1 Hz */
static void
slow_net ( int arg )
{
    	for ( ;; ) {
	    sem_block ( slow_net_sem );
	    arp_tick ();
	    dns_tick ();
	}
	/* NOTREACHED */
}

static int fast_net_clock = 0;

/* Called at the system clock rate
 * (either 100 or 1000 Hz, probably)
 */
static void
fast_net ( void )
{
	if ( ( fast_net_clock++ % system_clock_rate ) == 0 )
	    sem_unblock ( slow_net_sem );
}
#endif

/* Called from interrupt level to place a
 * packet on input queue and awaken handler thread.
 */

/* locking added only for BBB -- must be removed when
 * this is handled via receive interrupts */
void
net_rcv_bbb ( struct netbuf *nbp )
{
	int x;

	nbp->next = (struct netbuf *) 0;

	x = splhigh();	/* XXX XXX */
    	if ( inq_tail ) {
	    inq_tail->next = nbp;
	    inq_tail = nbp;
	} else {
	    inq_tail = nbp;
	    inq_head = nbp;
	}
	splx ( x );	/* XXX XXX */

	/*
	cpu_signal ( inq_sem );
	*/
	sem_unblock ( inq_sem );
}

void
net_rcv ( struct netbuf *nbp )
{
	int x;

	nbp->next = (struct netbuf *) 0;

    	if ( inq_tail ) {
	    inq_tail->next = nbp;
	    inq_tail = nbp;
	} else {
	    inq_tail = nbp;
	    inq_head = nbp;
	}

	cpu_signal ( inq_sem );
	// sem_unblock ( inq_sem );
}

/* Thread to process queue of arriving packets */
static void
net_thread ( int xxx )
{
    	struct netbuf *nbp;
	int x;

	for ( ;; ) {

	    x = splhigh ();
	    nbp = NULL;
	    if ( inq_head ) {
		nbp = inq_head;
		inq_head = nbp->next;
		if ( ! inq_head )
		    inq_tail = (struct netbuf *) 0;
	    }
	    splx ( x );

	    if ( nbp )
		net_handle ( nbp );
	    else
		sem_block_cpu ( inq_sem );
		/* XXX - hopefully avoiding race and deadlock */
	}
	/* NOTREACHED */
}

#ifdef oldway
/* Thread to process queue of arriving packets */
static void
net_thread ( int arg )
{
    	struct netbuf *nbp;

    	cpu_enter ();

	for ( ;; ) {
	    while ( inq_head ) {
		nbp = inq_head;
		inq_head = nbp->next;
		if ( ! inq_head )
		    inq_tail = (struct netbuf *) 0;
		cpu_leave ();

		net_handle ( nbp );

		cpu_enter ();
	    }

	    cpu_wait ( inq_sem );
	}
	/* NOTREACHED */
}
#endif

/* We see plenty of oddball packets, even on a home network
 * with a single Windows 7 machine on it.
 * That machine spews out endless SSDP packet traffic.
 */
static int oddball_count = 0;
static int total_count = 0;

static int not_our_mac ( struct netbuf *nbp )
{
	if ( memcmp ( nbp->eptr->dst, broad, ETH_ADDR_SIZE ) == 0 )
	    return 1;
	if ( memcmp ( nbp->eptr->dst, our_mac, ETH_ADDR_SIZE ) != 0 )
	    return 1;
	return 0;
}

static void
net_handle ( struct netbuf *nbp )
{
    	struct eth_hdr *ehp;

	nbp->ilen = nbp->elen - sizeof ( struct eth_hdr );

	/*
	printf ( "net_handle: %d, %d\n", nbp->elen, nbp->ilen );
	*/

	ehp = nbp->eptr;

	++total_count;

	if ( ehp->type == ETH_ARP_SWAP ) {
	    /*
	    printf ("net_handle: type = %04x len = %d ", ehp->type, nbp->elen );
	    printf ( "(ARP)\n" );
	    */
	    arp_rcv ( nbp );
	    return;
	}

	/* XXX - probably do not want to reject all of this ultimately
	 * but for now it helps with debugging, especially with chatty
	 * windows machines on the network.
	 */
	if ( not_our_mac ( nbp ) ) {
	    /*
	    printf ( "Rejected, dest: %s\n", ether2str(ehp->dst) );
	    */
	    netbuf_free ( nbp );
	    return;
	}

	if ( ehp->type == ETH_IP_SWAP ) {
	    /*
	    printf ("net_handle: type = %04x len = %d ", ehp->type, nbp->elen );
	    printf ( "(IP)\n" );
	    */
	    /*
	    dump_buf ( nbp->eptr, nbp->elen );
	    */
	    ip_rcv ( nbp );
	    return;
	}

	++oddball_count;
	/*
	printf (" oddball packet: %04x len = %d\n", ehp->type, nbp->elen );
	*/
	netbuf_free ( nbp );
}

void
net_show ( void )
{
	printf ( "My IP address is: %s (%08x)\n", ip2strl ( my_ip ), my_ip );
	printf ( "My MAC address is: %s\n", ether2str(our_mac) );

	printf ( "Gateway: %s\n", ip2strl ( gate_ip ) );
	printf ( "Packets processed: %d total (%d oddballs)\n", total_count, oddball_count );

#ifdef ARCH_X86
	if ( num_ee ) ee_show ();
	if ( num_rtl ) rtl_show ();
#endif
	if ( num_cpsw ) cpsw_show ();

	netbuf_show ();
	arp_show ();
	dns_cache_show ();
}

/* ----------------------------------------- */
/* device independent io routines. */
/* XXX */
/* ----------------------------------------- */

void
net_show_packet ( char *msg, struct netbuf *nbp )
{
	printf ( "%s, %d bytes\n", msg, nbp->elen );

	printf ( "ether src: %s\n", ether2str(nbp->eptr->src) );
	printf ( "ether dst: %s\n", ether2str(nbp->eptr->dst) );
	printf ( "ether type: %04x\n", nbp->eptr->type );

	if ( nbp->eptr->type == ETH_ARP_SWAP ) {
	    printf ( "ARP packet\n" );
	} else if ( nbp->eptr->type == ETH_IP_SWAP ) {
	    printf ( "ip src: %s (%08x)\n", ip2strl(nbp->iptr->src), nbp->iptr->src );
	    printf ( "ip dst: %s (%08x)\n", ip2strl(nbp->iptr->dst), nbp->iptr->dst );
	    printf ( "ip proto: %d\n", nbp->iptr->proto );
	} else
	    printf ( "unknown packet type\n" );


	dump_buf ( nbp->eptr, nbp->elen );
}

void
net_send ( struct netbuf *nbp )
{

    nbp->elen = nbp->ilen + sizeof(struct eth_hdr);

    if ( net_debug_f > 0 ) {
	net_show_packet ( "net_send", nbp );
	if ( net_debug_f == 1 )
	    net_debug_f = 0;
    }

#ifdef ARCH_X86
    if ( use_rtl )
	rtl_send ( nbp );
    else
	ee_send ( nbp );
#endif
    /*
    printf ("Sending packet\n" );
    */
    cpsw_send ( nbp );

    /* XXX - Someday we may have a transmit queue
     * so we must remain free to dispose the netbuf.
     */
    netbuf_free ( nbp );
}


/* We get bugs when we call this too soon before network
 * is initialized.
 * So I added the panic.  11-22-2015
 */
void
net_addr_get ( char *buf )
{
    if ( net_state != NET_RUN )
	panic ( "Premature MAC address access" );

#ifdef ARCH_X86
    if ( use_rtl )
	get_rtl_addr ( buf );
    else
	get_ee_addr ( buf );
#endif
	get_cpsw_addr ( buf );
}

/* ----------------------------------------- */
/* buffer handling. */
/* ----------------------------------------- */

/* XXX simple static block of memory (2M for 512)
 * asking for 512 puts skidoo into a reboot loop
 * (maybe a bug in the bss zeroing code ...
#define NUM_NETBUF	512
 * 2, 32, ... work OK.
 */
#define NUM_NETBUF	128

static char netbuf_buf[NUM_NETBUF * sizeof(struct netbuf)];

static struct netbuf *free;

static void
netbuf_init ( void )
{
	struct netbuf *ap;
	struct netbuf *end;
	int count = 0;

	ap = (struct netbuf *) netbuf_buf;
	end = &ap[NUM_NETBUF];

	free = (struct netbuf *) 0;

	for ( ap; ap < end; ap++ ) {
	    ap->next = free;
	    free = ap;
	    ++count;
	}

	printf ("%d net buffers initialized\n", count );
	netbuf_show ();
}

void
netbuf_show ( void )
{
	int count = 0;
	struct netbuf *ap;

	printf ( "Netbuf head shows: %08x\n", free );

	for ( ap = free; ap; ap = ap->next ) {
	    count++;
	    if ( count > (NUM_NETBUF+10) )	/* XXX */
		break;
	}

	printf ( "%d buffers free of %d\n", count, NUM_NETBUF );
}

/* get a netbuf, with lock */

struct netbuf *
netbuf_alloc ( void )
{
	struct netbuf *rv;

    	cpu_enter ();
	rv = netbuf_alloc_i ();
	cpu_leave ();

	return rv;
}

/* This can be called directly from interrupt level
 * since an ISR implicitly holds a lock.
 */
struct netbuf *
netbuf_alloc_i ( void )
{
	struct netbuf *rv;

	if ( ! free )
	    return (struct netbuf *) 0;

	rv = free;
	free = free->next;

	rv->refcount = 1;
	rv->bptr = rv->data;
	rv->eptr = (struct eth_hdr *) (rv->bptr + NETBUF_PREPAD);
	rv->iptr = (struct ip_hdr *) ((char *) rv->eptr + sizeof ( struct eth_hdr ));
	if ( ((unsigned long) rv->iptr) & 0x3 )
	    panic ( "Bad alignment for netbuf" );

	return rv;
}

void
netbuf_free ( struct netbuf *old )
{
	old->refcount--;
	if ( old->refcount > 0 )
	    return;

	old->next = free;
	free = old;
}

/* ------------------------------------------- */
/* ------------------------------------------- */

/* New way, gets pointer to bytes in network order */
char *
ip2str ( unsigned char *ip_arg )
{
    	static char buf[20];
	int i, len;
	char *p;

	len = 4;
	p = buf;
	for ( i=0; i<len; i++ ) {
	    sprintf ( p, "%d", ip_arg[i] );
	    p++;
	    if ( ip_arg[i] > 9 ) p++;
	    if ( ip_arg[i] > 99 ) p++;
	    if ( i < len-1 )
		*p++ = '.';
	}
	*p = '\0';

	return buf;
}

char *
ip2strl ( unsigned long ip_arg )
{
    	static char buf[20];
	int i, len;
	char *p;
	union { 
	    unsigned long ip_long;
	    unsigned char ip_buf[4]; 
	} ip_un;

	ip_un.ip_long = ip_arg;

	len = 4;
	p = buf;
	for ( i=0; i<len; i++ ) {
	    sprintf ( p, "%d", ip_un.ip_buf[i] );
	    p++;
	    if ( ip_un.ip_buf[i] > 9 ) p++;
	    if ( ip_un.ip_buf[i] > 99 ) p++;
	    if ( i < len-1 )
		*p++ = '.';
	}
	*p = '\0';

	return buf;
}

char *
ether2str ( unsigned char *bytes )
{
    	static char buf[20];
	int i, len;
	char *p;

	len = ETH_ADDR_SIZE;
	p = buf;
	for ( i=0; i<len; i++ ) {
	    sprintf ( p, "%02x", bytes[i] );
	    p += 2;
	    if ( i < len-1 )
		*p++ = ':';
	}
	*p = '\0';

	return buf;
}

/* generate an ephemeral port number.
 * Current practice recommends a number in
 * the range 49152 to 65535.
 * I just cycle through these numbers without
 * worrying about conflicts.
 */

static int next_ephem = 49152;

int
get_ephem_port ( void )
{
	int rv = next_ephem++;
	if ( next_ephem > 65535 )
	    next_ephem = 49152;
	return rv;
}

/* THE END */
