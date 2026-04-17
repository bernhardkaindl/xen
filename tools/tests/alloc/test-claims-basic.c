/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit tests for memory claims in xen/common/page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#define CONFIG_NUMA   1
#define CONFIG_SYSCTL 1
#include "libtest-page-alloc.h"

/*
 * Wrapper for domain_install_claim_set() with the function signature as
 * domain_set_outstanding_pages() to test both domain_install_claim_set()
 * and domain_set_outstanding_pages() by using a function pointer for
 * setting claims to check feature parity and consistent behavior.
 */

int test_set_global_claims(struct domain *d, unsigned long pages)
{
    memory_claim_t claim_set[] = {
        {.target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL, .pages = pages},
    };
    return domain_install_claim_set(d, ARRAY_SIZE(claim_set), claim_set);
}
typedef int (*set_global_claims)(struct domain *d, unsigned long pages);

/*
 * Function pointer to test both domain_install_claim_set() and
 * domain_set_outstanding_pages() interchangeably in the test
 * scenarios for feature parity and consistent behaviour.
 */
set_global_claims install_global_claims = test_set_global_claims;

/*
 * Test that memory claims are redeemed correctly during allocations.
 */
static void test_alloc_domheap_redeems_claims(int start_mfn)
{
    unsigned long avail_pages_zone;
    int zone, ret;
    struct page_info *pages = test_pages + start_mfn, *allocated;

    /*
     * PREPARE
     */

    /* Create a buddy of order 2 (4 pages) and add it to the heap. */
    zone = test_page_list_add_buddy(pages, order2);

    /* Verify the initial state of the heap */
    ASSERT_LIST_EQUAL(&heap(node, zone, order2), pages);
    ASSERT(page_list_empty(&heap(node, zone, order1)));
    ASSERT(page_list_empty(&heap(node, zone, order0)));
    CHECK_BUDDY(pages, "Order-2 buddy prepared on the heap");

    /*
     * ACT 1
     */

    /* Claim 3 out of the 4 pages for the dummy domain */
    ret = test_set_global_claims(dom1, 3);
    ASSERT(ret == 0);

    /* Allocate an order-1 page for the dummy domain */
    allocated = alloc_domheap_pages(dom1, order1, 0);
    CHECK(allocated == &pages[2], "Expect allocation start at 3rd page");

    /*
     * ASSERT 1
     *
     * The allocation is expected to split the order-2 buddy and allocate
     * an order-1 chunk from it, leaving the remaining order-1 chunk as a free
     * available pages, and the claim should have been consumed accordingly.
     */

    /* Verify the state of the heap after allocation */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    ASSERT(page_list_empty(&heap(node, zone, order0)));
    /* The remaining order-1 chunk should be the first page */
    ASSERT_LIST_EQUAL(&heap(node, zone, order1), pages);
    CHECK_BUDDY(pages, "Buddy after order-1 allocation");

    /* Verify the state of the aggregate counters */
    CHECK(TOTAL_CLAIMS == 1, "Expect 1 claims left after allocation");
    CHECK(FREE_PAGES == 2, "Expect 2 available after allocation");
    CHECK(avail_heap_pages(zone, zone, node) == 2, "Expect 2 in zone");

    /*
     * ACT 2
     */

    /* Allocate one of the two remaining order-0 pages for the dummy domain */
    allocated = alloc_domheap_pages(dom1, order0, 0);
    CHECK(allocated == &pages[1], "alloc_domheap_pages returned the 2nd page");

    /*
     * ASSERT 2
     *
     * The allocation is expected to split the remaining order-1
     * buddy and allocate an order-0 page from it, leaving the
     * remaining order-0 page as a free available page, and the
     * claim should have been consumed accordingly.
     */

    /* Verify the state of the heap after allocation */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    ASSERT(page_list_empty(&heap(node, zone, order1)));
    /* The remaining order-0 page should be the only free page we've left */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages);

    /* Verify the state of the aggregate counters */
    CHECK(TOTAL_CLAIMS == 0, "Expect all claims consumed after allocation");
    CHECK(FREE_PAGES == 1, "Expect one free page after allocation");

    avail_pages_zone = avail_heap_pages(zone, zone, node);
    CHECK(avail_pages_zone == 1, "Expect one page in zone after allocation");

    /*
     * PREPARE 3
     */

    /* Claim all free memory from another domain to block allocations */
    ret = test_set_global_claims(dom2, FREE_PAGES);
    ASSERT(ret == 0);

    /*
     * ACT 3
     */

    /* Claim more than dom1 already has fails with ENOMEM (claimed by dom2) */
    ret = test_set_global_claims(dom1, domain_tot_pages(dom1) + 1);
    CHECK(ret == -ENOMEM, "dom 1 claim +1 fails due to insufficient pages");

    /* Claim more than dom1's d->max_pages fails with EINVAL */
    ret = test_set_global_claims(dom1, dom1->max_pages + 1);
    CHECK(ret == -EINVAL, "dom 1 claim fails due to exceeding max_pages");

    /* Attempt to allocate an order-0 page with a foreign claim present */
    allocated = alloc_domheap_pages(dom1, order0, 0);
    CHECK(allocated == NULL, "dom 1 alloc fails b/c domain 2's claim");

    /*
     * ASSERT 3
     */

    /* Verify the state of the heap after failed allocation (no changes) */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    ASSERT(page_list_empty(&heap(node, zone, order1)));
    /* Due to the foreign claim, the remaining page should still be free */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages);

    /* Verify the state of the aggregate counters (no changes expected) */
    CHECK(TOTAL_CLAIMS == 1, "Expect domain 2's claim to be still present");
    CHECK(FREE_PAGES == 1, "Expect one free page after failed alloc");

    avail_pages_zone = avail_heap_pages(zone, zone, node);
    CHECK(avail_pages_zone == 1, "Expect one page in zone after allocation");
}

