/* bsd.h
 * I collect all the include files here to make life
 * easier and nicer and better.
 * Tom Trebisky  11-15-2022
 */
#include <kyu_compat.h>

#define __P(protos)     protos
#define NULL    0

#define BSD     199306          /* System version (year & month). */
#define BSD4_3  1
#define BSD4_4  1

#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <domain.h>
#include <protosw.h>
#include <uio.h>

#include <socket.h>
#include <socketvar.h>

#include <if.h>

#include <in.h>
#include <in_var.h>
#include <in_systm.h>
#include <ip.h>
#include <in_pcb.h>
#include <ip_var.h>

#include <tcp.h>
#include <tcp_fsm.h>
#include <tcp_seq.h>
#include <tcp_timer.h>
#include <tcpip.h>
#include <tcp_var.h>
#include <tcp_debug.h>

#include <mbuf.h>
