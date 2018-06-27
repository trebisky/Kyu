/*
 * Copyright (C) 2018  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* Kyu project
 * Tom Trebisky 5-29-2018
 *
 * Pleasant definitions for GPIO bits in the Orange Pi.
 * The H3 chip in the Orange Pi has 7 GPIO, which each have
 * 32 bits, in theory anyway, but they are not all fully populated.
 * Only 28 bits make it to the 40 pin IO header.
 * The following gives 224 bit definitions.
 * It could certainly be weeded out, but I am too lazy - for now anyway.
 // static int gpio_count[] = { 22, 0, 19, 18, 16, 7, 14, 0, 0, 12 };
 */

#define GPIO_A_0	0
#define GPIO_A_1	1
#define GPIO_A_2	2
#define GPIO_A_3	3
#define GPIO_A_4	4
#define GPIO_A_5	5
#define GPIO_A_6	6
#define GPIO_A_7	7
#define GPIO_A_8	8
#define GPIO_A_9	9
#define GPIO_A_10	10
#define GPIO_A_11	11
#define GPIO_A_12	12
#define GPIO_A_13	13
#define GPIO_A_14	14
#define GPIO_A_15	15
#define GPIO_A_16	16
#define GPIO_A_17	17
#define GPIO_A_18	18
#define GPIO_A_19	19
#define GPIO_A_20	20
#define GPIO_A_21	21
#define GPIO_A_22	22
#define GPIO_A_23	23
#define GPIO_A_24	24
#define GPIO_A_25	25
#define GPIO_A_26	26
#define GPIO_A_27	27
#define GPIO_A_28	28
#define GPIO_A_29	29
#define GPIO_A_30	30
#define GPIO_A_31	31

/* No pins in GPIO_B, so we skip over it */

#define GPIO_C_0	64
#define GPIO_C_1	65
#define GPIO_C_2	66
#define GPIO_C_3	67
#define GPIO_C_4	68
#define GPIO_C_5	69
#define GPIO_C_6	70
#define GPIO_C_7	71
#define GPIO_C_8	72
#define GPIO_C_9	73
#define GPIO_C_10	74
#define GPIO_C_11	75
#define GPIO_C_12	76
#define GPIO_C_13	77
#define GPIO_C_14	78
#define GPIO_C_15	79
#define GPIO_C_16	80
#define GPIO_C_17	81
#define GPIO_C_18	82
#define GPIO_C_19	83
#define GPIO_C_20	84
#define GPIO_C_21	85
#define GPIO_C_22	86
#define GPIO_C_23	87
#define GPIO_C_24	88
#define GPIO_C_25	89
#define GPIO_C_26	90
#define GPIO_C_27	91
#define GPIO_C_28	92
#define GPIO_C_29	93
#define GPIO_C_30	94
#define GPIO_C_31	95

#define GPIO_D_0	96
#define GPIO_D_1	97
#define GPIO_D_2	98
#define GPIO_D_3	99
#define GPIO_D_4	100
#define GPIO_D_5	101
#define GPIO_D_6	102
#define GPIO_D_7	103
#define GPIO_D_8	104
#define GPIO_D_9	105
#define GPIO_D_10	106
#define GPIO_D_11	107
#define GPIO_D_12	108
#define GPIO_D_13	109
#define GPIO_D_14	110
#define GPIO_D_15	111
#define GPIO_D_16	112
#define GPIO_D_17	113
#define GPIO_D_18	114
#define GPIO_D_19	115
#define GPIO_D_20	116
#define GPIO_D_21	117
#define GPIO_D_22	118
#define GPIO_D_23	119
#define GPIO_D_24	120
#define GPIO_D_25	121
#define GPIO_D_26	122
#define GPIO_D_27	123
#define GPIO_D_28	124
#define GPIO_D_29	125
#define GPIO_D_30	126
#define GPIO_D_31	127

#define GPIO_E_0	128
#define GPIO_E_1	129
#define GPIO_E_2	130
#define GPIO_E_3	131
#define GPIO_E_4	132
#define GPIO_E_5	133
#define GPIO_E_6	134
#define GPIO_E_7	135
#define GPIO_E_8	136
#define GPIO_E_9	137
#define GPIO_E_10	138
#define GPIO_E_11	139
#define GPIO_E_12	140
#define GPIO_E_13	141
#define GPIO_E_14	142
#define GPIO_E_15	143
#define GPIO_E_16	144
#define GPIO_E_17	145
#define GPIO_E_18	146
#define GPIO_E_19	147
#define GPIO_E_20	148
#define GPIO_E_21	149
#define GPIO_E_22	150
#define GPIO_E_23	151
#define GPIO_E_24	152
#define GPIO_E_25	153
#define GPIO_E_26	154
#define GPIO_E_27	155
#define GPIO_E_28	156
#define GPIO_E_29	157
#define GPIO_E_30	158
#define GPIO_E_31	159

