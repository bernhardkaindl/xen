/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test framework for xen/common/page_alloc.c.
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_ALLOC_LIBTEST_PAGE_ALLOC_H
#define TOOLS_TESTS_ALLOC_LIBTEST_PAGE_ALLOC_H

#pragma GCC diagnostic error "-Wextra"
#include "page-alloc-wrapper.h"
static const unsigned int node = 0;
static const unsigned int node0 = 0;
static const unsigned int node1 = 1;
static const unsigned int order0 = 0;
static const unsigned int order1 = 1;
static const unsigned int order2 = 2;

static heap_by_zone_and_order_t test_heap_storage[MAX_NUMNODES];
static unsigned long test_avail_storage[MAX_NUMNODES][NR_ZONES];
struct domain *domain_list;

static void init_numa_node_data(unsigned int start_mfn)
{
    (void)start_mfn;
#ifdef CONFIG_NUMA
    for ( unsigned int i = 0; i < NR_CPUS; i++ )
        cpu_to_node[i] = i;

    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
        node_to_cpumask[i] = (1UL << i);

    for ( unsigned int i = 0; i < MAX_NUMNODES; i++ )
    {
        node_data[i].node_start_pfn = start_mfn + (i * 8);
        node_data[i].node_present_pages = 8UL;
        node_data[i].node_spanned_pages = 8UL;
    }
    memnode_shift = 3;
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
    }
}

static void reset_page_alloc_state(int start_mfn)
{
    unsigned int zone;
    unsigned int order;

    memset(frame_table, 0, sizeof(frame_table));
    memset(test_heap_storage, 0, sizeof(test_heap_storage));
    memset(test_avail_storage, 0, sizeof(test_avail_storage));
    memset(&test_dummy_domain1, 0, sizeof(test_dummy_domain1));
    memset(&test_dummy_domain2, 0, sizeof(test_dummy_domain2));
    memset(&test_current_vcpu, 0, sizeof(test_current_vcpu));
    INIT_PAGE_LIST_HEAD(&page_offlined_list);
    INIT_PAGE_LIST_HEAD(&page_broken_list);
    INIT_PAGE_LIST_HEAD(&test_page_list);

    init_numa_node_data(start_mfn);
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
    first_valid_mfn = start_mfn;
    max_page = sizeof(frame_table) / sizeof(frame_table[0]);
    assert(first_valid_mfn < max_page);
    init_dummy_domains();
}

static void __used init_page_alloc_tests(void)
{
    setup_testcase_init_func(reset_page_alloc_state);
}

static void init_test_page(struct page_info *page, unsigned int order,
                           unsigned long state)
{
    mfn_t mfn = page_to_mfn(page);

    if ( mfn < first_valid_mfn && mfn > 0 && mfn < max_page )
        first_valid_mfn = mfn;

    if ( mfn >= max_page && mfn < ARRAY_SIZE(frame_table) )
        max_page = mfn + 1;

    memset(page, 0, sizeof(*page));
    page->v.free.order = order;
    page->u.free.first_dirty = INVALID_DIRTY_IDX;
    page->u.free.scrub_state = BUDDY_NOT_SCRUBBING;
    page->count_info = state;
}

static size_t __used page_list_add_buddy(struct page_info *pages,
                                         unsigned int order,
                                         const char *caller_file,
                                         const char *caller_func,
                                         int caller_line)
{
    size_t i, num_pages = 1U << order;

    init_test_page(&pages[0], order, PGC_state_inuse);
    for ( i = 1; i < num_pages; i++ )
        init_test_page(&pages[i], order0, PGC_state_inuse);
    free_heap_pages(&pages[0], order, false);

    if ( page_to_zone(&pages[0]) != page_to_zone(&pages[num_pages - 1]) )
        testcase_assert(false, caller_file, caller_line, caller_func,
                        "Buddy of order %u at MFN %lu crosses zones: "
                        "start zone %u, end zone %u", order,
                        page_to_mfn(&pages[0]),
                        page_to_zone(&pages[0]),
                        page_to_zone(&pages[num_pages - 1]));
    return page_to_zone(&pages[0]);
}
#define test_page_list_add_buddy(pages, order) \
        page_list_add_buddy(pages, order, __FILE__, __func__, __LINE__)
#endif
