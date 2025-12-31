/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * ccu_clocks.c for the Orange Pi PC and PC 2 (H3 and H5)
 *
 * Never actually used.  Has interesting code from linux drivers.
 *
 * Tom Trebisky  8/24/2020
 *
 */
#include <arch/types.h>

#define BIT(nr)		(1<<(nr))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* See dt-bindings/clock/sun8i-r-ccu.h */

#define CLK_AR100               0

#define CLK_APB0_PIO            3
#define CLK_APB0_IR             4
#define CLK_APB0_TIMER          5
#define CLK_APB0_RSB            6
#define CLK_APB0_UART           7
/* 8 is reserved for CLK_APB0_W1 on A31 */
#define CLK_APB0_I2C            9
#define CLK_APB0_TWD            10
#define CLK_IR                  11

/* See clk/sunxi-ng/ccu-sun8i-r.h */

/* AHB/APB bus clocks are not exported */
#define CLK_AHB0        1
#define CLK_APB0        2

#define CLK_NUMBER      (CLK_IR + 1)

/* ----------------------------------------------- */

static const struct clk_parent_data ar100_parents[] = {
        { .fw_name = "losc" },
        { .fw_name = "hosc" },
        { .fw_name = "pll-periph" },
        { .fw_name = "iosc" },
};

static const struct ccu_mux_var_prediv ar100_predivs[] = {
        { .index = 2, .shift = 8, .width = 5 },
};

static struct ccu_div ar100_clk = {
        .div            = _SUNXI_CCU_DIV_FLAGS(4, 2, CLK_DIVIDER_POWER_OF_TWO),

        .mux            = {
                .shift  = 16,
                .width  = 2,

                .var_predivs    = ar100_predivs,
                .n_var_predivs  = ARRAY_SIZE(ar100_predivs),
        },

        .common         = {
                .reg            = 0x00,
                .features       = CCU_FEATURE_VARIABLE_PREDIV,
                .hw.init        = CLK_HW_INIT_PARENTS_DATA("ar100",
                                                           ar100_parents,
                                                           &ccu_div_ops,
                                                           0),
        },
};

static SUNXI_CCU_M(apb0_clk, "apb0", "ahb0", 0x0c, 0, 2, 0);

static SUNXI_CCU_GATE_HWS(apb0_pio_clk,         "apb0-pio",
                          apb0_gate_parent, 0x28, BIT(0), CLK_IGNORE_UNUSED);
static SUNXI_CCU_GATE_HWS(apb0_ir_clk,          "apb0-ir",
                          apb0_gate_parent, 0x28, BIT(1), CLK_IGNORE_UNUSED);
static SUNXI_CCU_GATE_HWS(apb0_timer_clk,       "apb0-timer",
                          apb0_gate_parent, 0x28, BIT(2), CLK_IGNORE_UNUSED);
// static SUNXI_CCU_GATE_HWS(apb0_rsb_clk,         "apb0-rsb",
//                           apb0_gate_parent, 0x28, BIT(3), CLK_IGNORE_UNUSED);
static SUNXI_CCU_GATE_HWS(apb0_uart_clk,        "apb0-uart",
                          apb0_gate_parent, 0x28, BIT(4), CLK_IGNORE_UNUSED);
static SUNXI_CCU_GATE_HWS(apb0_i2c_clk,         "apb0-i2c",
                          apb0_gate_parent, 0x28, BIT(6), CLK_IGNORE_UNUSED);
static SUNXI_CCU_GATE_HWS(apb0_twd_clk,         "apb0-twd",
                          apb0_gate_parent, 0x28, BIT(7), CLK_IGNORE_UNUSED);

static SUNXI_CCU_MP_WITH_MUX_GATE(ir_clk, "ir",
                                  r_mod0_default_parents, 0x54,
                                  0, 4,         /* M */
                                  16, 2,        /* P */
                                  24, 2,        /* mux */
                                  BIT(31),      /* gate */
                                  CLK_IGNORE_UNUSED);

/* ----------------------------------------------- */

/* See clk/sunxi-ng/ccu-sun8i-r.c
 */
static struct ccu_common *sun8i_h3_r_ccu_clks[] = {
        &ar100_clk.common,
        &apb0_clk.common,
        &apb0_pio_clk.common,
        &apb0_ir_clk.common,
        &apb0_timer_clk.common,
        &apb0_uart_clk.common,
        &apb0_i2c_clk.common,
        &apb0_twd_clk.common,
        &ir_clk.common,
};

static struct clk_hw_onecell_data sun8i_h3_r_hw_clks = {
        .hws    = {
                [CLK_AR100]             = &ar100_clk.common.hw,
                [CLK_AHB0]              = &ahb0_clk.hw,
                [CLK_APB0]              = &apb0_clk.common.hw,
                [CLK_APB0_PIO]          = &apb0_pio_clk.common.hw,
                [CLK_APB0_IR]           = &apb0_ir_clk.common.hw,
                [CLK_APB0_TIMER]        = &apb0_timer_clk.common.hw,
                [CLK_APB0_UART]         = &apb0_uart_clk.common.hw,
                [CLK_APB0_I2C]          = &apb0_i2c_clk.common.hw,
                [CLK_APB0_TWD]          = &apb0_twd_clk.common.hw,
                [CLK_IR]                = &ir_clk.common.hw,
        },
        .num    = CLK_NUMBER,
};

/* See clk/sunxi-ng/ccu-sun8i-h3.c
 */
/* XXX */

#define R_CCU_BASE	(void *) 0x01f01400
#define CCU_BASE 	(void *) 0x01c20000

static void
r_clocks_init ( void )
{
}

void
clocks_init ( void )
{
}

/* THE END */
