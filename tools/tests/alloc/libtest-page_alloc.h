/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit test library for functions in xen/common/page_alloc.c.
 *
 * This header-only library provides helper functions and definitions
 * for unit testing functions in xen/common/page_alloc.c, and is intended
 * to be included directly in the test program for those unit tests.
 *
 * The test program includes this header and then includes the real
 * page_alloc.c directly into the test program's translation unit.
 *
 * This allows the test program to reuse the definitions of any types
 * and functions from the headers that page_alloc.c includes, while
 * preventing the test harness from picking up unwanted definitions
 * from those headers that conflict with the test harness and prevent
 * clean compilation of the included page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#include <stdarg.h>
#include <execinfo.h>

#define TEST_USES_PAGE_ALLOC_SHIM
#include "page_alloc_shim.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../../xen/common/page_alloc.c"
#pragma GCC diagnostic pop

static const unsigned int node   = 0;
static const unsigned int order0 = 0;
static const unsigned int order1 = 1;
static const unsigned int order2 = 2;

/* Allows the logging spinlock/unlock mocks to identify the heap lock */
static spinlock_t *heap_lock_ptr = &heap_lock;

/*
 * Global state for the test page allocator shim and helpers.
 *
 * This includes the heap storage and availability counters that the test
 * scenarios manipulate, as well as the domain list and a bug counter for
 * the test program to track any unexpected conditions encountered in the
 * test helpers.
 */
static heap_by_zone_and_order_t test_heap_storage[MAX_NUMNODES];
static unsigned long            test_avail_storage[MAX_NUMNODES][NR_ZONES];
struct domain                  *domain_list;
typedef int                     zone_t;

static unsigned long __used test_mark_page_offline(struct page_info *page,
                                                   int               flag,
                                                   const char *caller_func)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    /* Avoid logging verbose logging while marking a page offline */
    testcase_assert_verbose_assertions = false;
    printf("%s: Marking page at MFN %lu as %s.\n", caller_func,
           page_to_mfn(page), flag ? "broken" : "offlined");
    mark_page_offline(page, flag);
    testcase_assert_verbose_assertions = save_verbose_asserts;
    return 0;
}
#define mark_page_offline(pg, flag) test_mark_page_offline(pg, flag, __func__)

#ifdef CONFIG_SYSCTL
/* Helper for just getting the number of free pages for ASSERTs */
static uint64_t free_pages(void)
{
    uint64_t free_pages, total_claims;
    bool     verbose_asserts_save = testcase_assert_verbose_assertions;

    /* Avoid logging spinlock actions while getting the free page count */
    testcase_assert_verbose_assertions = false;
    get_outstanding_claims(&free_pages, &total_claims);
    testcase_assert_verbose_assertions = verbose_asserts_save;
    return free_pages;
}
#define FREE_PAGES free_pages()

/* Helper for just getting the total number of claimed pages for ASSERTs */
static uint64_t total_claims(void)
{
    uint64_t free_pages, total_claims;
    bool     verbose_asserts_save = testcase_assert_verbose_assertions;

    /* Avoid logging spinlock actions while getting the total claims */
    testcase_assert_verbose_assertions = false;
    get_outstanding_claims(&free_pages, &total_claims);
    testcase_assert_verbose_assertions = verbose_asserts_save;
    return total_claims;
}
#define TOTAL_CLAIMS total_claims()

#endif /* CONFIG_SYSCTL */

/*
 * Helper functions to print the state of the heap and offlined pages
 * for traceability and to assert consistency of the heap and offlined
 * page state.
 *
 * These functions are called at various points in the test scenarios
 * to validate that the internal state of the allocator is consistent
 * with expectations.
 */

