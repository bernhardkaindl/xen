/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test framework for xen/common/page_alloc.c, consisting of:
 *
 * 1. A header-only shim that provides the definitions and helpers
 *    needed for the test framework to include the real page_alloc.c
 *    directly into its translation unit.
 *
 * 2. A set of mocks for the Xen types and functions used by page_alloc.c,
 *    sufficient to support the test scenarios in tools/tests/alloc.
 *
 *    This includes NUMA-topology mocks that let test scenarios manipulate
 *    allocator state and verify behaviour consistent with the running Xen
 *    hypervisor, while remaining self-contained and suitable for unit and
 *    integration testing.
 *
 * 3. A wrapper that includes the real page_alloc.c for testing.
 *
 *    It intercepts key page_alloc.c functions for logging and verification,
 *    allowing test scenarios to log the actions taken and the resulting state,
 *    making allocator behaviour easier to follow during test execution.
 *
 * 4. A library for NUMA heap initialisation and heap-state assertions.
 *
 *    This library provides helpers to prepare allocator state for the
 *    test scenarios, including:
 *
 *    a. Initialising the heap before each test case, creating NUMA nodes,
 *       and adding pages to the heap in specific states, such as free,
 *       allocated, marked to be offlined or already offlined.
 *
 *    b. Verifying heap state and page_info contents after test actions,
 *       such as checking that pages were allocated or freed as expected
 *       and that page_info state remains consistent.
 *
 * 5. Test-case lifecycle management, such as initialising the test context
 *    before each test case, printing the outcome of each test case,
 *    tracking the number of assertions, logging assertions with file
 *    and line information, and printing a summary report at the end.
 *
 * 6. A Makefile for discovering, compiling, running the test cases,
 *    and reporting which test cases were run.
 *
 *    The Makefile supports running individual test cases or the full
 *    test suite for all supported CPU architectures.
 *
 *    It also builds the tests with address sanitizer (ASAN) enabled to
 *    catch memory errors in both the page allocator and the test code,
 *    especially when manipulating page_info state within the scenarios.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_ALLOC_LIBTEST_PAGE_ALLOC_H
#define TOOLS_TESTS_ALLOC_LIBTEST_PAGE_ALLOC_H

#include <stdarg.h>
#include <execinfo.h>
#include <string.h>

/* Enable -Wextra warnings as errors to catch e.g. sign-compare issues */
#pragma GCC diagnostic error "-Wextra"

/* Support for printing the status of pages for debugging */
struct page_info;
static void print_page_info(struct page_info *pos);
static void print_page_count_info(unsigned long count_info);

#define TEST_USES_PAGE_ALLOC_SHIM
#include "page-alloc-shim.h"

/* Include the real page_alloc.c for testing */

#include "page-alloc-wrapper.h"

static const unsigned int node = 0;
static const unsigned int node0 = 0;
static const unsigned int node1 = 1;
static const unsigned int order0 = 0;
static const unsigned int order1 = 1;
static const unsigned int order2 = 2;
static const unsigned int order3 = 3;

/**
 * Functions for setting up test scenarios with a clean allocator state,
 * and for building synthetic buddy trees with the expected page_info state.
 */

/* Set up a bare minimum NUMA node topology. */
static void init_numa_node_data(unsigned int start_mfn)
{
    (void)start_mfn;
#ifdef CONFIG_NUMA
    /*
     * Use a simple default topology: one CPU per node, with each node's
     * cpumask containing only that CPU.
     *
     * The current tests do not require multiple CPUs per node, but they
     * can override cpu_to_node[] and node_to_cpumask[] if needed.
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
        node_data[i].node_start_pfn = start_mfn + (i * 8);
        node_data[i].node_present_pages = 8UL;
        node_data[i].node_spanned_pages = 8UL;
    }

    /*
     * Set up memnodemap so that mfn_to_nid() correctly resolves MFN
     * ranges to NUMA nodes: with memnode_shift=3 each memnodemap entry
     * covers 8 MFNs (2^3), matching the 8-page-per-node layout above.
     * Entry i maps MFNs [i*8 .. (i+1)*8 - 1] to node i.
     */
    memnode_shift = 3;
    for ( unsigned int i = 0; i < 64; i++ )
        memnodemap[i] = (nodeid_t)i;
#endif /* CONFIG_NUMA */
}

