/* dns.c
 * Handle a DNS cache and resolver.
 * T. Trebisky  5-24-2005
 */

#include "kyu.h"
#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "cpu.h"

/*
nameserver 208.67.222.222
nameserver 208.67.220.220
#nameserver 192.168.0.1
*/

/*
#define RESOLVER "10.0.0.54"
#define RESOLVER "128.196.100.50"
*/
#define RESOLVER "192.168.0.1"

/* In seconds */
#define LOOKUP_TIMEOUT	30

struct dns_info {
	unsigned short id;
	unsigned short flags;
	unsigned short nquery;
	unsigned short n_ans;
	unsigned short n_auth;
	unsigned short n_add;
	char buf[16];
};

struct rr_info {
	unsigned short type;
	unsigned short class;
	unsigned long ttl;
	unsigned short len;
	char buf[4];
};

#define	F_RESP	0x8000
#define	F_INV	0x0800	/* op = 1 */
#define	F_STAT	0x1000	/* op = 2 */
#define	F_AA	0x0400
#define	F_TC	0x0200
#define	F_RD	0x0100
#define	F_RA	0x0080

static int dns_id = 1;

#define STD_REQ		(F_RD)

/* Query types */
#define Q_A		1
#define Q_NS		2
#define Q_CNAME		5
#define Q_PTR		12
#define Q_HINFO		13
#define Q_MX		15
#define Q_AXFR		252
#define Q_ANY		255

/* Query classes */
#define C_IN		1

#define MAX_DNS_CACHE	32

#define F_PENDING	0x0001
#define F_PERM		0x0002
#define F_VALID		0x0004

struct dns_data {
    	char name[64];
	unsigned long ip_addr;
	int flags;
	int id;
	struct sem *sem;
	int ttl;
} dns_cache[MAX_DNS_CACHE];

unsigned long dns_lookup_t ( char *, int );
unsigned long dns_lookup ( char * );
static struct dns_data * dns_alloc ( void );
static unsigned long dns_cache_lookup ( char * );
void dns_show ( char * );
void dns_cache_show ( void );

/* Take a string of the form fred.dingus.edu
 * and convert it to a DNS packet string.
 */
static char *
dns_pack ( char *fqdn, char *buf )
{
	char *p;
	char *cp;
	int count;

	p = fqdn;

	for ( ;; ) {
	    if ( *p == '.' )
		return 0;

	    cp = buf++;
	    count = 0;
	    while ( *p && *p != '.' ) {
	    	*buf++ = *p++;
		count++;
	    }
	    *cp = count;

	    if ( ! *p )
	    	break;

	    /* skip the dot */
	    p++;
	}

	*buf++ = '\0';

	return buf;
}

static char *
dns_unpack ( char *pkt, char *data, char *buf )
{
    	char *p = data;
	int n;
	char *rv = (char *) 0;
	short val;

	while ( *p ) {
	    if ( (*p & 0xc0) == 0xc0 ) {
		if ( rv == (char *) 0 )
		    rv = p + 2;
		/*
		n = ntohs(*(short *)p) & 0x3fff;
		*/
		memcpy ( &val, p, sizeof(short) );
		n = ntohs(val) & 0x3fff;
		p = &pkt[n];
		continue;
	    }
	    n = *p++;
	    while ( n-- )
		*buf++ = *p++;
	    if ( *p )
		*buf++ = '.';
	}

	*buf++ = '\0';

	if ( ! rv )
	    rv = p + 1;

	return  rv;
}

static unsigned long dns_ip;

/*
#define REPLY_PORT	32770
*/

#define REPLY_PORT	53
#define DNS_PORT	53

void
dns_test ( void )
{
	dns_show ( "sol.as.arizona.edu" );
	dns_show ( "sol.as.arizona.edu" );
	dns_show ( "sol.as.arizona.edu" );
	dns_show ( "fish.edu" );
	dns_show ( "www.the-oasis.org" );
	dns_show ( "www.the-oasis.org" );
	dns_show ( "prep.ai.mit.edu" );
	dns_show ( "yahoo.com" );
	dns_show ( "hacksaw.mmto.arizona.edu" );
	dns_show ( "mount.mmto.arizona.edu" );
	dns_show ( "mmtvme1.mmto.arizona.edu" );
}