/* Function to print and assert the state of an offlined page.*/
static void print_and_assert_offlined_page(struct page_info *pos)
{
    printf("      mfn %lu: order %u, first_dirty %x\n", page_to_mfn(pos),
           PFN_ORDER(pos), pos->u.free.first_dirty);

    /*
     * The order of offlined pages must always be 0 because pages are only
     * offlined as standalone pages.  Higher-order pages on the offline lists
     * are not supported by reserve_offlined_page() and online_page().
     */
    CHECK(PFN_ORDER(pos) == 0, "All offlined pages must have order 0");

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

#define ASSERT_HEAP_CONSISTENCY(pages) \
    assert_heap_consistency(pages, ARRAY_SIZE(pages), __FILE__, __LINE__)

/* Function to print the order and first_dirty of each page for debugging. */
static void assert_heap_consistency(struct page_info *pages, size_t size,
                                    const char *file, int line)
{
    bool verbose_asserts_save = testcase_assert_verbose_assertions;

    printf("  %s:%d: %s():\n", file, line, __func__);
    /* Avoid logging internal ssertions while logging the free list status */
    testcase_assert_verbose_assertions = false;

    /*
     * Inside pages, first_dirty must (if not INVALID_DIRTY_IDX) index the
     * (first) page itself or a subpage within the page's range (<= 2^order).
     */
    for ( size_t i = 0; i < size; i++ )
    {
        unsigned long first_dirty = pages[i].u.free.first_dirty;
        unsigned int  tail_offset = (1U << PFN_ORDER(&pages[i])) - 1;

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

                    /* Assert the subpages of a buddy to have order-0. */
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
    testcase_assert_verbose_assertions = verbose_asserts_save;
}

/*
 * Failure reporting helper that prints the provided message, the test
 * caller context and a native backtrace before aborting.
 */
static void fail_with_ctx(const char *caller_file, const char *caller_func,
                          int caller_line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "\n- %s: Assertion failed: ", caller_func);
    va_start(ap, fmt);
    testcase_assert(false, caller_file, caller_line, caller_func, fmt, ap);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/*
 * Assert that a page_list matches the provided sequence of page pointers.
 *
 * The public helper below is a macro so call sites can provide a simple list
 * of page pointers while the implementation works over an ordinary array.
 */
static void __used assert_list_eq_array(struct page_list_head  *list,
                                        struct page_info *const expected[],
                                        unsigned int            nr_expected,
                                        const char             *call_file,
                                        const char             *caller_func,
                                        int                     caller_line)
{
    struct page_info *pos;
    unsigned int      index = 0;
    unsigned int      count = test_page_list_count(list);

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

    for ( pos = page_list_first(list); pos; pos = pos->list_next, index++ )
    {
        if ( index >= nr_expected )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list contains more elements than expected");

        if ( pos != expected[index] )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "element %u mismatch: expected %p, got %p", index,
                          expected[index], pos);

        if ( pos->list_prev != (index ? expected[index - 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_prev mismatch at index %u", index);

        if ( pos->list_next !=
             (index + 1 < nr_expected ? expected[index + 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_next mismatch at index %u", index);
    }

    if ( index != nr_expected )
        fail_with_ctx(
            call_file, caller_func, caller_line,
            "list element count consumed mismatch: expected %u, got %u",
            nr_expected, index);
}

#define ASSERT_LIST_EQUAL(list, ...)                                           \
    do                                                                         \
    {                                                                          \
        struct page_info *const expected[] = {__VA_ARGS__};                    \
        assert_list_eq_array((list), expected, ARRAY_SIZE(expected), __FILE__, \
                             __func__, __LINE__);                              \
    } while ( 0 )

/*
 * Functions for setting up test scenarios with a clean allocator state,
 * and for building synthetic buddy trees with the expected page_info state.
 */

/* Set up a bare minimum NUMA node topology. */
static void init_numa_node_data(unsigned int start_mfn)
{
    (void)start_mfn;
#ifdef CONFIG_NUMA
    /*
     * For simplicity, we assign each CPU to its own node, and set each
     * node's cpumask to contain just that CPU.
     *
     * If needed, we could easily modify this setup to have multiple CPUs
     * per node by adjusting the cpu_to_node assignments and node_to_cpumask
     * values accordingly.
     *
     * The test scenarios in this suite do not currently require multiple
     * CPUs per node, but we could extend them to do so if desired.
     *
     * This is just a default setup that the test scenarios can rely on,
     * and they are free to modify the cpu_to_node and node_to_cpumask
     * values as needed for their specific test cases.
     */
    for ( unsigned int i = 0; i < NR_CPUS; i++ )
        cpu_to_node[i] = i;

    /* Each node has a single CPU in its cpumask for simplicity */
    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
        node_to_cpumask[i] = (1UL << i);

    /* Initialize node data structures */
    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
    {
        /* Each node has 8 pages for testing for now */
        node_data[i].node_start_pfn     = start_mfn + (i * 8);
        node_data[i].node_present_pages = 8UL;
        node_data[i].node_spanned_pages = 8UL;
    }
#endif /* CONFIG_NUMA */
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
static void reset_page_alloc_state_func(const char  *caller_func,
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
    memset(&test_dummy_domain1, 0, sizeof(test_dummy_domain1));
    memset(&test_dummy_domain2, 0, sizeof(test_dummy_domain2));
    memset(&test_current_vcpu, 0, sizeof(test_current_vcpu));

    /* Reinitialise the global page lists manipulated by the allocator. */
    INIT_PAGE_LIST_HEAD(&page_offlined_list);
    INIT_PAGE_LIST_HEAD(&page_broken_list);
    INIT_PAGE_LIST_HEAD(&test_page_list);

    init_numa_node_data(start_mfn); /* Only used for NUMA-enabled builds */

    /* Reinitialise every per-zone, per-order free-list bucket. */
    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
    {
        _heap[node] = &test_heap_storage[node];
        avail[node] = test_avail_storage[node];
        for ( zone = 0; zone < NR_ZONES; zone++ )
            for ( order = 0; order <= MAX_ORDER; order++ )
                INIT_PAGE_LIST_HEAD(&heap(node, zone, order));
    }

    total_avail_pages  = 0;
    outstanding_claims = 0;
    /*
     * Initially, prevent free_heap_pages() merging successors/predecessors
     * by forcing mfn_valid() to return false for all MFNs until the test
     * until this test library explicitly adds support for it.
     */
    first_valid_mfn = INVALID_MFN_INITIALIZER;
    max_page        = 0;

    /* Provide a current vcpu/domain pair for code paths that inspect it. */
    test_current_vcpu.domain = &test_dummy_domain1;

    /* Provide the dummy domains for tests that need some domains */
    domain_list                     = &test_dummy_domain1;
    test_dummy_domain1.next_in_list = &test_dummy_domain2;

    test_dummy_domain1.domain_id = 1;
    test_dummy_domain2.domain_id = 2;
    test_dummy_domain1.max_pages = 6;
    test_dummy_domain2.max_pages = 6;
}

/*
 * Populate a page descriptor with the minimal state needed by
 * reserve_offlined_page().
 *
 * Tests build synthetic buddy trees by placing a small set of
 * page_info objects into allocator free lists. This helper
 * keeps that setup consistent across scenarios.
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
 * Initialize the given pages as a buddy of the requested order,
 * with the first page as the buddy head and the rest as subpages
 * of it, and add the intialised buddy to the heap.
 */
static zone_t __used page_list_add_buddy(struct page_info *pages,
                                         mfn_t start_mfn, unsigned int order,
                                         const char *caller)
{
    size_t i, num_pages = 1U << order;

    /*
     * Initialize the first page as the head of the buddy with the given order.
     * All pages are initialized as in-use as this is the API expected by
     * free_heap_pages() when it adds a buddy to the heap(). This model is
     * consistent with the way the boot allocator and online_page() handle
     * page initialization as well as the normal way for used pages to be freed.
     */
    init_test_page(&pages[0], start_mfn++, order, PGC_state_inuse);

    /* Add the subpages of the buddy as order-0 buddies to the heap */
    for ( i = 1; i < num_pages; i++ )
        init_test_page(&pages[i], start_mfn++, order0, PGC_state_inuse);

    /*
     * Add the created buddy to the heap. This uses the same code path as
     * freeing used pages and is consistent with the way the boot allocator
     * and online_page() handle page initialization. Using free_heap_pages()
     * has the additional benefit of ensuring that the heap structures are
     * consistent even if the internal logic of the heap management changes.
     *
     * For example, implementing NUMA claims adds new per-node claims counters
     * and logic to free_heap_pages(), so using it here ensures that the test
     * setup will be correct even after such changes.
     */
    printf("%s: Adding buddy of order %u at MFN %lu to the heap.\n", caller,
           order, page_to_mfn(&pages[0]));

    /* Avoid logging spinlocks and verbose assertions during initialization */
    bool verbose_asserts_save          = testcase_assert_verbose_assertions;
    testcase_assert_verbose_assertions = false;

    free_heap_pages(&pages[0], order, false);

    testcase_assert_verbose_assertions = verbose_asserts_save;
    return page_to_zone(&pages[0]);
}

#define test_page_list_add_buddy(pages, start_mfn, order) \
    page_list_add_buddy(pages, start_mfn, order, __func__)