static void init_dummy_domains(void);
/**
 * Reset all page_alloc translation-unit globals that these tests observe.
 *
 * The test program includes xen/common/page_alloc.c directly, so its
 * file-scope variables become global variables of this translation unit.
 *
 * Each test must start from a clean page allocator state, with a clean heap,
 * clean availability counters, and empty offlined and broken lists.
 */
static void reset_page_alloc_state(const char *caller_func, int start_mfn)
{
    unsigned int zone;
    unsigned int order;

    printf("\n%s: start_mfn = %u\n", caller_func, start_mfn);

    /* Clear the test page table used for synthetic page_info objects. */
    memset(frame_table, 0, sizeof(frame_table));

    /* Clear the backing storage used by the imported allocator globals. */
    memset(test_heap_storage, 0, sizeof(test_heap_storage));
    memset(test_avail_storage, 0, sizeof(test_avail_storage));

    /* Clear the shim-owned singleton objects used by helper macros. */
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
        node_avail_pages[node] = 0;
        for ( zone = 0; zone < NR_ZONES; zone++ )
            for ( order = 0; order <= MAX_ORDER; order++ )
                INIT_PAGE_LIST_HEAD(&heap(node, zone, order));
    }

    total_avail_pages = 0;
    outstanding_claims = 0;
    /*
     * Configure the valid MFN range to cover only the test frame table so
     * that page_alloc.c touches only the intended test state and does not
     * access uninitialised memory or run out of bounds.
     *
     * This also sets the initial mfn_valid() range used by free_heap_pages()
     * to determine whether predecessor or successor pages exist for merging.
     * Unless those neighbours were initialised as free, the allocator should
     * forego merging and add the provided page as-is to the heap.
     */
    first_valid_mfn = start_mfn;
    max_page = sizeof(frame_table) / sizeof(frame_table[0]);
    assert(first_valid_mfn < max_page);

    init_dummy_domains();
}

static void init_dummy_domains(void)
{
    nodemask_t dom_node_affinity;
    struct domain *dom;
    int dom_id = 1;            /* Start domain IDs from 1 for clarity in logs */

    /* Provide a current vcpu/domain pair for code paths that inspect it. */
    test_current_vcpu.domain = &test_dummy_domain1;

    /* Provide the dummy domains for tests that need some domains */
    domain_list = &test_dummy_domain1;
    test_dummy_domain1.next_in_list = &test_dummy_domain2;

    nodes_clear(dom_node_affinity);
    node_set(node0, dom_node_affinity);
    node_set(node1, dom_node_affinity);

    for_each_domain ( dom )
    {
        dom->node_affinity = dom_node_affinity;
        dom->max_pages = MAX_PAGES;
        dom->domain_id = dom_id++;
    }
}

/* Initialize the page allocator tests */
static void __used init_page_alloc_tests(void)
{
    /* Define the function above as the testcase initialization function */
    setup_testcase_init_func(reset_page_alloc_state);
}

/**
 * Populate a page descriptor with the minimal state needed by
 * reserve_offlined_page().
 *
 * Tests build synthetic buddy trees by placing a small set of
 * page_info objects into allocator free lists. This helper keeps
 * that setup consistent across scenarios.
 *
 * Args:
 *  page (struct page_info *): Pointer to the page_info to initialise.
 *  order (unsigned int):      The order to store in the page_info order field.
 *  state (unsigned long):     State bits to store in the page's count_info
 *                             field, e.g. PGC_state_inuse for pages to be
 *                             added to the heap, or PGC_state_offlined for
 *                             pages to be added to the offlined list.
 */
