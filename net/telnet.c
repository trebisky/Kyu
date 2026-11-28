/*
 * Copyright (C) 2026  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * telnet.c
 * A stripped down telnet server
 * T. Trebisky  11-27-2026
 */

#include <arch/types.h>
#include <kyu.h>
#include <kyulib.h>

#include "thread.h"
#include "net.h"
#include "netbuf.h"
#include "arch/cpu.h"

#include "tcp.h"

/* The usual telnet port is 23, although 2323 is a common alternate.
 * port 22 is ssh
 */

#define TELNET_PORT	23
// #define TELNET_PORT	2323
#define PRI_TELNET	27

void telnet_thread ( long );

void
telnet_init ( void )
{
	(void) safe_thr_new ( "telnet-server", telnet_thread, (void *) 0, PRI_TELNET, 0 );
}

// Telnet protocol bytes (RFC 854)
#define IAC  255 // 0xFF: Interpret As Command
#define DONT 254 // 0xFE
#define DO   253 // 0xFD
#define WONT 252 // 0xFC
#define WILL 251 // 0xFB
#define SB   250 // 0xFA: Subnegotiation Begin
#define SE   240 // 0xF0: Subnegotiation End


static int
strip_telnet ( char *buf, int len, struct socket *so )
{
	static int state = 0; // 0=DATA, 1=IAC, 2=CMD, 3=SB
    static unsigned char cmd = 0;
    size_t out_len = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char b = buf[i];
        if (state == 0) {
            if (b == IAC) state = 1;
            else buf[out_len++] = b;
        } else if (state == 1) {
            if (b == IAC) { // Escaped 0xFF literal
                buf[out_len++] = IAC;
                state = 0;
            } else if (b == WILL || b == WONT || b == DO || b == DONT) {
                cmd = b;
                state = 2;
            } else if (b == SB) {
                state = 3;
            } else {
                state = 0; // 2-byte command (e.g., NOP, AYT)
            }
        } else if (state == 2) {
            // Politely refuse all client options to keep things simple
            unsigned char reply[3] = {IAC, (cmd == DO || cmd == DONT) ? WONT : DONT, b};
            // write(client_fd, reply, 3);
			printf ( "Telnet declines %02x %02x %02x\n",
					reply[0], reply[1], reply[2] );
			tcp_send ( so, reply, 3 );
            state = 0;
        } else if (state == 3) {
            if (b == SE) state = 0; // End of subnegotiation
        }
    }
    return out_len;
}

static void
run_telnet ( struct socket *so )
{
    char msg[128];
    int n;
	int nn;

    for ( ;; ) {
        n = tcp_recv ( so, msg, 128 );
        /* This happens when telnet closes
         * the connection.
         */
        if ( n == 0 )
			break;

		printf ( "Telnet rcv %d bytes\n", n );

		nn = strip_telnet ( msg, n, so );

		printf ( "Telnet stripped to %d bytes\n", nn );

		/* Echo */
		if ( nn > 0 )
			tcp_send ( so, msg, nn );

        if ( msg[0] == 'q' )
			break;
    }

    printf ( "telnet done, closing\n" );
    tcp_close ( so );

    strcpy ( msg, "Telnet: All Done\n" );
    tcp_send ( so, msg, strlen(msg) );
}


void
telnet_thread ( long xxx )
{
    struct socket *so;
    struct socket *cso;
    int fip;

	printf ( "Telnet server starting\n" );

    so = tcp_bind ( TELNET_PORT );
    if ( ! so ) {
        printf ( "bind fails\n" );
        return;
    }

    for ( ;; ) {
        cso = tcp_accept ( so );
        fip = tcp_getpeer_ip ( cso );
        printf ( "telnet server got a connection from: %s\n", ip2str32_h(fip) );

        /* Really shoud run this in new thread */
        run_telnet ( cso );
    }

	printf ( "Telnet server finished\n" );
}

/* THE END */