void
dns_show ( char *host )
{
	unsigned long ip;

	if ( ip = dns_lookup_t ( host, 2 ) )
	    printf ( "DNS lookup: %s --> %s\n", host, ip2strl ( ip ) );
	else
	    printf ( "DNS lookup: %s --> FAILED!\n", host );
}

unsigned long
dns_lookup_t ( char *host, int timeout )
{
	char dns_buf[128];
	struct dns_info *dp;
	char *np;
	unsigned long ipnum;
	struct dns_data *dcp;
	unsigned short val;

	/*
	printf ("DNS lookup started for %s\n", host );
	*/

	if ( ipnum = dns_cache_lookup ( host ) )
	    return ipnum;

	/* build request packet */
	dp = (struct dns_info *) dns_buf;
	/*
	dp->id = htons(dns_id++);
	*/
	dp->id = htons(dns_id);

	dp->flags = htons ( STD_REQ );
	dp->nquery = htons ( 1 );
	dp->n_ans = 0;
	dp->n_auth = 0;
	dp->n_add = 0;

	np = dns_pack ( host, dp->buf );

	/* must be careful about ARM alignment */
	val = htons ( Q_A );
	memcpy ( np, &val, 2 );
	np += 2;
	val = htons ( C_IN );
	memcpy ( np, &val, 2 );
	np += 2;

	/* build cache entry */
	dcp = dns_alloc ();
	dcp->flags = F_PENDING;
	dcp->ttl = timeout;
	dcp->id = dns_id++;
	strcpy ( dcp->name, host );

	dns_cache_show ();
	printf ("Waiting on DNS ID: %d\n", dcp->id );
	/*
	*/

	dcp->sem = sem_signal_new ( SEM_FIFO );

	/*
	printf ("DNS udp, sending to %s\n", ip2strl ( dns_ip ) );
	*/

	udp_send ( dns_ip, REPLY_PORT, DNS_PORT, (char *) dp, (char *)np - dns_buf );

	sem_block ( dcp->sem );
	sem_destroy ( dcp->sem );

	dns_cache_show ();

	if ( dcp->flags & F_VALID ) {
	    /*
	    printf ("DNS got it: %s\n", ip2strl ( dcp->ip_addr ) );
	    */
	    return dcp->ip_addr;
	}

	/* timed out or failed */
	/*
	*/
	printf ("DNS failed: %d\n", dcp->flags);
	dcp->flags = 0;
	return 0;
}

unsigned long
dns_lookup ( char *host )
{
	return dns_lookup_t ( host, LOOKUP_TIMEOUT );
}

void
dns_resp_show ( struct netbuf *nbp )
{
    	struct dns_info *dp;
	struct rr_info *rp;
	char buf[128];
	int rcode;
	char *cp;
	unsigned long ip;
	unsigned short sval;
	unsigned long lval;

	dp = (struct dns_info *) nbp->dptr;

    	printf ("Received DNS reply (%d bytes)\n", nbp->dlen );

	printf ("id = %d\n", ntohs(dp->id));
	printf ("flags = %04x\n", ntohs(dp->flags));
	rcode = ntohs(dp->flags) & 0x000f;
	if ( rcode ) {
	    printf ("OOPs, DNS return code: %d\n", rcode );
	    return;
	}

	/* skip the Query */
	cp = dns_unpack ( (char *) dp, dp->buf, buf );
	cp += 4;
	printf ("query: %s\n", buf );
	printf ("next: %d\n", cp-((char *)dp) );

	rp = (struct rr_info *) dns_unpack ( (char *) dp, cp, buf );
	printf ("name = %s\n", buf );
	printf ("next: %d\n", ((char *)rp)-((char *)dp) );

	memcpy ( &lval, &rp->ttl, 4);
	printf ("ttl = %d\n", ntohl(lval));

	memcpy ( &sval, &rp->len, 2);
	printf ("rlen = %d\n", ntohs(sval));

	memcpy ( &ip, rp->buf, 4);
	printf ("IP = %s\n", ip2strl ( ip ) );
}

