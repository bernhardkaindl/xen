/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration tests for redeeming NUMA memory claim set as implemented
 * in xen/common/page_alloc.c's redeem_claims_for_allocation() and
 * related functions.
 *
 * redeem_claims_for_allocation() is exercised indirectly through
 * alloc_domheap_pages() which is the primary interface for allocating
 * pages from a domain's heap.
 *
 * By means of domain_install_claim_set(), a claim set with global and
 * per-NUMA-node claims is installed for a dummy domain, and then
 * allocations with NUMA node affinity are performed to verify that the
 * appropriate claims are redeemed (same-node first, global fallback next,
 * then other nodes to not exceed page limits). The test also verifies that
 * aggregate counters are updated correctly after each allocation.
 *
 * The test verifies that when a domain has a claim set installed with
 * global and per-NUMA-node claims, allocations that specify NUMA node
 * affinity will redeem the appropriate claims (same-node first, global
 * fallback claim next, then other nodes to not exceed page limits).
 * It also verifies that the aggregate claim counters are updated
 * correctly after each allocation.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#define CONFIG_NUMA   1
#define CONFIG_SYSCTL 1
#include "libtest-page-alloc.h"

/*
 * Test redeeming NUMA memory claims in exchange for allocations,
 * and the redeemed claims are correctly reflected in the domain's
 * claim state and the aggregate claim counters.
 */
static void test_claims_numa_install(int start_mfn)
{
    unsigned long avail_pages_zone;
    int zone, ret;
    struct page_info *pages = test_pages + start_mfn, *allocated;

    /*
     * PREPARE
     */

    /*
     * Node 1's pages start at the pfn set by init_numa_node_data():
     * node_data[node1].node_start_pfn = start_mfn + 8 (8 MFNs per node with
     * memnode_shift=3). The order-2 buddy (4 pages) placed there satisfies
     * the 2-page node1 claim and provides enough total pages for the
     * 2 global + 2 node0 + 2 node1 = 6-page claim set (2 + 4 = 6 total).
     */
    struct page_info *pages_node1 =
        test_pages + node_data[node1].node_start_pfn;

    /* Create an order-1 buddy (2 pages) for node 0 and add it to the heap. */
    zone = test_page_list_add_buddy(pages, order1);

    /* Verify the initial state of node 0's heap. */
    ASSERT_LIST_EQUAL(&heap(node0, zone, order1), pages);
    ASSERT(page_list_empty(&heap(node0, zone, order0)));
    CHECK_BUDDY(pages, "Order-1 buddy on node 0 prepared");

    /* Create an order-2 buddy (4 pages) for node 1 and add it to the heap. */
    test_page_list_add_buddy(pages_node1, order2);
    CHECK_BUDDY(pages_node1, "Order-2 buddy on node 1 prepared");

    /*
     * ACT 1
     */

    /* Install a claim set with global + per-NUMA-node claims. */
    memory_claim_t claim_set[] = {
        {.target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL, .pages = 2},
        {.target = node0,                          .pages = 2},
        {.target = node1,                          .pages = 2},
    };
    ret = domain_install_claim_set(dom1, ARRAY_SIZE(claim_set), claim_set);
    CHECK(ret == 0, "domain_install_claim_set should succeed: %d", ret);

    /* Assert dom1's claims */
    CHECK(TOTAL_CLAIMS == 6, "Expect 6 total claims after installation");
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 2,
          "Expect dom1 having 2 global claims after installation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 2,
          "Expect dom1 having 2 claims for node0 after installation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 2,
          "Expect dom1 having 2 claims for node1 after installation");

    /* Allocate an order-0 page from node 0 for the dummy domain. */
    allocated = alloc_domheap_pages(dom1, order0, MEMF_node(node0));
    CHECK(allocated != NULL, "alloc_domheap_pages should succeed");

    /*
     * ASSERT 1
     *
     * The order-0 allocation from node 0 splits the node 0 order-1 buddy:
     * - The lower half (pages[0]) stays on node 0's order-0 heap.
     * - The upper half (pages[1]) is returned as the allocated page.
     * One node 0 claim is consumed by the allocation.
     */
    CHECK_BUDDY(pages, "Buddy after order-0 allocation");
    /* Verify the state of node 0's heap after allocation. */
    ASSERT(page_list_empty(&heap(node0, zone, order2)));
    ASSERT(page_list_empty(&heap(node0, zone, order1)));
    /* The lower half (pages[0]) remains as the sole order-0 buddy on node 0. */
    ASSERT_LIST_EQUAL(&heap(node0, zone, order0), pages);

    avail_pages_zone = avail_heap_pages(zone, zone, node0);
    CHECK(avail_pages_zone == 1, "Expect one page in node0 after allocation");

    /* Verify the state of the aggregate counters after allocation. */
    CHECK(TOTAL_CLAIMS == 5, "Expect 5 total claims left after allocation");
    CHECK(FREE_PAGES == 5, "Expect 5 free pages left after allocation");

    /* Assert dom1's claims after the allocation from node0 */
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 2,
          "Expect dom1 still having 2 global claims after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 1,
          "Expect dom1 having 1 claim for node0 after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 2,
          "Expect dom1 still having 2 claims for node1 after allocation");

    /* Allocate an order-0 page from node 1 for the dummy domain. */
    allocated = alloc_domheap_pages(dom1, order0, MEMF_node(node1));
    CHECK(allocated != NULL, "order-0 alloc from node1");

    /* Assert dom1's claims after the allocation from node1 */
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 2,
          "Expect dom1 still having 2 global claims after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 1,
          "Expect dom1 having 1 claim for node0 after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 1,
          "Expect dom1 having 1 claim for node1 after allocation");

    /* Allocate an order-1 page from node 1 for the dummy domain. */
    allocated = alloc_domheap_pages(dom1, order1, MEMF_node(node1));
    CHECK(allocated != NULL, "order-1 alloc from node1");

    /* Assert dom1's claims after the allocation from node1 */
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 1,
          "Expect dom1 having redeemed one global claim after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 1,
          "Expect dom1 having 1 claim for node0 after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 0,
          "Expect dom1 having 0 claims for node1 after allocation");

    /* Allocate an order-0 page from node 1 for the dummy domain. */
    allocated = alloc_domheap_pages(dom1, order0, MEMF_node(node1));
    CHECK(allocated != NULL, "order-0 alloc from node1");

    /* Assert dom1's claims after the allocation from node1 */
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 0,
          "Expect dom1 having redeemed one global claim after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 1,
          "Expect dom1 having 1 claim for node0 after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 0,
          "Expect dom1 having 0 claims for node1 after allocation");

    /* Allocate an order-0 page from node 1 for the dummy domain. */
    allocated = alloc_domheap_pages(dom1, order0, MEMF_node(node1));
    CHECK(allocated != NULL, "order-0 alloc from node1");

    /* Assert dom1's claims after the allocation from node1 */
    CHECK(DOM_GLOBAL_CLAIMS(dom1) == 0,
          "Expect dom1 having redeemed one global claim after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node0) == 0,
          "Expect dom1 having 0 claims for node0 after allocation");
    CHECK(DOM_NODE_CLAIMS(dom1, node1) == 0,
          "Expect dom1 having 0 claims for node1 after allocation");
}

int main(int argc, char *argv[])
{
    const char *topic = "Test legacy claims with allocation from the heap";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();
    /*
     * Use test_set_global_claims() which is a wrapper around
     * domain_install_claim_set() to check ensure consistent
     * behavior with domain_set_outstanding_pages().
     */
    RUN_TESTCASE(CNI0, test_claims_numa_install, 0);

    testcase_print_summary(program_name);
    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
