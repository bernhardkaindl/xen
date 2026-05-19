/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test environment to compile xen/common/page_alloc.c into the test
 * environment and provide the necessary supporting structures and stubs
 * for it to function correctly.
 *
 * Also provide helper functions to set up test scenarios and check the
 * resulting state of the page allocator.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_PAGE_ALLOC_ENV_H
#define TOOLS_TESTS_NATIVE_PAGE_ALLOC_ENV_H

#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "native.h"
static const unsigned int node = 0;
static const unsigned int node0 = 0;
static const unsigned int node1 = 1;
static const unsigned int order0 = 0;
static const unsigned int order1 = 1;
static const unsigned int order2 = 2;
static const unsigned int order3 = 3;

/* Backing storage for the synthetic allocator state used by the tests. */
#ifndef PAGES_PER_ZONE
#define PAGES_PER_ZONE 8
#endif

#ifndef MAX_PAGES
#define MAX_PAGES (MAX_NUMNODES * NR_ZONES * PAGES_PER_ZONE)
#endif

/*
 * The synthetic frame table backs the page_info entries used by the tests.
 * It is indexed by MFN so helper code and the imported allocator can
 * translate directly between MFNs and page_info pointers.
 */
struct page_info test_frame_table[MAX_PAGES];
static heap_by_zone_and_order_t test_heap_storage[MAX_NUMNODES];
static unsigned long test_avail_storage[MAX_NUMNODES][NR_ZONES];
struct domain *domain_list;

static void init_numa_node_data(unsigned int start_mfn)
{
    (void)start_mfn;
#ifdef CONFIG_NUMA
    unsigned long node_spanned_pages = 16;

    for ( unsigned int i = 0; i < NR_CPUS; i++ )
        cpu_to_node[i] = i;

    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
        node_to_cpumask[i] = (1UL << i);

    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
    {
        node_data[i].node_start_pfn = start_mfn + (i * node_spanned_pages);
        node_data[i].node_present_pages = node_spanned_pages;
        node_data[i].node_spanned_pages = node_spanned_pages;
    }
    memnode_shift = fls(node_spanned_pages - 1);
    for ( unsigned int i = 0; i < 64; i++ )
        memnodemap[i] = (nodeid_t)i;
#endif
}

static void init_dummy_domains(void)
{
    nodemask_t dom_node_affinity;
    struct domain *dom;
    int dom_id = 1;

    nodes_clear(dom_node_affinity);
    node_set(node0, dom_node_affinity);
    node_set(node1, dom_node_affinity);
    test_current_vcpu.domain = &test_dummy_domain1;
    domain_list = &test_dummy_domain1;
    test_dummy_domain1.next_in_list = &test_dummy_domain2;

    for_each_domain ( dom )
    {
        dom->node_affinity = dom_node_affinity;
        dom->max_pages = MAX_PAGES;
        dom->domain_id = dom_id++;
        INIT_PAGE_LIST_HEAD(&dom->page_list);
    }
}

/* Internal correctness check: page_get_owner and page_set_owner must match. */
static void sanity_check_page_ownership_accessors(void)
{
    struct page_info *page = &frame_table[0];

    /* Ignore the always true warning caused by the static dummy domain */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Waddress"
    page_set_owner(page, &test_dummy_domain1);
    #pragma GCC diagnostic pop

    ASSERT(page_get_owner(page) == &test_dummy_domain1);
    page_set_owner(page, NULL); /* Clear the owner again */
}

static void reset_page_alloc_state(int start_mfn)
{
    unsigned int zone, order;

    /* Initialize frame table and heap structures */
    FRAMETABLE_VIRT_START = (unsigned long)test_frame_table;
    FRAMETABLE_VIRT_END = (unsigned long)(test_frame_table + MAX_PAGES);
    memset(test_frame_table, 0, sizeof(test_frame_table));
    memset(test_heap_storage, 0, sizeof(test_heap_storage));
    memset(test_avail_storage, 0, sizeof(test_avail_storage));
    memset(&test_dummy_domain1, 0, sizeof(test_dummy_domain1));
    memset(&test_dummy_domain2, 0, sizeof(test_dummy_domain2));
    memset(&test_current_vcpu, 0, sizeof(test_current_vcpu));
    system_state = SYS_STATE_active;
    INIT_PAGE_LIST_HEAD(&page_offlined_list);
    INIT_PAGE_LIST_HEAD(&page_broken_list);

    init_numa_node_data(start_mfn);
    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
    {
        _heap[node] = &test_heap_storage[node];
        avail[node] = test_avail_storage[node];
        node_avail_pages[node] = 0;
        claimed_pages[node] = 0;
        for ( zone = 0; zone < NR_ZONES; zone++ )
            for ( order = 0; order <= MAX_ORDER; order++ )
                INIT_PAGE_LIST_HEAD(&heap(node, zone, order));
    }
    total_avail_pages = 0;
    outstanding_claims = 0;
    first_valid_mfn = _mfn(start_mfn);
    max_page = sizeof(test_frame_table) / sizeof(test_frame_table[0]);
    ASSERT(mfn_x(first_valid_mfn) < max_page);
    init_dummy_domains();
    sanity_check_page_ownership_accessors();
    testcase_assert_verbose_assertions = true;
}

