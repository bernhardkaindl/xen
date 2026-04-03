/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit test library for functions in common/page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#include "harness.h"
#include "page_alloc_shim.h"
#include <stdarg.h>
#include <execinfo.h>

#include "../../xen/common/page_alloc.c"

static const unsigned int node   = 0;
static const unsigned int order0 = 0;
static const unsigned int order1 = 1;
static const unsigned int order2 = 2;

static heap_by_zone_and_order_t test_heap_storage;
static unsigned long test_avail_storage[NR_ZONES];
static int bugs_encountered;
struct domain *domain_list;

static void print_and_assert_offlined_page(struct page_info *pos)
{
    printf("      mfn %lu: order %u, first_dirty %x\n", page_to_mfn(pos),
           PFN_ORDER(pos), pos->u.free.first_dirty);

    /*
     * The order of offlined pages must always be 0 because pages are only
     * offlined as standalone pages.  Higher-order pages on the offline lists
     * are not supported by reserve_offlined_page() and online_page().
     */
    if ( PFN_ORDER(pos) != 0 )
    {
        printf("ERROR: offlined page at MFN %lu has order %u but expected 0\n",
               page_to_mfn(pos), PFN_ORDER(pos));
        /*
         * Possible CRITICAL BUG in reserve_offlined_page():
         *
         * The initial implementation of reserve_offlined_page()
         * has a bug where when offlining the head of a buddy,
         * it does not update the order of the page to 0.
         *
         * While this may not cause functional issues as long as the
         * offlined page stays offline, page_online() forwards the
         * page to heap as well again without checking its order.
         *
         * This could cause heap corruption and crashes down the line
         * when the allocator attempts to merge, split and allocate
         * such pages.
         */
        bugs_encountered++;
    }
    /* FIXME: Fix this bug and enable: ASSERT(PFN_ORDER(pos) == 0); */

    /*
     * Assert that the first_dirty of offlined pages don't index a tail
     * that does not exist because of reserving the offlined page out of
     * a larger buddy.
     *
     * This is not a strict requirement for correctness, because first_dirty
     * is only meaningful for free pages on the heap, and an offlined page is
     * not on the heap and thus cannot be allocated, so it does not cause
     * functional issues if it is left with an inconsistent first_dirty index.
     *
     * If the page is onlined again, the allocator is responsible for making
     * that the first_dirty is valid again before putting it back to the heap.
     *
     * The actual information of the scubbing status of the standalone offlined
     * page is the PG_need_scrub bit in count_info, and the first_dirty field
     * is not meaningful after the page is offlined.
     *
     * However, for consistency, it is cleaner to not leave a stale first_dirty
     * index in the first_dirty field of an offlined page.
     */
    if ( pos->u.free.first_dirty != INVALID_DIRTY_IDX )
    {
        printf("ERROR: offlined page at MFN %lu has first_dirty %x but "
               "expected INVALID_DIRTY_IDX\n",
               page_to_mfn(pos), pos->u.free.first_dirty);
        ASSERT(pos->u.free.first_dirty == INVALID_DIRTY_IDX);
        /*
         * Note: When PG_need_scrub is set on the offlined page, it would be
         * cleaner to also set first_dirty to 0, but as it never read for an
         * offlined page, and onlining sets first_dirty, it is not a functional
         * issue if it is left as an invalid index (iow, indicating as clean),
         * even if that is inconsistent with the PG_need_scrub bit.
         * So we don't consider this a bug worth fixing in immediately, but
         * we could revisit fixing it to not cause confusion in the future.
         */
    }
}

#define ASSERT_HEAP_CONSISTENCY(pages) \
    assert_heap_consistency(pages, ARRAY_SIZE(pages), __FILE__, __LINE__)

