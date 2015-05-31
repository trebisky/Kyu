/* net_main.c
 * T. Trebisky  3-24-2005
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

unsigned long agate_ip = 0x0100000a;	/* agate: 10.0.0.1 */
unsigned long trona_ip = 0x3600000a;	/* trona: 10.0.0.54 */

unsigned long mmt_ip = 0x3264c480;	/* mmt: 128.196.100.50 */
unsigned long caliente_ip = 0x3364c480;	/* caliente: 128.196.100.51 */
unsigned long cholla_ip = 0x3464c480;	/* cholla: 128.196.100.52 */

static void slow_net ( int );
static void fast_net ( void );

static void net_thread ( int );
static void netbuf_init ( void );
void netbuf_show ( void );
void net_show ( void );
static void net_handle ( struct netbuf * );

struct netbuf * netbuf_alloc ( void );
struct netbuf * netbuf_alloc_i ( void );

static int num_ee = 0;
static int num_rtl = 0;
static int num_el3 = 0;

static int use_rtl = 0;

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
static struct sem *outq_sem;

static struct sem *slow_net_sem;

void
net_init ( void )
{
    /*
    unsigned long xxx;
    if ( ! net_dots ( "128.196.100.54", &xxx ) )
	printf ( "Cannot parse dotted address\n" );
    printf ( "Addr = %08x, %s\n", xxx, ip2str ( xxx ) );
    */

    netbuf_init();
    arp_init();
    dns_init();

    inq_head = (struct netbuf *) 0;
    inq_tail = (struct netbuf *) 0;

    inq_sem = cpu_new ();
    if ( ! inq_sem )
	panic ("Cannot get net input semaphore");

    outq_sem = cpu_new ();
    if ( ! outq_sem )
	panic ("Cannot get net output semaphore");

    /* XXX review and revise priorities someday */
    (void) safe_thr_new ( "net", net_thread, (void *) 0, 10, 0 );
    (void) safe_thr_new ( "net_tmr", slow_net, (void *) 0, 11, 0 );

    slow_net_sem = sem_signal_new ( SEM_FIFO );
    net_tmr_hookup ( fast_net );

    num_ee = ee_init ();
    num_rtl = rtl_init ();
    num_el3 = el3_init ();

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

    if ( num_ee + num_rtl == 0 )
	return;

    arp_announce ();

    /*
    show_arp_ping ( mmt_ip );
    show_arp_ping ( caliente_ip );
    show_arp_ping ( cholla_ip );
    ping ( cholla_ip );
    */
}

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

/* Called roughly at 100 Hz */
static void
fast_net ( void )
{
	if ( (fast_net_clock++ % 100) == 0 )
	    sem_unblock ( slow_net_sem );
}

/* Called from interrupt level to place a
 * packet on input queue and awaken handler thread.
 */
void
net_rcv ( struct netbuf *nbp )
{
	nbp->next = (struct netbuf *) 0;

    	if ( inq_tail ) {
	    inq_tail->next = nbp;
	    inq_tail = nbp;
	} else {
	    inq_tail = nbp;
	    inq_head = nbp;
	}

	cpu_signal ( inq_sem );
}

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

static int oddball_count = 0;
static int total_count = 0;

static void
net_handle ( struct netbuf *nbp )
{
    	struct eth_hdr *ehp;

	nbp->ilen = nbp->elen - sizeof ( struct eth_hdr );

	ehp = nbp->eptr;

	/*
	printf ("net_handle: type = %04x len = %d\n", ehp->type, nbp->elen );
	*/
	++total_count;

	if ( ehp->type == ETH_ARP_SWAP ) {
	    arp_rcv ( nbp );
	} else if ( ehp->type == ETH_IP_SWAP ) {
	    ip_rcv ( nbp );
	} else {
	    ++oddball_count;
	    /*
	    printf ("Non ARP packet: %04x %d\n", ehp->type, nbp->elen );
	    */
	    netbuf_free ( nbp );
	}
}

void
net_show ( void )
{
	printf ( "My IP address is: %s (%08x)\n", ip2str ( my_ip ), my_ip );
	printf ( "Gateway: %s\n", ip2str ( gate_ip ) );
	printf ( "Packets processed: %d total (%d oddballs)\n", total_count, oddball_count );

	if ( num_ee ) ee_show ();
	if ( num_rtl ) rtl_show ();
	netbuf_show ();
	arp_show ();
}

/* ----------------------------------------- */
/* device independent io routines. */
/* XXX */
/* ----------------------------------------- */

void
net_send ( struct netbuf *nbp )
{
    if ( use_rtl )
	rtl_send ( nbp );
    else
	ee_send ( nbp );

    /* XXX - Someday we may have a transmit queue
     * so we must remain free to dispose the netbuf.
     */
    netbuf_free ( nbp );
}

void
net_addr_get ( char *buf )
{
    if ( use_rtl )
	get_rtl_addr ( buf );
    else
	get_ee_addr ( buf );
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

	for ( ap = free; ap; ap = ap->next )
	    count++;

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

	rv->bptr = rv->data;
	rv->eptr = (struct eth_hdr *) (rv->bptr + NETBUF_PREPAD);
	rv->iptr = (struct ip_hdr *) ((char *) rv->eptr + sizeof ( struct eth_hdr ));

	return rv;
}

void
netbuf_free ( struct netbuf *old )
{
	old->next = free;
	free = old;
}

/* ------------------------------------------- */
/* ------------------------------------------- */

char *
ip2str ( unsigned long ip_arg )
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


/* THE END */
