/* 
 * udptest.c
 *
 * This implements a UDP server that listens on port 6789 and replies
 *  back to whatever host/port sends it messages.
 *
 * This can be used to implement a "ping pong" test where two machines send
 *   UDP packets back and forth.
 *
 * One one machine just run "udptest".
 *  This will start a server that listens on port 6789
 *  This can be used for Kyu to test against.
 *
 * If you want to test between two linux machines, the "-s" switch
 *  is useful.  One one machine just run "udptest", then run "udptest -s" on the other.
 *
 * If it amuses you, you can poke at this with netcat.  Use something like:
 *
 *   echo -n "hello" | nc -4u host 6789
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* We listen on this and echo back to whatever host/port we heard from */
int server_port = 6789;

/* The following is only used to send the start packet */
char *server_host = "192.168.0.24";

void error ( char * );
void usage ( void );
void bind_return ( int );
void send_start_packet ( void );

#define BUFSIZE 2048
#define TEST_LEN 1024

char buf[BUFSIZE];

int main ( int argc, char **argv )
{
    int first = 1;
    int count;
    int len;
    int n;
    struct sockaddr_in peer_addr;
    int sockfd;
    int send_start = 0;
    int verbose = 0;
    unsigned long packet_num;

    argc--;
    argv++;
    if ( argc > 0 ) {
	if ( argc > 1 )
	    usage ();
	if ( strcmp ( *argv, "-s" ) == 0 )
	    send_start = 1;
	else if ( strcmp ( *argv, "-v" ) == 0 )
	    verbose = 1;
	else
	    usage ();
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) 
	error("ERROR opening socket");

    bind_return ( sockfd );

    if ( send_start )
	send_start_packet ();

    for ( ;; ) {
	// bzero(buf, BUFSIZE);
	len = sizeof(peer_addr);

	n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &peer_addr, &len);
	if (n < 0)
	    error("ERROR in recvfrom");

	count++;
	if ( (count % 1000) == 0 )
	    printf ( "%7d packets echoed\n", count );

	packet_num = * (unsigned long *) buf;


	if ( first ) {
	    char peer[32];

	    inet_ntop(AF_INET, &(peer_addr.sin_addr), peer, INET_ADDRSTRLEN);

	    printf("Received %d bytes from %s (port %d)\n", n, peer, ntohs( peer_addr.sin_port ) );
	    first = 0;
	}
    
	n = sendto(sockfd, buf, n, 0, (struct sockaddr *) &peer_addr, len);
	if (n < 0) 
	    error("ERROR in sendto");

	printf ( "Responded to %d\n", packet_num );
    }
  /* NOTREACHED */
}

void
bind_return ( int fd )
{
    int optval = 1;
    struct sockaddr_in return_addr;

    /* Without doing this, if we exit the program the bind lingers for about
     * 20 seconds and we have to wait before restarting the program.
     * Otherwise we get: "ERROR on binding: Address already in use"
     */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    bzero((char *) &return_addr, sizeof(return_addr));
    return_addr.sin_family = AF_INET;
    return_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return_addr.sin_port = htons((unsigned short)server_port);

    /*
    XXX - we really should bind a port for the return packets,
    otherwise they will come back to whatever random port the
    ephemeral port number gets assigned to and our firewall will
    block them all.
    */
    if (bind(fd, (struct sockaddr *) &return_addr, sizeof(return_addr)) < 0) 
	error("ERROR on binding");
}

void
send_start_packet ( void )
{
    struct sockaddr_in server_addr;
    unsigned long server_ip;
    int fd;

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ( fd < 0 ) 
	error("ERROR opening start socket");

    /* This yields network byte order */
    inet_pton(AF_INET, server_host, &server_ip );

    memset( (char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = server_ip;

    bind_return ( fd );

    memset ( buf, 0xab, 1024 );

    if ( sendto ( fd, buf, TEST_LEN, 0, (struct sockaddr *) &server_addr, sizeof(server_addr) ) < 0 )
	    error("ERROR in sendto");

    close ( fd );

    // printf ( "First packet sent !!\n" );
}

void
error ( char *msg )
{
    perror ( msg );
    exit ( 1 );
}

void
usage ( void )
{
    printf ( "Usage: udptest [-s]\n" );
    exit ( 1 );
}

/* THE END */
