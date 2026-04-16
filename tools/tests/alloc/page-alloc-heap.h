/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Includes page-alloc-wrapper and provides helpers for inspecting heap state.*
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_PAGE_ALLOC_HEAP_H
#define TOOLS_TESTS_ALLOC_PAGE_ALLOC_HEAP_H

#include <inttypes.h>
#include "page-alloc-wrapper.h"

struct PGC_flag_names {
    unsigned long flag;
    const char   *name;
} PGC_flag_names[] = {
    {.flag = PGC_need_scrub, "PGC_need_scrub"},
    {.flag = PGC_extra,      "PGC_extra"     },
    {.flag = PGC_broken,     "PGC_broken"    },
    {.flag = PGC_xen_heap,   "PGC_xen_heap"  },
};

static const char *pgc_state_name(unsigned long count_info)
{
    switch ( count_info & PGC_state )
    {
    case PGC_state_inuse:
        return "PGC_state_inuse";
    case PGC_state_offlining:
        return "PGC_state_offlining";
    case PGC_state_offlined:
        return "PGC_state_offlined";
    case PGC_state_free:
        return "PGC_state_free";
    default:
        assert("Invalid page state" && false);
    }
}

static bool page_aligned(struct page_info *pg)
{
    return IS_ALIGNED(mfn_x(page_to_mfn(pg)), 1UL << PFN_ORDER(pg));
}

static void print_list_location(struct page_list_head *list)
{
    bool found = false;

    if ( list == &page_offlined_list )
    {
        printf("page_offlined_list");
        found = true;
    }
    else if ( list == &page_broken_list )
    {
        printf("page_broken_list");
        found = true;
    }
    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
        for ( size_t zone = 0; zone < NR_ZONES; zone++ )
            for ( size_t order = 0; order <= MAX_ORDER; order++ )
                if ( &heap(node, zone, order) == (list) )
                {
                    printf("_heap(node %u zone %zu order %zu)", node, zone,
                           order);
                    found = true;
                }
    assert("List not found in known locations" && found);
}

static void print_page_count(unsigned long count_info)
{
    printf("        flags: %s", pgc_state_name(count_info));
    for ( size_t i = 0; i < ARRAY_SIZE(PGC_flag_names); i++ )
        if ( count_info & PGC_flag_names[i].flag )
            printf(" %s", PGC_flag_names[i].name);
    puts("");
}

static void print_page(struct page_info *pos)
{
    printf("      mfn %lu:", page_to_mfn(pos));

    if ( pos->u.free.first_dirty != INVALID_DIRTY_IDX )
        printf(" first_dirty %x", pos->u.free.first_dirty);

    /* Check whether the page is aligned to its order. */
    if ( !page_aligned(pos) )
        printf(" not aligned to order %u!", PFN_ORDER(pos));

    printf("\n");
    print_page_count(pos->count_info);
}

static void print_page_list(struct page_list_head *list, size_t order)
{
    struct page_info *pos = page_list_first(list);
    if ( pos )
    {
        printf("    List at ");
        print_list_location(list);
        printf(" for order %zu:\n", order);
    }
    while ( pos )
    {
        size_t page_order = PFN_ORDER(pos);

        print_page(pos);
        /* Print the subpages of the buddy head */
        for ( size_t sub_pg = 1; sub_pg < (1U << page_order); sub_pg++ )
        {
            struct page_info *sub_pos = pos + sub_pg;

            printf("  ");
            print_page(sub_pos);
            /* Assert the subpages of a buddy to have order-0. */
            assert(PFN_ORDER(sub_pos) == 0);
        }
        /* Assert that the page_order matches the heap order. */
        if ( page_order != order )
        {
            printf("ERROR:mfn %lu has order %zu but expected %zu "
                   "based on heap position\n",
                   page_to_mfn(pos), page_order, order);
            assert(page_order == order);
        }
        pos = pos->list_next;
    }
}

/* Print the heap structure for debugging. */
void print_heap(const char *file, int line)
{
    printf("  %s:%d: %s():\n", file, line, __func__);

    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
        for ( size_t order = 0; order <= MAX_ORDER; order++ )
            for ( size_t zone = 0; zone < NR_ZONES; zone++ )
                if ( page_list_first(&heap(node, zone, order)) )
                    print_page_list(&heap(node, zone, order), order);
}
#define PRINT_HEAP() print_heap(__FILE__, __LINE__)

#endif