static void __used init_page_alloc_tests(void)
{
    setup_testcase_init_func(reset_page_alloc_state);
}

static void init_test_page(struct page_info *page, unsigned int order,
                           unsigned long state)
{
    mfn_t mfn = page_to_mfn(page);

    if ( mfn_x(mfn) < mfn_x(first_valid_mfn) && mfn_x(mfn) > 0 &&
         mfn_x(mfn) < max_page )
        first_valid_mfn = mfn;

    if ( mfn_x(mfn) >= max_page && mfn_x(mfn) < ARRAY_SIZE(test_frame_table) )
        max_page = mfn_x(mfn) + 1;

    memset(page, 0, sizeof(*page));
    page->v.free.order = order;
    page->u.free.first_dirty = INVALID_DIRTY_IDX;
    page->u.free.scrub_state = BUDDY_NOT_SCRUBBING;
    page->count_info = state;
}

/* Check whether the page is aligned to its order (is size-aligned) */
static bool page_aligned(struct page_info *pg)
{
    return IS_ALIGNED(mfn_x(page_to_mfn(pg)), 1UL << PFN_ORDER(pg));
}

static size_t __used page_list_add_buddy(struct page_info *pages,
                                         unsigned int order,
                                         const char *caller_file,
                                         const char *caller_func,
                                         int caller_line)
{
    size_t i, num_pages = 1U << order;
    bool verbose = testcase_assert_verbose_assertions;

    testcase_assert_verbose_assertions = false;
    init_test_page(&pages[0], order, PGC_state_inuse);
    for ( i = 1; i < num_pages; i++ )
        init_test_page(&pages[i], order0, PGC_state_inuse);
    free_heap_pages(&pages[0], order, false);

    if ( !page_aligned(&pages[0]))
        testcase_assert(false, caller_file, caller_line, caller_func,
                        "Buddy of order %u at MFN %lu is not size-aligned",
                        order, mfn_x(page_to_mfn(&pages[0])));
    if ( page_to_zone(&pages[0]) != page_to_zone(&pages[num_pages - 1]) )
        testcase_assert(false, caller_file, caller_line, caller_func,
                        "Buddy of order %u at MFN %lu crosses zones: "
                        "start zone %u, end zone %u", order,
                        mfn_x(page_to_mfn(&pages[0])),
                        page_to_zone(&pages[0]),
                        page_to_zone(&pages[num_pages - 1]));
    testcase_assert_verbose_assertions = verbose;
    return page_to_zone(&pages[0]);
}
#define test_page_list_add_buddy(pages, order) \
        page_list_add_buddy(pages, order, __FILE__, __func__, __LINE__)

#define test_page_list_add_node_buddy(node, start_mfn, order)             \
        page_list_add_buddy(frame_table + node_data[node].node_start_pfn + \
                            (start_mfn), order, __FILE__, __func__, __LINE__)

#define test_get_node_page(node, offset) \
        (frame_table + node_data[node].node_start_pfn + (offset))

/* Get the number of free pages and assert it is equal to total_avail_pages */
unsigned long get_free_pages(const char *file, int line, const char *func)
{
    bool verbose = testcase_assert_verbose_assertions;
    unsigned long free_pages = avail_heap_pages(MEMZONE_XEN, NR_ZONES - 1, -1);

    testcase_assert_verbose_assertions = false;
    testcase_assert(free_pages == total_avail_pages, file, line, func,
                    "Expect %lu avail_heap_pages() == total_avail_pages %lu",
                    free_pages, total_avail_pages);
    testcase_assert_verbose_assertions = verbose;
    return free_pages;
}
#define FREE_PAGES get_free_pages(__FILE__, __LINE__, __func__)
#define TOTAL_CLAIMS (outstanding_claims)

/* Stubs needed for the page allocator */

/* Stub for page_alloc.c's keyhandler registrations */
void __init register_keyhandler(unsigned char key, keyhandler_fn_t *fn,
                                const char *desc, bool diagnostic)
{
    (void)key;
    (void)fn;
    (void)desc;
    (void)diagnostic;
}

#endif
#endif
