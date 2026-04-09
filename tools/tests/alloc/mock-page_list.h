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

#include "harness.h" /* For ASSERT() */

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
                unsigned int first_dirty : MAX_ORDER + 1;
#define INVALID_DIRTY_IDX ((1UL << (MAX_ORDER + 1)) - 1)
                bool          need_tlbflush:1;
                unsigned long scrub_state:2;
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
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0;
}
#define INIT_PAGE_LIST_HEAD(l) test_page_list_init(l)

/* Used by page_alloc.c */
#define page_list_for_each_safe(pos, tmp, list)    \
    for ( (pos) = page_list_first(list),           \
          (tmp) = (pos) ? (pos)->list_next : NULL; \
          (pos) != NULL;                           \
          (pos) = (tmp), (tmp) = (pos) ? (pos)->list_next : NULL )

typedef unsigned long        mfn_t;
static struct page_list_head test_page_list;

#define page_to_list(d, pg)          (&test_page_list)
#define page_list_add(pg, list)      test_page_list_add((pg), (list))
#define page_list_add_tail(pg, list) test_page_list_add_tail((pg), (list))
#define page_list_del(pg, list)      test_page_list_del((pg), (list))
#define page_list_empty(list)        test_page_list_empty((list))
#define page_list_first(list)        ((list)->head)
#define page_list_last(list)         ((list)->tail)
#define page_list_remove_head(list)  test_page_list_remove_head((list))

#define test_page_list_count(list) ((list)->count)
#define test_page_list_empty(list) ((list)->head == NULL)

static inline void test_page_list_add_common(struct page_info      *pg,
                                             struct page_list_head *list,
                                             bool                   at_tail)
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
        pg->list_prev         = list->tail;
        list->tail->list_next = pg;
        list->tail            = pg;
    }
    else
    {
        pg->list_next         = list->head;
        list->head->list_prev = pg;
        list->head            = pg;
    }

    list->count++;
}

#define test_page_list_add(pg, list) test_page_list_add_common(pg, list, false)
#define test_page_list_add_tail(pg, list) \
    test_page_list_add_common(pg, list, true)

static inline void test_page_list_del(struct page_info      *pg,
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

#endif /* _MOCK_PAGE_LIST_H_ */