void
dns_rcv ( struct netbuf *nbp )
{
    	struct dns_info *dp;
	struct rr_info *rp;
	struct dns_data *ap;
	char buf[128];
	char *cp;
	unsigned long ip;
	int i;
	int len;
	unsigned long ttl;

	dns_resp_show ( nbp );
	/*
	*/

	dp = (struct dns_info *) nbp->dptr;

	/* Ignore anything that isn't a response.
	 * (we aren't a DNS server, for crying
	 * out loud)
	 */
	if ( ! (ntohs(dp->flags) & F_RESP)  )
	    return;

	/* If we have a pending entry and get an error,
	 * we should wake it up, but we just let it timeout
	 * for now XXX XXX */

	if ( ntohs(dp->flags) & 0x000f )
	    return;

	/* skip the Query */
	cp = 4 + dns_unpack ( (char *) dp, dp->buf, buf );

	/* get the resource record */
	rp = (struct rr_info *) dns_unpack ( (char *) dp, cp, buf );

	/* ARM alignment issues */
	memcpy ( &len, &rp->len, 2 );
	if ( ntohs(len) != 4 )
	    return;

	/* ARM alignment issues */
	/*
	ip = *(unsigned long *) rp->buf;
	*/
	memcpy ( &ip, rp->buf, 4 );
	/*
	printf ("IP = %s\n", ip2strl(ip) );
	*/

	for ( i=0; i<MAX_DNS_CACHE; i++ ) {
	    ap = &dns_cache[i];
	    if ( (ap->flags & F_PENDING) &&
		ap->id == ntohs ( dp->id ) ) {
		    ap->flags = F_VALID;
		    memcpy ( &ttl, &rp->ttl, sizeof(long) );
		    ap->ttl = ntohl(ttl);
		    ap->ip_addr = ip;
		    printf ( "Adding entry for %s\n", ip2strl ( ip ) );
		    sem_unblock ( ap->sem );
		}
	}
	printf ( "dns_rcv is done\n" );
}

void
dns_init ( void )
{
	int i;

	if ( ! net_dots ( RESOLVER, &dns_ip ) )		/* mmt */
	    panic ( "resolver netdots" );

	for ( i=0; i<MAX_DNS_CACHE; i++ )
	    dns_cache[i].flags = 0;
}

/* This is called once a second */
void
dns_tick ( void )
{
	struct dns_data *ap;
	int i;

	for ( i=0; i<MAX_DNS_CACHE; i++ ) {
	    ap = &dns_cache[i];
	    if ( ! ap->flags )
		continue;
	    if ( ap->flags & F_PERM )	/* permanent */
		continue;

	    if ( ap->ttl )
		ap->ttl--;
	    else {
		printf ( "DNS entry for %s expired\n", ap->name );
		if ( ap->flags & F_PENDING )
		    sem_unblock ( ap->sem );
		ap->flags = 0;
	    }
	}
}

static unsigned long
dns_cache_lookup ( char *name )
{
	int i;

	for ( i=0; i<MAX_DNS_CACHE; i++ )
	    if ( (dns_cache[i].flags & F_VALID) &&
		strcmp ( name, dns_cache[i].name ) == 0 )
		    return dns_cache[i].ip_addr;

	return 0;
}

void
dns_cache_show ( void )
{
	struct dns_data *ap;
	int i;

	for ( i=0; i<MAX_DNS_CACHE; i++ ) {
	    ap = &dns_cache[i];

	    if ( ! ap->flags )
		continue;

	    printf ( "dns: %s at %s (ttl= %d)", ap->name,
		ip2strl ( ap->ip_addr ), ap->ttl );
	    printf ( " %04x\n", ap->flags );
	}
}

/* Find a free entry in the dns cache.
 * make second pass if needed to find the entry
 * nearly ready to expire.
 * ALWAYS returns something.
 * XXX - may want a lock here.
 */
static struct dns_data *
dns_alloc ( void )
{
	int i;
	struct dns_data *ap;
	struct dns_data *minp;

	for ( i=0; i<MAX_DNS_CACHE; i++ ) {
	    ap = &dns_cache[i];
	    if ( ! ap->flags )
	    	return ap;
	}

	minp = &dns_cache[0];
	for ( i=0; i<MAX_DNS_CACHE; i++ ) {
	    ap = &dns_cache[i];
	    if ( ! ap->flags )	/* XXX */
		continue;
	    if ( ap->ttl < minp->ttl )
	    	minp = ap;
	}
	return minp;
}

/* THE END */