#define GPIO_F_0	160
#define GPIO_F_1	161
#define GPIO_F_2	162
#define GPIO_F_3	163
#define GPIO_F_4	164
#define GPIO_F_5	165
#define GPIO_F_6	166
#define GPIO_F_7	167
#define GPIO_F_8	168
#define GPIO_F_9	169
#define GPIO_F_10	170
#define GPIO_F_11	171
#define GPIO_F_12	172
#define GPIO_F_13	173
#define GPIO_F_14	174
#define GPIO_F_15	175
#define GPIO_F_16	176
#define GPIO_F_17	177
#define GPIO_F_18	178
#define GPIO_F_19	179
#define GPIO_F_20	180
#define GPIO_F_21	181
#define GPIO_F_22	182
#define GPIO_F_23	183
#define GPIO_F_24	184
#define GPIO_F_25	185
#define GPIO_F_26	186
#define GPIO_F_27	187
#define GPIO_F_28	188
#define GPIO_F_29	189
#define GPIO_F_30	190
#define GPIO_F_31	191

#define GPIO_G_0	192
#define GPIO_G_1	193
#define GPIO_G_2	194
#define GPIO_G_3	195
#define GPIO_G_4	196
#define GPIO_G_5	197
#define GPIO_G_6	198
#define GPIO_G_7	199
#define GPIO_G_8	200
#define GPIO_G_9	201
#define GPIO_G_10	202
#define GPIO_G_11	203
#define GPIO_G_12	204
#define GPIO_G_13	205
#define GPIO_G_14	206
#define GPIO_G_15	207
#define GPIO_G_16	208
#define GPIO_G_17	209
#define GPIO_G_18	210
#define GPIO_G_19	211
#define GPIO_G_20	212
#define GPIO_G_21	213
#define GPIO_G_22	214
#define GPIO_G_23	215
#define GPIO_G_24	216
#define GPIO_G_25	217
#define GPIO_G_26	218
#define GPIO_G_27	219
#define GPIO_G_28	220
#define GPIO_G_29	221
#define GPIO_G_30	222
#define GPIO_G_31	223

/* We ignore GPIO_H and GPIO_I, but need to skip over them */
/* GPIO_J is the R_PIO that has the "Power LED" */

#define GPIO_J_0	288
#define GPIO_J_1	289
#define GPIO_J_2	290
#define GPIO_J_3	291
#define GPIO_J_4	292
#define GPIO_J_5	293
#define GPIO_J_6	294
#define GPIO_J_7	295
#define GPIO_J_8	296
#define GPIO_J_9	297
#define GPIO_J_10	298
#define GPIO_J_11	299
#define GPIO_J_12	300
#define GPIO_J_13	301
#define GPIO_J_14	302
#define GPIO_J_15	303
#define GPIO_J_16	304
#define GPIO_J_17	305
#define GPIO_J_18	306
#define GPIO_J_19	307
#define GPIO_J_20	308
#define GPIO_J_21	309
#define GPIO_J_22	310
#define GPIO_J_23	311
#define GPIO_J_24	312
#define GPIO_J_25	313
#define GPIO_J_26	314
#define GPIO_J_27	315
#define GPIO_J_28	316
#define GPIO_J_29	317
#define GPIO_J_30	318
#define GPIO_J_31	319

/* They call this a "power" LED, but that is misleading */
/* Power is green, Status is red */
#define POWER_LED	GPIO_J_10
#define STATUS_LED	GPIO_A_15
#define STATUS_LED_NEO	GPIO_A_10

/* They call this a "power button", which is even more misleading */
#define POWER_BUTTON	GPIO_J_3

/* Aliases for GPIO pins on the Orange Pi 40 pin header.
 * (28 of them)
 */
#define OPI_PIN_13	GPIO_A_0
#define OPI_PIN_11	GPIO_A_1
#define OPI_PIN_22	GPIO_A_2
#define OPI_PIN_15	GPIO_A_3
#define OPI_PIN_7	GPIO_A_6
#define OPI_PIN_29	GPIO_A_7
#define OPI_PIN_31	GPIO_A_8
#define OPI_PIN_33	GPIO_A_9
#define OPI_PIN_35	GPIO_A_10
#define OPI_PIN_5	GPIO_A_11
#define OPI_PIN_3	GPIO_A_12
#define OPI_PIN_8	GPIO_A_13
#define OPI_PIN_10	GPIO_A_14
#define OPI_PIN_28	GPIO_A_18
#define OPI_PIN_27	GPIO_A_19
#define OPI_PIN_37	GPIO_A_20
#define OPI_PIN_26	GPIO_A_21
#define OPI_PIN_19	GPIO_C_0
#define OPI_PIN_21	GPIO_C_1
#define OPI_PIN_23	GPIO_C_2
#define OPI_PIN_24	GPIO_C_3
#define OPI_PIN_16	GPIO_C_4
#define OPI_PIN_18	GPIO_C_7
#define OPI_PIN_12	GPIO_D_14
#define OPI_PIN_38	GPIO_G_6
#define OPI_PIN_40	GPIO_G_7
#define OPI_PIN_32	GPIO_G_8
#define OPI_PIN_36	GPIO_G_9

/* Aliases for GPIO pins on the Nano Pi NEO
 * The schematic shows CON1 as the dual rows of 24 pins
 *  and CON2 as the single row of 12 pins.
 * In each case, pin 1 is away from the ethernet jack.
 */

/* THE END */