/* Function to print the order and first_dirty of each page for debugging. */
static void assert_heap_consistency(struct page_info *pages, size_t size,
                                    const char *file, int line)
{
    printf("\n  %s:%d\n", file, line);

    /*
     * Inside pages, first_dirty must (if not INVALID_DIRTY_IDX) index the
     * (first) page itself or a subpage within the page's range (<= 2^order).
     */
    for ( size_t i = 0; i < size; i++ )
    {
        unsigned long first_dirty = pages[i].u.free.first_dirty;
        unsigned int tail_offset  = (1U << PFN_ORDER(&pages[i])) - 1;

        if ( first_dirty != INVALID_DIRTY_IDX && first_dirty > tail_offset )
        {
            printf("page at index %zu has first_dirty %lx but expected <= %u "
                   "based on its order\n",
                   i, first_dirty, tail_offset);
            ASSERT(pages[i].u.free.first_dirty == tail_offset);
        }
    }

    /* Traverse the offlined list, print and assert errors in it. */
    struct page_info *pos = page_list_first(&page_offlined_list);
    if ( pos )
        printf("    Offlined list:\n");
    while ( pos )
    {
        print_and_assert_offlined_page(pos);
        pos = pos->list_next;
    }

    /* Traverse the broken list, print and assert errors in it. */
    pos = page_list_first(&page_broken_list);
    if ( pos )
        printf("    Broken list:\n");
    while ( pos )
    {
        print_and_assert_offlined_page(pos);
        pos = pos->list_next;
    }

    /*
     * Traverse the _heap[node] for each order and zone and print and assert
     * the order and first_dirty of each page for each heap for debugging.
     *
     * This is to help verify that the heap structure is consistent with the
     * page_info order fields after operations that manipulate both, such as
     * reserve_offlined_page().
     */
    for ( size_t order = 0; order <= MAX_ORDER; order++ )
        for ( size_t zone = 0; zone < NR_ZONES; zone++ )
        {
            struct page_info *pos = page_list_first(&heap(node, zone, order));

            if ( pos )
                printf("    Heap for zone %zu, order %zu:\n", zone, order);
            while ( pos )
            {
                size_t page_order = PFN_ORDER(pos);

                printf("      mfn %lu: order %u, first_dirty %x\n",
                       page_to_mfn(pos), PFN_ORDER(pos),
                       pos->u.free.first_dirty);

                /* Print the subpages of the buddy */
                for ( size_t sub_pg = 1; sub_pg < (1U << page_order); sub_pg++ )
                {
                    struct page_info *sub_pos = pos + sub_pg;

                    printf("        mfn %lu: order %u, first_dirty %x\n",
                           page_to_mfn(sub_pos), PFN_ORDER(sub_pos),
                           sub_pos->u.free.first_dirty);

                    /* Assert that the subpages of a buddy are always order-0.
                     */
                    ASSERT(PFN_ORDER(sub_pos) == 0);
                }
                /* Assert that the page_order matches the heap order. */
                if ( page_order != order )
                {
                    printf("ERROR:mfn %lu has order %zu but expected %zu "
                           "based on heap position\n",
                           page_to_mfn(pos), page_order, order);
                    ASSERT(page_order == order);
                }

                pos = pos->list_next;
            }
        }
}

#define reset_page_alloc_state(mfn) reset_page_alloc_state_func(__func__, mfn)
/*
 * Reset all page_alloc translation-unit globals that these tests observe.
 *
 * The test program includes xen/common/page_alloc.c directly, so its file-scope
 * variables are part of this translation unit. Each test must start from a
 * clean heap, clean availability counters, and empty offlined/broken lists or
 * assertions from one scenario would bleed into the next.
 */
static void reset_page_alloc_state_func(const char *caller_func,
                                        unsigned int start_mfn)
{
    unsigned int zone;
    unsigned int order;

    printf("\n%s: start_mfn = %u\n", caller_func, start_mfn);

    /* Clear the backing storage used by the imported allocator globals. */
    memset(&test_heap_storage, 0, sizeof(test_heap_storage));
    memset(test_avail_storage, 0, sizeof(test_avail_storage));

    /* Clear the shim-owned singleton objects used by helper macros. */
    memset(&test_dummy_page, 0, sizeof(test_dummy_page));
    memset(&test_dummy_domain, 0, sizeof(test_dummy_domain));
    memset(&test_current_vcpu, 0, sizeof(test_current_vcpu));
    memset(&test_cpumask, 0, sizeof(test_cpumask));

    /* Repoint page_alloc.c at the test-owned heap and availability arrays. */
    _heap[0]           = &test_heap_storage;
    avail[node]        = test_avail_storage;
    total_avail_pages  = 0;
    outstanding_claims = 0;

    /* Reinitialise the global page lists manipulated by the allocator. */
    INIT_PAGE_LIST_HEAD(&page_offlined_list);
    INIT_PAGE_LIST_HEAD(&page_broken_list);
    INIT_PAGE_LIST_HEAD(&test_page_list);

    /* Reinitialise every per-zone, per-order free-list bucket. */
    for ( zone = 0; zone < NR_ZONES; zone++ )
        for ( order = 0; order <= MAX_ORDER; order++ )
            INIT_PAGE_LIST_HEAD(&heap(node, zone, order));

    /* Provide a current vcpu/domain pair for allocator paths that inspect it.
     */
    test_current_vcpu.domain = &test_dummy_domain;

    /* Provide the dummy domain for allocator paths that inspect it. */
    domain_list = &test_dummy_domain;
}