/*
 * Test that memory claims are consumed correctly during allocations.
 */
static void test_cancel_claims(int start_mfn)
{
    struct page_info *page = test_pages + start_mfn;
    unsigned long claims;

    /* Create a buddy of order 2 (4 pages) and add it to the heap. */
    test_page_list_add_buddy(page, order2);
    claims = FREE_PAGES / 2;
    /* Claim half of the free pages for dom1 */
    ASSERT(test_set_global_claims(dom1, claims) == 0);
    ASSERT(TOTAL_CLAIMS == claims);

    /*
     * Act: Cancel the claims for the dummy domain and verify that the
     * claim counts are updated and the free pages are available again.
     */

    /* Act + Assert 2: Claim all free pages for dom2, should fail */
    ASSERT(test_set_global_claims(dom2, FREE_PAGES) == -ENOMEM);
    ASSERT(TOTAL_CLAIMS == claims);

    /* Act + Assert 1: Cancel all claims for dom1 */
    ASSERT(test_set_global_claims(dom1, 0) == 0);
    ASSERT(TOTAL_CLAIMS == 0);

    /* Act + Assert 2: Claim all free pages for dom2, should work */
    ASSERT(test_set_global_claims(dom2, FREE_PAGES) == 0);
    ASSERT(TOTAL_CLAIMS == FREE_PAGES);
}

int main(int argc, char *argv[])
{
    const char *topic = "Test legacy claims with allocation from the heap";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    /* Use domain_set_outstanding_pages() for staking claims */
    install_global_claims = domain_set_outstanding_pages;
    RUN_TESTCASE(ADCL, test_alloc_domheap_redeems_claims, 4);

    /*
     * Use test_set_global_claims() which is a wrapper around
     * domain_install_claim_set() to check ensure consistent
     * behavior with domain_set_outstanding_pages().
     */
    install_global_claims = test_set_global_claims;
    RUN_TESTCASE(ADCG, test_alloc_domheap_redeems_claims, 4);
    RUN_TESTCASE(TCCL, test_cancel_claims, 4);

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
