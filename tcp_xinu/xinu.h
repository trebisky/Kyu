/* xinu.h - include all system header files */

#include <kernel.h>

#include <kyu_glue.h>

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