/*
 * Populate a page descriptor with the minimal state needed by
 * reserve_offlined_page().
 *
 * Tests build synthetic buddy trees by placing a small set of page_info objects
 * into allocator free lists. This helper keeps that setup consistent across
 * scenarios.
 */
static void init_test_page(struct page_info *page, mfn_t mfn,
                           unsigned int order, unsigned long state)
{
    memset(page, 0, sizeof(*page));

    /* Assign the synthetic identity used by page_alloc.c helper macros. */
    page->mfn = mfn;
    page->nid = 0;

    /* Model the page as a free buddy head of the requested order. */
    page->v.free.order = order;

    /* Default to no tracked dirty subrange and no active scrubbing. */
    page->u.free.first_dirty = INVALID_DIRTY_IDX;
    page->u.free.scrub_state = BUDDY_NOT_SCRUBBING;

    /* Install the requested allocator state bits for this synthetic page. */
    page->count_info = state;
}

/*
 * Failure reporting helper that prints the provided message, the test
 * caller context and a native backtrace before aborting.
 */
static void fail_with_ctx(const char *caller_file, const char *caller_func,
                          int caller_line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "Assertion failed: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\nCalled from: %s(): tools/tests/alloc/%s:%d\n",
            caller_func ? caller_func : "<unknown>",
            caller_file ? caller_file : "<unknown>", caller_line);

    /* Print a native backtrace to help debugging from the test. */
    {
        void *bt[32];
        int bt_size    = backtrace(bt, ARRAY_SIZE(bt));
        char **bt_syms = backtrace_symbols(bt, bt_size);

        if ( bt_syms )
        {
            fprintf(stderr, "Backtrace (%d frames):\n", bt_size);
            for ( int i = 0; i < bt_size; i++ )
                fprintf(stderr, "  %s\n", bt_syms[i]);
            free(bt_syms);
        }
    }

    abort();
}

/*
 * Assert that a page_list matches the provided sequence of page pointers.
 *
 * The public helper below is a macro so call sites can provide a simple list
 * of page pointers while the implementation works over an ordinary array.
 */
static void assert_list_eq_array(struct page_list_head *list,
                                 struct page_info *const expected[],
                                 unsigned int nr_expected,
                                 const char *call_file, const char *caller_func,
                                 int caller_line)
{
    struct page_info *pos;
    unsigned int i     = 0;
    unsigned int count = test_page_list_count(list);

    if ( count != nr_expected )
        fail_with_ctx(call_file, caller_func, caller_line,
                      "list count mismatch: expected %u, got %u", nr_expected,
                      count);

    if ( nr_expected == 0 )
    {
        if ( page_list_first(list) != NULL )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "expected empty list but head != NULL");
        return;
    }

    if ( page_list_first(list) != expected[0] )
        fail_with_ctx(call_file, caller_func, caller_line,
                      "list head mismatch: expected %p, got %p", expected[0],
                      page_list_first(list));

    for ( pos = page_list_first(list); pos; pos = pos->list_next, i++ )
    {
        if ( i >= nr_expected )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list contains more elements than expected");

        if ( pos != expected[i] )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "element %u mismatch: expected %p, got %p", i,
                          expected[i], pos);

        if ( pos->list_prev != (i ? expected[i - 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_prev mismatch at index %u", i);

        if ( pos->list_next != (i + 1 < nr_expected ? expected[i + 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_next mismatch at index %u", i);
    }

    if ( i != nr_expected )
        fail_with_ctx(
            call_file, caller_func, caller_line,
            "list element count consumed mismatch: expected %u, got %u",
            nr_expected, i);
}

#define ASSERT_LIST_EQUAL(list, ...)                                           \
    do                                                                         \
    {                                                                          \
        struct page_info *const expected[] = {__VA_ARGS__};                    \
        assert_list_eq_array((list), expected, ARRAY_SIZE(expected), __FILE__, \
                             __func__, __LINE__);                              \
    } while ( 0 )
