/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * mmu_dump.c for the ARM v8
 *
 * This is code from u-boot/arch/arm/cpu/armv8/cache_v8.c
 *
 * I found this MMU pretty print code tucked away there, with no ability
 *  to use it from u-boot and decided to transplant it here and put
 *  it to use.
 *
 * Tom Trebisky  5/2/2026
 */

#include "kyu.h"
#include "kyulib.h"
#include "board/board.h"
#include "cpu.h"

/* We ran this on the h5 5-2-2026 and this is the output:
 * Walking pagetable at 000000007fff0000, va_bits: 32. Using 3 levels
 * [0x00000000000000 - 0x00000040000000]     |  Block | RWX        | Device-nGnRnE | Non-shareable
 * [0x00000040000000 - 0x00000100000000]     |  Block | RWX        | Normal        | Inner-shareable
 *
 * Just 2 lines of output!  And nothing terribly amazing.
 *
 * XXX
 * However, after this seemingly runs OK we somewhat later get
 *  a synchronous abort -- so this has trashed memory or the
 *  stack in some way.  Is it worth it to track down and
 *  debug?  I don't think so at this time.
 */

/* Various necessary things.
 * We used ctags in the U-boot source to track them down.
 */
#define BITS_PER_LONG_LONG 64

#define GENMASK_ULL(h, l) \
    (((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

/* This was handled in include/linux/const.h
 * The idea is to generate a constand with a desired length
 */
#define UL(x)	(x##UL)

/* From arch/arm/include/asm/system.h
 */
static inline unsigned int current_el (void)
{
    unsigned long el;

    asm volatile("mrs %0, CurrentEL" : "=r" (el) : : "cc");
    return 3 & (el >> 2);
}

/* This is straight from U-boot
 */
#include "mmu.h"

/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */

/*
 *  Here is how things work with a 48 bit VA and 4k pages.
 *
 *  With 4k page granule, a virtual address is split into 4 lookup parts
 *  spanning 9 bits each:
 *
 *    _______________________________________________
 *   |       |       |       |       |       |       |
 *   |   0   |  Lv0  |  Lv1  |  Lv2  |  Lv3  |  off  |
 *   |_______|_______|_______|_______|_______|_______|
 *     63-48   47-39   38-30   29-21   20-12   11-00
 *
 *             mask        page size
 *
 *    Lv0: FF8000000000       --
 *    Lv1:   7FC0000000       1G
 *    Lv2:     3FE00000       2M
 *    Lv3:       1FF000       4K
 *    off:          FFF
 *
 *  The H5 has a 32 bit VA, but it is really treated as a 39 bit VA.
 *  Level 0 in the above is skipped and the bulk of the Level 1 table
 *  maps invalid 1G sections.  Only 4 of the 512 Lv1 entries are valid.
 *
 *    _______________________________________________
 *   |       |       |       |       |       |       |
 *   |   0   |    0  |  Lv1  |  Lv2  |  Lv3  |  off  |
 *   |_______|_______|_______|_______|_______|_______|
 *     63-48   47-39   38-30   29-21   20-12   11-00
 *
 *             mask        page size
 *
 *    Lv1:   7FC0000000       1G
 *    Lv2:     3FE00000       2M
 *    Lv3:       1FF000       4K
 *    off:          FFF
 */

static int
get_effective_el(void)
{
    int el = current_el();

#ifdef notdef
    if (el == 2) {
        u64 hcr_el2;

        /*
         * If we are using the EL2&0 translation regime, the TCR_EL2
         * looks like the EL1 version, even though we are in EL2.
         */
        __asm__ ("mrs %0, HCR_EL2\n" : "=r" (hcr_el2));
        if (hcr_el2 & BIT(HCR_EL2_E2H_BIT))
            return 1;
    }
#endif

    return el;
}


#define MAX_PTE_ENTRIES 512

static int
pte_type(u64 *pte)
{
    return *pte & PTE_TYPE_MASK;
}

/* Returns the LSB number for a PTE on level <level> */
static int
level2shift(int level)
{
    /* Page is 12 bits wide, every level translates 9 bits */
    return (12 + 9 * (3 - level));
}


#define ALL_ATTRS (3 << 8 | PMD_ATTRMASK)
#define PTE_IS_TABLE(pte, level) (pte_type(&(pte)) == PTE_TYPE_TABLE && (level) < 3)

enum walker_state {
    WALKER_STATE_START = 0,
    WALKER_STATE_TABLE,
    WALKER_STATE_REGION, /* block or page, depending on level */
};

/**
 * __pagetable_walk() - Walk through the pagetable and call cb() for each memory region
 *
 * This is a software implementation of the ARMv8-A MMU translation table walk. As per
 * section D5.4 of the ARMv8-A Architecture Reference Manual. It recursively walks the
 * 4 or 3 levels of the page table and calls the callback function for each discrete
 * region of memory (that being the discovery of a new table, a collection of blocks
 * with the same attributes, or of pages with the same attributes).
 *
 * U-Boot picks the smallest number of virtual address (VA) bits that it can based on the
 * memory map configured by the board. If this is less than 39 then the MMU will only use
 * 3 levels of translation instead of 3 - skipping level 0.
 *
 * Each level has 512 entries of 64-bits each. Each entry includes attribute bits and
 * an address. When the attribute bits indicate a table, the address is the physical
 * address of the table, so we can recursively call _pagetable_walk() on it (after calling
 * @cb). If instead they indicate a block or page, we record the start address and attributes
 * and continue walking until we find a region with different attributes, or the end of the
 * table, in either case we call @cb with the start and end address of the region.
 *
 * This approach can be used to fully emulate the MMU's translation table walk, as per
 * Figure D5-25 of the ARMv8-A Architecture Reference Manual.
 *
 * @addr:       The address of the table to walk
 * @tcr:        The TCR register value
 * @level:      The current level of the table
 * @cb:         The callback function to call for each region
 * @priv:       Private data to pass to the callback function
 */
static void
__pagetable_walk(u64 addr, u64 tcr, int level, pte_walker_cb_t cb, void *priv)
{
    u64 *table = (u64 *)addr;
    u64 attrs, last_attrs = 0, last_addr = 0, entry_start = 0;
    int i;
    u64 va_bits = 64 - (tcr & (BIT(6) - 1));
    static enum walker_state state[4] = { 0 };
    static bool exit;

	printf ( "PT walk: %016lx, level = %d\n", addr, level );

    if (!level) {
        exit = false;
        if (va_bits < 39)
            level = 1;
    }

    state[level] = WALKER_STATE_START;

    /* Walk through the table entries */
    for (i = 0; i < MAX_PTE_ENTRIES; i++) {
        u64 pte = table[i];
        u64 _addr = pte & GENMASK_ULL(va_bits, PAGE_SHIFT);

        if (exit)
            return;

        if (pte_type(&pte) == PTE_TYPE_FAULT)
            continue;

        attrs = pte & ALL_ATTRS;
        /* If we're currently inside a block or set of pages */
        if (state[level] > WALKER_STATE_START && state[level] != WALKER_STATE_TABLE) {
            /*
             * Continue walking if this entry has the same attributes as the last and
             * is one page/block away -- it's a contiguous region.
             */
            if (attrs == last_attrs && _addr == last_addr + (1 << level2shift(level))) {
                last_attrs = attrs;
                last_addr = _addr;
                continue;
            } else {
                /* We either hit a table or a new region */
                exit = cb(entry_start, last_addr + (1 << level2shift(level)),
                      va_bits, level, priv);
                if (exit)
                    return;
                state[level] = WALKER_STATE_START;
            }
        }

        last_attrs = attrs;
        last_addr = _addr;

        if (PTE_IS_TABLE(pte, level)) {
            /* After the end of the table might be corrupted data */
            if (!_addr || (pte & 0xfff) > 0x3ff)
                return;
            state[level] = WALKER_STATE_TABLE;
            /* Signify the start of a table */
            exit = cb(pte, 0, va_bits, level, priv);
            if (exit)
                return;

            /* Go down a level */
            __pagetable_walk(_addr, tcr, level + 1, cb, priv);
            state[level] = WALKER_STATE_START;
        } else if (pte_type(&pte) == PTE_TYPE_BLOCK || pte_type(&pte) == PTE_TYPE_PAGE) {
            /* We foud a block or page, start walking */
            entry_start = pte;
            state[level] = WALKER_STATE_REGION;
        }
    }

    if (state[level] > WALKER_STATE_START)
        exit = cb(entry_start, last_addr + (1 << level2shift(level)), va_bits, level, priv);
}

static void
pretty_print_pte_type(u64 pte)
{
    switch (pte_type(&pte)) {
    case PTE_TYPE_FAULT:
        printf(" %-5s", "Fault");
        break;
    case PTE_TYPE_BLOCK:
        printf(" %-5s", "Block");
        break;
    case PTE_TYPE_PAGE:
        printf(" %-5s", "Pages");
        break;
    default:
        printf(" %-5s", "Unk");
    }
}

static void
pretty_print_table_attrs(u64 pte)
{
    int ap = (pte & PTE_TABLE_AP) >> 61;

    printf(" | %2s %10s",
           (ap & 2) ? "RO" : "",
           (ap & 1) ? "!EL0" : "");
    printf(" | %3s %2s %2s",
           (pte & PTE_TABLE_PXN) ? "PXN" : "",
           (pte & PTE_TABLE_XN) ? "XN" : "",
           (pte & PTE_TABLE_NS) ? "NS" : "");
}


static void
pretty_print_block_attrs(u64 pte)
{
    u64 attrs = pte & PMD_ATTRINDX_MASK;
    u64 perm_attrs = pte & PMD_ATTRMASK;
    char mem_attrs[16] = { 0 };
    int cnt = 0;

    if (perm_attrs & PTE_BLOCK_PXN)
        cnt += snprintf(mem_attrs + cnt, sizeof(mem_attrs) - cnt, "PXN ");
    if (perm_attrs & PTE_BLOCK_UXN) {
        if (get_effective_el() == 1)
            cnt += snprintf(mem_attrs + cnt, sizeof(mem_attrs) - cnt, "UXN ");
        else
            cnt += snprintf(mem_attrs + cnt, sizeof(mem_attrs) - cnt, "XN ");
    }
    if (perm_attrs & PTE_BLOCK_RO)
        cnt += snprintf(mem_attrs + cnt, sizeof(mem_attrs) - cnt, "RO");
    if (!mem_attrs[0])
        snprintf(mem_attrs, sizeof(mem_attrs), "RWX ");

    printf(" | %-10s", mem_attrs);

    switch (attrs) {
    case PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE):
        printf(" | %-13s", "Device-nGnRnE");
        break;
    case PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE):
        printf(" | %-13s", "Device-nGnRE");
        break;
    case PTE_BLOCK_MEMTYPE(MT_DEVICE_GRE):
        printf(" | %-13s", "Device-GRE");
        break;
    case PTE_BLOCK_MEMTYPE(MT_NORMAL_NC):
        printf(" | %-13s", "Normal-NC");
        break;
    case PTE_BLOCK_MEMTYPE(MT_NORMAL):
        printf(" | %-13s", "Normal");
        break;
    default:
        printf(" | %-13s", "Unknown");
    }
}

static void
pretty_print_block_memtype(u64 pte)
{
    u64 share = pte & (3 << 8);

    switch (share) {
    case PTE_BLOCK_NON_SHARE:
        printf(" | %-16s", "Non-shareable");
        break;
    case PTE_BLOCK_OUTER_SHARE:
        printf(" | %-16s", "Outer-shareable");
        break;
    case PTE_BLOCK_INNER_SHARE:
        printf(" | %-16s", "Inner-shareable");
        break;
    default:
        printf(" | %-16s", "Unknown");
    }
}

static void
print_pte(u64 pte, int level)
{
    if (PTE_IS_TABLE(pte, level)) {
        printf(" %-5s", "Table");
        printf(" %-12s", "|");
        pretty_print_table_attrs(pte);
    } else {
        pretty_print_pte_type(pte);
        pretty_print_block_attrs(pte);
        pretty_print_block_memtype(pte);
    }
    printf("\n");
}

/**
 * pagetable_print_entry() - Callback function to print a single pagetable region
 *
 * This is the default callback used by @dump_pagetable(). It does some basic pretty
 * printing (see example in the U-Boot arm64 documentation). It can be replaced by
 * a custom callback function if more detailed information is needed.
 *
 * @start_attrs:    The start address and attributes of the region (or table address)
 * @end:        The end address of the region (or 0 if it's a table)
 * @va_bits:        The number of bits used for the virtual address
 * @level:      The level of the region
 * @priv:       Private data for the callback (unused)
 */
static bool
pagetable_print_entry(u64 start_attrs, u64 end, int va_bits, int level, void *priv)
{
    u64 _addr = start_attrs & GENMASK_ULL(va_bits, PAGE_SHIFT);
    int indent = va_bits < 39 ? level - 1 : level;

    printf("%*s", indent * 2, "");
    if (PTE_IS_TABLE(start_attrs, level))
        printf("[%#016llx]%19s", _addr, "");
    else
        printf("[%#016llx - %#016llx]", _addr, end);

    printf("%*s | ", (3 - level) * 2, "");

    print_pte(start_attrs, level);

    return false;
}

// void 
// walk_pagetable(u64 ttbr, u64 tcr, pte_walker_cb_t cb, void *priv)
// {
//     __pagetable_walk(ttbr, tcr, 0, cb, priv);
// }

void
dump_pagetable(u64 ttbr, u64 tcr)
{
    u64 va_bits = 64 - (tcr & (BIT(6) - 1));

    printf("Walking pagetable at %016lx, va_bits: %lld. Using %d levels\n",
		(void *)ttbr, va_bits, va_bits < 39 ? 3 : 4);
    // walk_pagetable(ttbr, tcr, pagetable_print_entry, NULL);
    __pagetable_walk ( ttbr, tcr, 0, pagetable_print_entry, NULL);
}

void
dump_mmu ( void )
{
	u64 ttbr, tcr;

    asm volatile("mrs %0, ttbr0_el2" : "=r" (ttbr) );
    asm volatile("mrs %0, tcr_el2" : "=r" (tcr) );

	// 8 bytes on aarch64, same as long
	// printf ( "Size of long long = %d\n", sizeof(long long) );

	printf ( "Dump mmu: %016lx %016lx\n", ttbr, tcr );

	dump_pagetable ( ttbr, tcr );
}

/* THE END */
