/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration tests for NUMA-aware memory claims.
 *
 * The install test verifies that when a domain has a claim set installed
 * with host-wide and per-NUMA-node claims, allocations that specify NUMA
 * node affinity will redeem the appropriate claims (same-node first,
 * host-wide fallback claim next, then other nodes, to not exceed page
 * limits). It also verifies that the aggregate claim counters are updated
 * correctly after each allocation.
 *
 * The get test verifies that callers can query the required number of
 * claim records by passing a count of 0 and a NULL claim set buffer.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifdef __x86_64__
#define CONFIG_NUMA 1 /* Enable NUMA support in the test environment. */
#define TEST_ENABLE_XC_DOMAIN_C /* Enable xc_domain.c wrapper */
#include "harness/native.h"

typedef int (*set_numa_claims)
    (struct domain *d, claim_set_t *request);
set_numa_claims install_numa_claims;

/*
 * Test redeeming NUMA memory claims in exchange for allocations,
 * where the redeemed claims are correctly reflected in the domain's
 * claim state and the aggregate claim counters.
 */
static void test_claims_numa_install(int start_mfn)
{
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Install a claim set with host-wide + per-NUMA-node claims. */
    xen_domctl_memory_claim_t claim_set[3] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    claim_set_t request = {
        .nr_entries = ARRAY_SIZE(claim_set),
        .claim = claim_set,
    };
    int install = install_numa_claims(dom1, &request);

    ASSERT(install == 0);
    CHECK(TOTAL_CLAIMS == 6, "Expect 6 total claims after installation");
    CLAIMS(dom1, claim_set);

    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node0)));
    CHECK(TOTAL_CLAIMS == 5, "Expect 5 total claims left after allocation");
    CHECK(FREE_PAGES == 7, "Expect 7 free pages left after allocation");
    ASSERT(claim_set[1].target == node0);
    claim_set[1].pages--; /* Expect the allocation redeemed from node 0 */
    CLAIMS(dom1, claim_set);

    /* An allocation on node 1 redeems a claim from node 1 */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    CHECK(TOTAL_CLAIMS == 4, "Expect 4 total claims left after allocation");
    CHECK(FREE_PAGES == 6, "Expect 6 free pages left after allocation");
    ASSERT(claim_set[2].target == node1);
    claim_set[2].pages--; /* Expect the allocation redeemed from node 1 */
    CLAIMS(dom1, claim_set);

    /* An allocation on node 1 redeems the last claim from node 1 */
    ASSERT(alloc_domheap_pages(dom1, order1, MEMF_node(node1)));
    CHECK(TOTAL_CLAIMS == 2, "Expect 2 total claims left after allocation");
    CHECK(FREE_PAGES == 4, "Expect 4 free pages left after allocation");
    xen_domctl_memory_claim_t claim_set2[2] = {
        claim_set[0], /* The Host-wide claim should still be present. */
        claim_set[1], /* Claim from node 0 should still be present. */
        /* The claim from node 1 is consumed, not part of the claim set. */
    };
    claim_set2[0].pages--; /* The 2nd page is redeemed from host-wide claim */
    CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to the host-wide claim */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    CHECK(TOTAL_CLAIMS == 1, "Expect 1 total claims left after allocation");
    CHECK(FREE_PAGES == 3, "Expect 3 free pages left after allocation");
    claim_set2[0].pages--; /* The 2nd page is redeemed from host-wide claim */
    CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to node 0 */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    CHECK(TOTAL_CLAIMS == 0, "Expect 0 total claims left after allocation");
    CHECK(FREE_PAGES == 2, "Expect 2 free pages left after allocation");
    CLAIMS(dom1, /* All claims should be consumed */
           ((xen_domctl_memory_claim_t[]){
                { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 0 },
            }));
}

/* Test getting the current claim set for a domain. */
static void test_claims_numa_get(int start_mfn)
{
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Install a claim set with host-wide + per-NUMA-node claims. */
    xen_domctl_memory_claim_t claim_set[3] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    claim_set_t request = {
        .nr_entries = ARRAY_SIZE(claim_set),
        .claim = claim_set,
    };
    int install = install_numa_claims(dom1, &request);

    ASSERT(install == 0);

    /*
     * Assert that the direct call can get the number of claim records by
     * passing a count of 0 and NULL for the claim set buffer.
     */
    claim_set_t claims = {0};
    uint32_t expected_records = ARRAY_SIZE(claim_set);

    ASSERT(domain_get_claim_entries(dom1, &claims) == -ERANGE);
    ASSERT(claims.nr_entries == expected_records);

    /*
     * Assert that the libxc wrapper can get the number of claim records for
     * a domain by passing a count of 0 and NULL for the claim set buffer.
     */
    uint32_t records = 0;
    ASSERT(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id,
                                       &records, NULL) == -ERANGE);
    ASSERT(records == expected_records);

    /* Assert the libxc wrapper returning the expected claim set contents */
    CLAIMS(dom1,
           ((xen_domctl_memory_claim_t[]){
                { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
                { .target = node0, .pages = 2 },
                { .target = node1, .pages = 2 }
            }));
}

int main(int argc, char *argv[])
{
    const char *topic = "Test NUMA-aware claims with allocation from the heap";

    if ( !parse_args(argc, argv, topic) )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    /* Run test cases with different NUMA claim installation methods */

    /* Run the test with a direct call to domain_set_claim_entries() */
    install_numa_claims = domain_set_claim_entries;
    RUN_TESTCASE("CNIS", test_claims_numa_install, 4);

    /* Run the test for getting the current claim set for a domain */
    install_numa_claims = domain_set_claim_entries;
    RUN_TESTCASE("CNGS", test_claims_numa_get, 4);

    return test_complete();
}
#else
#include <stdio.h>
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("This test requires NUMA, which is only available on x86_64.\n");
    return 0;
}
#endif
