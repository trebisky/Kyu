/* xinu.h - include all system header files */

#include <kernel.h>

/* Add these two for Kyu */
#include <kyu_glue.h>
#include <../net/netbuf.h>
#include <../arch/cpu.h>

// #define TCP_DEBUG

#ifdef TCP_DEBUG
#define kyu_printf(fmt ...)	printf ( fmt )
#else
#define kyu_printf(fmt ...)
#endif


#include <ether.h>
#include <net.h>
#include <ip.h>
#include <mq.h>
#include <tcp.h>
#include <tcb.h>
#include <timer.h>

#include <prototypes.h>

#define XINU_TCP_OUT_PRI	29
#define XINU_TIMER_PRI		28
