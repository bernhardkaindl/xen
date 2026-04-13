/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test framework for testing the memory-allocation functionality
 * of xen/common/page_alloc.c, consisting of:
 *
 * 1. A header-only shim for page_alloc.c to provide the necessary
 *    definitions and helpers to allow the test framework to include
 *    the real page_alloc.c directly into its translation unit.
 *
 * 2. A set of mocks for the Xen types and functions used by page_alloc.c,
 *    sufficient to support the test scenarios in tools/tests/alloc.
 *
 *    This includes mocks for NUMA topology, designed to allow the test
 *    scenarios to manipulate the state of the allocator and verify its
 *    behavior in a way that is consistent with how page_alloc.c acts when
 *    used by the running Xen hypervisor, while being self-contained and
 *    suitable for unit and integration testing.
 *
 * 3. A tiny wrapper which includes the real page_alloc.c for testing.
 *
 *    It disables a few of the -Wextra warnings enabled by the test
 *    framework that are not yet fixed in page_alloc.c, such as some
 *    sign-compare warnings and unused parameter warnings in its code.
 *
 * 4. A library for NUMA heap initialisation, and asserting the heap status.
 *
 *    This library provides functions to prepare the state of the memory
 *    allocator for the test scenarios, such as:
 *
 *    a. Initializing the heap before each test case, creating NUMA nodes,
 *       and adding pages to the heap in specific states, such as free,
 *       allocated, marked to be offlined or already offlined.
 *
 *    b. Verifying the state of the heap and the page_info structures after
 *       test actions, such as checking that pages are allocated or freed
 *       as expected, that the state of the page_info structures is consistent
 *       with the expected state.
 *
 * 5. Test case lifecycle management, such as initializing the test context
 *    before each test case, printing the outcome of each test case,
 *    tracking the number of assertions, logging assertions with file
 *    and line information, and printing a summary report at the end.
 *
 * 6. A Makefile for discovering, compiling, running the test cases,
 *    and reporting results which test cases were run.
 *
 *    The Makefile is designed to allow running individual test cases
 *    or the entire test suite for all supported CPU architectures, if
 *    so desired.
 *
 *    It is also responsible for compiling the tests with address sanitizer
 *    (ASAN) enabled to catch memory errors in the page allocator code
 *    and the test code, especially when manipulating the state of the
 *    page_info structures inside the test scenarios.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
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
#include "page_alloc_shim.h"

/* Include the real page_alloc.c for testing */

#include "page_alloc-wrapper.h"

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
     * The valid MFN range for the test context is configured to cover only
     * the test frame table, so that any attempts by page_alloc.c to prevent
     * functions in page_alloc.c is only manipulating the intended test
     * state and not accessing uninitialized memory or going out of bounds.
     *
     * Set up the initial range of valid pages for mfn_valid() used by
     * free_heap_pages() as condition if there are successors/predecessors
     * to merge pages with. Unless successors/predecessors are initialized
     * to be free, it should forgoe merging and just add the provided page
     * as-is to the heap, but to prevent it looking up uninitialised memory,
     * we set the valid MFN range to cover the frame_table only.
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
 * page_info objects into allocator free lists. This helper
 * keeps that setup consistent across scenarios.
 *
 * Args:
 *  page (struct page_info *): Pointer to the page_info to be initialised.
 *  order (unsigned int):      The order to set in the page_info's order field
 *  state (unsigned long):     The state bits to set in the page's count_info
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
 * Initialize the given pages as a buddy of the requested order,
 * with the first page as the buddy head and the rest as subpages
 * of it, and add the intialised buddy to the heap.
 *
 * This helper is intended to be used by test scenarios to set up
 * the heap with buddies of the expected order and state for testing
 * operations that manipulate the heap, such as reserve_offlined_page()
 * and free_heap_pages(), and to ensure that the heap state is consistent
 * with the page_info state after those operations.
 *
 * The buddy is added to the heap using free_heap_pages() which
 * models the expected usage of the heap and ensures that the
 * heap structures are updated correctly according to the logic
 * of the allocator, which may change over time.
 *
 * For example, if the logic for merging buddies or tracking claims changes,
 * using free_heap_pages() ensures that the test setup will be correct even
 * after such changes, and that the test scenarios will be testing the real
 * behaviour of the allocator rather than an idealised version of it.
 *
 * Args:
 *  pages (struct page_info *): Pointer to the first page_info in an array.
 *  order (unsigned int):       The order of the buddy to be created.
 *  caller (const char *):      The name of the calling function for context.
 * Returns:
 *  The zone of the added buddy, which can be useful for test scenarios that
 *  need to know the zone of the buddy for further operations or assertions.
 */
static zone_t __used page_list_add_buddy(struct page_info *pages,
                                         unsigned int order, const char *caller)
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
    printf("%s: Adding buddy of order %u at MFN %lu to the heap.\n", caller,
           order, page_to_mfn(&pages[0]));

    testcase_assert_verbose_assertions = false;

    free_heap_pages(&pages[0], order, false);

    testcase_assert_verbose_assertions = verbose_asserts_save;
    return page_to_zone(&pages[0]);
}

#define test_page_list_add_buddy(pages, order) \
        page_list_add_buddy(pages, order, __func__)