static void init_test_page(struct page_info *page, unsigned int order,
                           unsigned long state)
{
    mfn_t mfn = page_to_mfn(page);

    if ( mfn < first_valid_mfn && mfn > 0 && mfn < max_page )
        first_valid_mfn = mfn;

    if ( mfn >= max_page && mfn < ARRAY_SIZE(frame_table) )
        max_page = mfn + 1;

    CHECK(mfn_valid(mfn), "mfn %lu valid: %lu-%lu", mfn, first_valid_mfn,
          max_page);

    memset(page, 0, sizeof(*page));

    /* Model the page as a free buddy head of the requested order. */
    page->v.free.order = order;

    /* Default to no tracked dirty subrange and no active scrubbing. */
    page->u.free.first_dirty = INVALID_DIRTY_IDX;
    page->u.free.scrub_state = BUDDY_NOT_SCRUBBING;

    /* Install the requested allocator state bits for this synthetic page. */
    page->count_info = state;
}

/**
 * Initialise the given pages as a buddy of the requested order,
 * with the first page as the buddy head and the rest as its subpages,
 * then add the initialised buddy to the heap.
 *
 * Test scenarios use this helper to populate the heap with buddies of the
 * expected order and state before exercising operations such as
 * reserve_offlined_page() and free_heap_pages(). It also helps ensure that
 * heap state matches the corresponding page_info state afterwards.
 *
 * The buddy is added with free_heap_pages(), which follows the allocator's
 * real heap-management path and keeps the heap structures consistent even
 * as allocator internals evolve.
 *
 * Args:
 *  pages (struct page_info *): Pointer to the first page_info in an array.
 *  order (unsigned int):       The order of the buddy to create.
 *  caller (const char *):      The name of the calling function for context.
 * Returns:
 *  The zone of the added buddy, for scenarios that need it for further
 *  operations or assertions.
 */
static zone_t __used page_list_add_buddy(struct page_info *pages,
                                         unsigned int order,
                                         const char *caller_file,
                                         const char *caller_func,
                                         int caller_line)
{
    size_t i, num_pages = 1U << order;
    bool verbose_asserts_save = testcase_assert_verbose_assertions;

    /* Avoid logging spinlocks and verbose assertions during initialization */
    testcase_assert_verbose_assertions = false;

    /*
     * Initialize the first page as the head of the buddy with the given order.
     * All pages are initialized as in-use as this is the API expected by
     * free_heap_pages() when it adds a buddy to the heap(). This model is
     * consistent with the way the boot allocator and online_page() handle
     * page initialization as well as the normal way for used pages to be freed.
     */
    init_test_page(&pages[0], order, PGC_state_inuse);

    /* Add the subpages of the buddy as order-0 buddies to the heap */
    for ( i = 1; i < num_pages; i++ )
        init_test_page(&pages[i], order0, PGC_state_inuse);

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
    printf("%s: Adding buddy of order %u at MFN %lu to the heap.\n",
           caller_func, order, page_to_mfn(&pages[0]));

    testcase_assert_verbose_assertions = false;

    free_heap_pages(&pages[0], order, false);

    /* Ensure that the buddy does not cross zones; buddies may not do so. */
    if ( page_to_zone(&pages[0]) != page_to_zone(&pages[num_pages - 1]) )
        testcase_assert(false, caller_file, caller_line, caller_func,
                        "Buddy of order %u at MFN %lu crosses zones: "
                        "start zone %u, end zone %u", order,
                        page_to_mfn(&pages[0]),
                        page_to_zone(&pages[0]),
                        page_to_zone(&pages[num_pages - 1]));

    testcase_assert_verbose_assertions = verbose_asserts_save;
    return page_to_zone(&pages[0]);
}

#define test_page_list_add_buddy(pages, order) \
        page_list_add_buddy(pages, order, __FILE__, __func__, __LINE__)

#endif /* TOOLS_TESTS_ALLOC_LIBTEST_PAGE_ALLOC_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
