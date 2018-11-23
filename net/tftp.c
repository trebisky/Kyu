/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * tftp.c
 * T. Trebisky  9-12-2015
 */

#include <arch/types.h>
#include "kyu.h"
#include "kyulib.h"
#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

#define TFTP_PORT	69
#define TFTP_SERVER	"192.168.0.5"

/* Note that the TFTP codes fit neatly in a byte,
 * even though the protocol gives them 2 bytes to fit in.
 * I take advantage of this to about byte swapping calls.
 */
#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERR	5

#define TFTP_BSIZE	512

static u32 tftp_ip;

static int tftp_verbose = 0;

int tftp_fetch ( char *, char *, int );

static void tftp_rcv ( struct netbuf * );
static void tftp_ack ( int );
static void tftp_debug ( int );
static void tftp_start ( char * );

/* ------------- */

#define TEST_TFTP
#ifdef TEST_TFTP
#define TEST_SIZE	200000
static char *test_file = "kyu.sym";
static char test_buf[TEST_SIZE];

/* Called from test.c */
void
test_tftp ( int arg )
{
	int count;

	tftp_debug ( 1 );
	count = tftp_fetch ( test_file, test_buf, TEST_SIZE );
	printf ( "TFTP test transfer finished: %d bytes\n", count );
}
#endif

/* ------------- */

static int local_port;		/* my ephemeral port */
static int server_port;		/* servers ephemeral port */

static char *	tftp_buf;
static int	tftp_limit;
static int	tftp_count;
static struct sem *tftp_sem;

/* set the server to use */
void
tftp_server ( char *server )
{
	 if ( ! net_dots ( server, &tftp_ip ) )
	     panic ( "tftp netdots" );
}

void
tftp_debug ( int arg )
{
	tftp_verbose = arg;
}

void
tftp_init ( void )
{
	tftp_server ( TFTP_SERVER );
	tftp_sem = sem_signal_new ( SEM_FIFO );
}

#define TFS_IDLE	0
#define TFS_START	1
#define TFS_RUN		2

static int tftp_state = TFS_IDLE;

/* Times in seconds to start and to finish */
#define TFTP_START_WAIT		5
#define TFTP_TOTAL_WAIT		60

/* Handle timeouts.
 * We are a bad boy and do timeouts our own way.
 * We watch that the initial packet gets a
 * response and that the whole transaction finishes
 * within a reasonable time.  Doing this properly
 * involves timing out each packet and handling
 * retransmits.  Not that hard, but we are lazy.
 */
static void
tftp_wdog ( long bogus )
{
	int delay;
	int hz = timer_rate_get();

	delay = TFTP_START_WAIT;

	while ( tftp_state == TFS_START && delay-- )
	    thr_delay ( hz );

	if ( tftp_state == TFS_START ) {
	    tftp_count = 0;
	    tftp_state = TFS_IDLE;
	    printf ( "TFTP server not listening\n" );
	    sem_unblock ( tftp_sem );
	}

	delay = TFTP_TOTAL_WAIT - TFTP_START_WAIT;

	while ( tftp_state != TFS_IDLE && delay-- )
	    thr_delay ( hz );

	if ( tftp_state != TFS_IDLE ) {
	    tftp_count = 0;
	    tftp_state = TFS_IDLE;
	    printf ( "TFTP transfer did not finish\n" );
	    sem_unblock ( tftp_sem );
	}
}

/* This is the main externally visible entry point */
int
tftp_fetch ( char *file, char *buf, int limit )
{
	local_port = get_ephem_port ();
	if ( tftp_verbose )
	    printf ( "TFTP listening on port %d\n", local_port );
	udp_hookup ( local_port, tftp_rcv );

	tftp_buf = buf;
	tftp_limit = limit;
	tftp_count = 0;

	tftp_state = TFS_START;
	(void) safe_thr_new ( "tftp_wdog", tftp_wdog, (void *) 0, 13, 0 );

	tftp_start ( file );

	/* Wait here for transfer to finish (or timeout) */
	sem_block ( tftp_sem );
	udp_unhook ( local_port );

	return tftp_count;
}

/* Set up and send RRQ packet to start a request */
static void
tftp_start ( char *file )
{
	char rrq_buf[64];
	char *p;

	p = rrq_buf;
	*p++ = 0;
	*p++ = TFTP_RRQ;
	strcpy ( p, file );
	p += strlen ( file ) + 1;
	strcpy ( p, "octet" );
	p += strlen ( "octet" ) + 1;

	udp_send ( tftp_ip, local_port, TFTP_PORT, rrq_buf, p - rrq_buf );
}

void
tftp_ack ( int rblock )
{
	char ack_buf[16];
	char *p;
	unsigned short rb;

	p = ack_buf;
	*p++ = 0;
	*p++ = TFTP_ACK;

	rb = rblock;
	memcpy ( p, (char *) &rb, 2 );

	/*
	printf ( "TFTP sending Ack: " );
	dump_buf ( ack_buf, 4 );
	*/

	udp_send ( tftp_ip, local_port, server_port, ack_buf, 4 );
}

void
tftp_rcv ( struct netbuf *nbp )
{
	int code;
	int block;
	unsigned short rblock;
	int count;

	/*
    	printf ("Received TFTP packet (%d bytes)\n", nbp->dlen );
	*/

	code = nbp->dptr[1];

	/* This may be expected if we are probing for a file
	 * that does not exist.
	 */
	if ( code == TFTP_ERR ) {
	    if ( tftp_verbose ) {
		dump_buf ( nbp->dptr, nbp->dlen );
		printf ( "TFTP error: %s\n", &nbp->dptr[4] );
	    }
	    tftp_count = 0;
	    sem_unblock ( tftp_sem );
	    tftp_state = TFS_IDLE;
	    return;
	}

	if ( code == TFTP_DATA ) {
	    tftp_state = TFS_RUN;
	    memcpy ( &rblock, &nbp->dptr[2], 2 );
	    block = ntohs ( rblock );
	    if ( block == 1 ) {
		server_port = udp_get_sport ( nbp );
		if ( tftp_verbose )
		    printf ( "TFTP server at port %d\n", server_port );
	    }

	    count = nbp->dlen - 4;
	    if ( tftp_verbose )
		printf ( "TFTP data block %d (%d bytes)\n", block, count );
	    if ( count && count < tftp_limit ) {
		memcpy ( tftp_buf, &nbp->dptr[4], count );
		tftp_buf += count;
		tftp_limit -= count;
		tftp_count += count;
	    }


	    tftp_ack ( rblock );

	    if ( count < TFTP_BSIZE ) {
		tftp_state = TFS_IDLE;
		if ( tftp_verbose )
		    printf ( "TFTP transfer finished, %d bytes received\n", tftp_count );
		sem_unblock ( tftp_sem );
	    }

	    tftp_state = TFS_IDLE;
	    return;
	}

	/* XXX - should never happen */
	printf ("TFTP unexpected opcode: %d\n", code );
	tftp_count = 0;
	tftp_state = TFS_IDLE;
	sem_unblock ( tftp_sem );
}

/* THE END */
