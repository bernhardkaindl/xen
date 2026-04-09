/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Mock page list implementation for testing page allocator functions.
 *
 * This mock implementation provides a simplified version of the page list
 * structures and functions used by the page allocator, allowing unit tests
 * to be written without relying on the full Xen environment.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef _MOCK_PAGE_LIST_H_
#define _MOCK_PAGE_LIST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "harness.h" /* Provides the generic Xen defintitions needed */

/*
 * Wrapper around xen/config.h for common Xen definitions for the test context
 */
#define __XEN_KCONFIG_H
#undef __nonnull
#undef offsetof
#include <xen/config.h>
/* Xen code adds cf_check as an attribute macro to functions we don't call */
#undef cf_check
#define cf_check __used

#define MAX_ORDER 20

/*
 * The page_info structures in the frame table are manipulated directly
 * by page_alloc.c, so they must be defined in a way that is consistent
 * with how page_alloc.c uses them, while being self-contained and suitable
 * for unit testing.
 *
 * The test scenarios can then manipulate the state of these page_info
 * structures to set up test conditions and verify the behavior of the
 * allocator in a way that is consistent with how it acts when used by
 * the running hypervisor.
 */
struct page_info {
    unsigned long count_info; /* PGC_state_inuse and other flags/counters */
    union {
        /* When the page is in use, u.inuse.type_info is used for status */
        struct {
            unsigned long type_info;
        } inuse;
        /*
         * When the page is free, u.free is used for buddy management.
         *
         * Using BUILD_BUG_ON(sizeof(var->u) != sizeof(long)), page_alloc
         * enforces that the u.free struct is exactly the size of a long.
         * 32-bit architectures need to use bitfields to fit the fields
         * in a long, while 64-bit architectures can use normal fields.
         */
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
#define PAGE_LIST_HEAD(name) struct page_list_head name = {NULL, NULL, 0}

static inline void test_page_list_init(struct page_list_head *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
#define INIT_PAGE_LIST_HEAD(l) test_page_list_init(l)

/* Used by page_alloc.c */
#define page_list_for_each_safe(pos, tmp, list)        \
        for ( (pos) = page_list_first(list),           \
              (tmp) = (pos) ? (pos)->list_next : NULL; \
              (pos) != NULL;                           \
              (pos) = (tmp), (tmp) = (pos) ? (pos)->list_next : NULL )

typedef unsigned long mfn_t;
static struct page_list_head test_page_list;

#define page_to_list(d, pg)          (&test_page_list)
#define page_list_add(pg, list)      test_page_list_add((pg), (list))
#define page_list_add_tail(pg, list) test_page_list_add_tail((pg), (list))
#define page_list_del(pg, list)      test_page_list_del((pg), (list))
#define page_list_empty(list)        ((list)->head == NULL)
#define page_list_first(list)        ((list)->head)
#define page_list_last(list)         ((list)->tail)
#define page_list_remove_head(list)  test_page_list_remove_head((list))

static inline void test_page_list_add_common(struct page_info *pg,
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

static inline void test_page_list_del(struct page_info *pg,
                                      struct page_list_head *list)
{
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
}

static inline struct page_info *
test_page_list_remove_head(struct page_list_head *list)
{
    struct page_info *pg = list->head;

    if ( pg )
        test_page_list_del(pg, list);

    return pg;
}

/*
 * The frame table is the foundation for the buddy allocator algorithm
 * implemented by page_alloc.c, and the test scenarios manipulate the
 * state of the page_info structures in the frame table to set up test
 * conditions and verify the behavior of the allocator.
 *
 * The frame table is indexed by MFN as required by the buddy allocator
 * algorithm in page_alloc.c, so the translation functions between page_info
 * pointers and MFNs are defined to allow page_alloc.c to manipulate the
 * page_info structures in the test frame table using MFN-based translations.
 */
extern struct page_info frame_table[];
#define page_to_mfn(pg)   ((mfn_t)((pg) - &frame_table[0]))
#define mfn_to_page(mfn)  (&frame_table[(mfn)])
#define mfn_valid(mfn)    (mfn >= first_valid_mfn && mfn < max_page)
#define maddr_to_page(pa) (CHECK(false, "Not implemented"))

/*
 * Helper functions to print the state of the heap and offlined pages
 * for reference while asserting consistency of the heap and offlined
 * page state.
 *
 * These functions are called at various points in the test scenarios
 * to validate that the internal state of the allocator is consistent
 * with expectations.
 */

/* Architecture-specific page state defines */
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

/* Print the count_info flags of a page_info for reference */
static void print_page_count_info(unsigned long count_info)
{
    printf("        flags: %s", pgc_state_name(count_info));
    for ( size_t i = 0; i < ARRAY_SIZE(PGC_flag_names); i++ )
        if ( count_info & PGC_flag_names[i].flag )
            printf(" %s", PGC_flag_names[i].name);
    puts("");
}

/* Print the state of a single page for reference */
static void print_page_info(struct page_info *pos)
{
    printf("      mfn %lu: order %u, first_dirty %x\n", page_to_mfn(pos),
           PFN_ORDER(pos), pos->u.free.first_dirty);
    print_page_count_info(pos->count_info);
}

/* Print and assert the state of an offlined page.*/
static void print_and_assert_offlined_page(struct page_info *pos)
{
    print_page_info(pos);
    /*
     * The order of offlined pages must always be 0 because pages are only
     * offlined as standalone pages.  Higher-order pages on the offline lists
     * are not supported by reserve_offlined_page() and online_page().
     */
    if ( PFN_ORDER(pos) != 0 )
        EXPECTED_TO_FAIL_BEGIN();
    CHECK(PFN_ORDER(pos) == 0, "All offlined pages must have order 0");
    if ( PFN_ORDER(pos) != 0 )
        EXPECTED_TO_FAIL_END(1);

    /*
     * Check the first_dirty index of offlined pages: Current code does
     * not use first_dirty for offlined pages as it only points to the
     * first dirty subpage within a buddy on the heap, and offlined pages
     * are not on the heap. As it is not used, current code sets it to
     * INVALID_DIRTY_IDX when offlining a page, so just confirm that.
     *
     * PS: Their scrubbing state is tracked by count_info & PG_need_scrub.
     * In case an offlined page is onlined, the onlining code will be
     * responsible to set first_dirty based on the scrubbing state.
     */
    if ( pos->u.free.first_dirty != INVALID_DIRTY_IDX )
    {
        printf("WARNING: offlined page at MFN %lu has first_dirty %x but "
               "expected INVALID_DIRTY_IDX\n",
               page_to_mfn(pos), pos->u.free.first_dirty);
        ASSERT(pos->u.free.first_dirty == INVALID_DIRTY_IDX);
    }
}

#endif /* _MOCK_PAGE_LIST_H_ */