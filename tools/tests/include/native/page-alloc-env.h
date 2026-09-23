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
#ifdef CONFIG_NUMA
    static typeof(*memnodemap) _memnodemap[MAX_NUMNODES];
    unsigned long node_spanned_pages = 16;

    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
    {
        node_data[i].node_start_pfn = start_mfn + (i * node_spanned_pages);
        node_data[i].node_spanned_pages = node_spanned_pages;
    }
    memnode_shift = fls(node_spanned_pages - 1);
    memnodemap = _memnodemap;
    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
        memnodemap[i] = (nodeid_t)i;
    memnodemapsize = sizeof(_memnodemap) / sizeof(*memnodemap);
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

static void reset_page_alloc_state(int start_mfn)
{
    unsigned int zone, order;

    if ( test_dummy_domain1.claims )
        xvfree(test_dummy_domain1.claims);
    if ( test_dummy_domain2.claims )
        xvfree(test_dummy_domain2.claims);

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

    init_numa_node_data(start_mfn);
    nodes_setall(node_online_map);
    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
    {
        _heap[node] = &test_heap_storage[node];
        avail[node] = test_avail_storage[node];
        node_avail_pages[node] = 0;
        node_claimed_pages[node] = 0;
        for ( zone = 0; zone < NR_ZONES; zone++ )
            for ( order = 0; order <= MAX_ORDER; order++ )
                INIT_PAGE_LIST_HEAD(&heap(node, zone, order));
    }
    total_avail_pages = 0;
    outstanding_claims = 0;
    init_dummy_domains();
}

static void run_test(void (*test_func)(int), int start_mfn)
{
    reset_page_alloc_state(start_mfn);
    test_func(start_mfn);
    test_functions_run++;
}

static size_t __used page_list_add_buddy(struct page_info *pages,
                                         unsigned int order,
                                         const char *caller_file,
                                         const char *caller_func,
                                         int caller_line)
{
    free_heap_pages(&pages[0], order, false);
    return page_to_zone(&pages[0]);
}

#define test_page_list_add_buddy(pages, order) \
        page_list_add_buddy(pages, order, __FILE__, __func__, __LINE__)

#define test_page_list_add_node_buddy(node, start_mfn, order)             \
        page_list_add_buddy(frame_table + node_data[node].node_start_pfn + \
                            (start_mfn), order, __FILE__, __func__, __LINE__)

#define test_get_node_page(node, offset) \
        (frame_table + node_data[node].node_start_pfn + (offset))

/* Stub for page_alloc.c's keyhandler registrations */
void __init register_keyhandler(unsigned char key, keyhandler_fn_t *fn,
                                const char *desc, bool diagnostic)
{
}

#endif
#endif
