/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* board_x86.c
 * Tom Trebisky  1/5/2017
 *
 */

#include "kyu.h"
#include "kyulib.h"
#include "board.h"

void
board_init ( void )
{
	/* Must call trap_init() first */
	trap_init ();
	timer_init ();
	/* timer_init ( DEFAULT_TIMER_RATE ); */
	kb_init ();
	sio_init ();

	/* CPU interrupts on */
	enable_irq ();

}

/* This gets called after the network is alive and well */
void
board_init_net ( void )
{
}

/* THE END */
