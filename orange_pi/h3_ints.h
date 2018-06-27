/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * h3_ints.h
 *  Kyu project  1-6-2017  Tom Trebisky
 *
 */

/* List of interrupt numbers for the Allwinner H3
 */

/* The H3 defines 157 interrupts (0-156)
 * We treat this as 160, more or less
 */
#define IRQ_SGI_0	0
#define IRQ_SGI_1	1
#define IRQ_SGI_2	2
#define IRQ_SGI_3	3
#define IRQ_SGI_4	4
#define IRQ_SGI_5	5
#define IRQ_SGI_6	6
#define IRQ_SGI_7	7
#define IRQ_SGI_8	8
#define IRQ_SGI_9	9
#define IRQ_SGI_10	10
#define IRQ_SGI_11	11
#define IRQ_SGI_12	12
#define IRQ_SGI_13	13
#define IRQ_SGI_14	14
#define IRQ_SGI_15	15

#define IRQ_PPI_0	16
#define IRQ_PPI_1	17
#define IRQ_PPI_2	18
#define IRQ_PPI_3	19
#define IRQ_PPI_4	20
#define IRQ_PPI_5	21
#define IRQ_PPI_6	22
#define IRQ_PPI_7	23
#define IRQ_PPI_8	24
#define IRQ_PPI_9	25
#define IRQ_PPI_10	26
#define IRQ_PPI_11	27
#define IRQ_PPI_12	28
#define IRQ_PPI_13	29
#define IRQ_PPI_14	30
#define IRQ_PPI_15	31

#define IRQ_UART0	32

#define IRQ_PIO_A	43
#define IRQ_PIO_E	49
#define IRQ_PIO_J	77	/* aka pio "L" */

#define IRQ_TIMER0	50
#define IRQ_EMAC	114	/* Ethernet */

/* Software generated interrupt channels */
#define SGI_0	0
#define SGI_1	1
#define SGI_2	2
#define SGI_3	3
#define SGI_4	4
#define SGI_5	5
#define SGI_6	6
#define SGI_7	7
#define SGI_8	8
#define SGI_9	9
#define SGI_10	10
#define SGI_11	11
#define SGI_12	12
#define SGI_13	13
#define SGI_14	14
#define SGI_15	15

/* THE END */
