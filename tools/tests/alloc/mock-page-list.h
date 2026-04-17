/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Mock page-list implementation for page allocator tests.
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_ALLOC_MOCK_PAGE_LIST_H
#define TOOLS_TESTS_ALLOC_MOCK_PAGE_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "harness.h"

#define __XEN_KCONFIG_H
#undef __nonnull
#undef offsetof
#include <xen/config.h>
#undef cf_check
#define cf_check __used

#define MAX_ORDER 20

struct page_info {
    unsigned long count_info;
    union {
        struct {
            unsigned long type_info;
        } inuse;
        union {
            struct {
                unsigned int  first_dirty : MAX_ORDER + 1;
#define INVALID_DIRTY_IDX ((1UL << (MAX_ORDER + 1)) - 1)
                bool          need_tlbflush : 1;
                unsigned long scrub_state : 2;
#define BUDDY_NOT_SCRUBBING 0
#define BUDDY_SCRUBBING     1
#define BUDDY_SCRUB_ABORT   2
            };
            unsigned long val;
        } free;
    } u;
    union {
        struct {
            unsigned int order;
#define PFN_ORDER(pg) ((pg)->v.free.order)
        } free;
        unsigned long type_info;
    } v;
    uint32_t          tlbflush_timestamp;
    struct domain    *owner;
    struct page_info *list_next;
    struct page_info *list_prev;
};

struct page_list_head {
    struct page_info *head;
    struct page_info *tail;
    unsigned int      count;
};
static struct page_list_head test_page_list;
typedef unsigned long mfn_t;

#define PAGE_LIST_HEAD(name) struct page_list_head name = {NULL, NULL, 0}

extern struct page_info frame_table[];
#define page_to_mfn(pg)   ((mfn_t)((pg) - &frame_table[0]))
#define mfn_to_page(mfn)  (&frame_table[(mfn)])
#define mfn_valid(mfn)    (mfn >= first_valid_mfn && mfn < max_page)
#define maddr_to_page(pa) (CHECK(false, "Not implemented"))

static void test_page_list_init(struct page_list_head *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
#define INIT_PAGE_LIST_HEAD(l)       test_page_list_init(l)
#define page_list_empty(list)        ((list)->head == NULL)
#define page_list_first(list)        ((list)->head)
#define page_list_last(list)         ((list)->tail)
#define page_list_remove_head(list)  test_page_list_del((list)->head, (list))
#define page_to_list(d, pg)          (&test_page_list)
#define page_list_add(pg, list)      test_page_list_add(pg, list)
#define page_list_add_tail(pg, list) test_page_list_add_tail(pg, list)
#define page_list_del(pg, list)      test_page_list_del(pg, list);
#define page_list_for_each_safe(pos, tmp, list)        \
        for ( (pos) = page_list_first(list),           \
              (tmp) = (pos) ? (pos)->list_next : NULL; \
              (pos) != NULL;                           \
              (pos) = (tmp), (tmp) = (pos) ? (pos)->list_next : NULL )

static void test_page_list_add_common(struct page_info *pg,
                                      struct page_list_head *list,
                                      bool at_tail)
{
    pg->list_next = NULL;
    pg->list_prev = NULL;

    if ( list->head == NULL )
    {
        list->head = pg;
        list->tail = pg;
    }
    else if ( at_tail )
    {
        pg->list_prev = list->tail;
        list->tail->list_next = pg;
        list->tail = pg;
    }
    else
    {
        pg->list_next = list->head;
        list->head->list_prev = pg;
        list->head = pg;
    }
    list->count++;
}

#define test_page_list_add(pg, list) test_page_list_add_common(pg, list, false)
#define test_page_list_add_tail(pg, list) \
        test_page_list_add_common(pg, list, true)

static struct page_info *test_page_list_del(struct page_info *pg,
                                            struct page_list_head *list)
{
    if ( !pg )
        return NULL;
    if ( pg->list_prev )
        pg->list_prev->list_next = pg->list_next;
    else
        list->head = pg->list_next;

    if ( pg->list_next )
        pg->list_next->list_prev = pg->list_prev;
    else
        list->tail = pg->list_prev;

    pg->list_next = NULL;
    pg->list_prev = NULL;

    ASSERT(list->count > 0);
    list->count--;
    return pg;
}

#define PG_shift(idx)         (BITS_PER_LONG - (idx))
#define PG_mask(x, idx)       (x##UL << PG_shift(idx))
#define PGT_count_width       PG_shift(2)
#define PGT_count_mask        ((1UL << PGT_count_width) - 1)
#define PGC_allocated         PG_mask(1, 1)
#define PGC_xen_heap          PG_mask(1, 2)
#define _PGC_need_scrub       PG_shift(4)
#define PGC_need_scrub        PG_mask(1, 4)
#define _PGC_broken           PG_shift(7)
#define PGC_broken            PG_mask(1, 7)
#define PGC_state             PG_mask(3, 9)
#define PGC_state_inuse       PG_mask(0, 9)
#define PGC_state_offlining   PG_mask(1, 9)
#define PGC_state_offlined    PG_mask(2, 9)
#define PGC_state_free        PG_mask(3, 9)
#define page_state_is(pg, st) (((pg)->count_info & PGC_state) == PGC_state_##st)
#define PGC_count_width       PG_shift(9)
#define PGC_count_mask        ((1UL << PGC_count_width) - 1)
#define _PGC_extra            PG_shift(10)
#define PGC_extra             PG_mask(1, 10)
#endif
