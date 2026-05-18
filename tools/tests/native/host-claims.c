/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test basic host-wide (not node-affine) functionality of memory claims,
 * including installing and redeeming claims, and that claims are
 * respected by allocations and protected against other allocations.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#define TEST_WRAP_TOOLS_LIBS_CTRL_XC_DOMAIN_C /* Enable xc_domain.c wrapper */

#include "harness/native.h"

typedef int (*set_global_claims)(struct domain *d, unsigned long pages);
set_global_claims install_host_claims;

/* Install a host-wide claim using the legacy xc_domain_claim_pages() call */
int install_host_claims_legacy(struct domain *d, unsigned long pages)
{
    if (pages == 0)
        return xc_domain_claim_pages(xch, d->domain_id, 0);

    /* The legacy call need resetting claims before claims can be set again */
    xc_domain_claim_pages(xch, d->domain_id, 0);

    /* The argument of the legacy call includes the domain's existing pages */
    pages += domain_tot_pages(d);

    return xc_domain_claim_pages(xch, d->domain_id, pages);
}

/* Install a host-wide claim set using the xc_domain.c hypercall API */
int xc_domain_claim_memory_host(struct domain *d, unsigned long pages)
{
    xen_memory_claim_t claim_set[] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = pages },
    };
    uint32_t nr_entries = ARRAY_SIZE(claim_set);

    return xc_domain_claim_memory(xch, d->domain_id,
        XEN_DOMCTL_CLAIM_MEMORY_SET, &nr_entries, claim_set);
}

static void test_alloc_domheap_redeems_claims(int start_mfn)
{
    int ret;
    struct page_info *pages = frame_table + start_mfn, *pg;

    test_page_list_add_buddy(pages, order2);
    ASSERT(!install_host_claims(dom1, 3));
    ASSERT(alloc_domheap_pages(dom1, order1, 0) == pages + 2);
    ASSERT(alloc_domheap_pages(dom1, order0, 0) == pages + 1);
    CHECK(TOTAL_CLAIMS == 0, "Expect all claims consumed after allocations");
    CHECK(FREE_PAGES == 1, "Expect one free page after allocations");

    ASSERT(!install_host_claims(dom2, FREE_PAGES));

    /* Claim more than dom1 already has fails with ENOMEM (claimed by dom2) */
    ret = install_host_claims(dom1, domain_tot_pages(dom1) + 1);
    CHECK(ret == -ENOMEM, "dom 1 claim +1 fails due to insufficient pages");

    /* Claim more than dom1's d->max_pages fails with EINVAL */
    ret = install_host_claims(dom1, dom1->max_pages + 1);
    CHECK(ret == -EINVAL, "dom 1 claim fails due to exceeding max_pages");

    /* Attempt to allocate an order-0 page with a foreign claim present */
    pg = alloc_domheap_pages(dom1, order0, 0);
    CHECK(pg == NULL, "dom 1 allocation fails because of domain 2's claim");
    CHECK(TOTAL_CLAIMS == 1, "Expect domain 2's claim to be still present");
    CHECK(FREE_PAGES == 1, "Expect one free page after failed alloc");
}

/*
 * Test that memory claims can be cancelled by setting the claim count to 0,
 * and that cancelled claims are freed up for other domains to claim.
 *
 * This is important for domain_kill() to be able to cancel claims of a dying
 * domain and free up the pages for other domains to claim and free, otherwise
 * the host might run out of free pages due to claims that are not released.
 *
 * - Test that after claiming pages for a domain, allocations redeem a portion
 *   of those claims.
 *
 * - Test that other domains cannot claim more pages than the unclaimed free
 *   pages, and that cancelled claims are no longer present after cancellation.
 *
 * - Test that after cancelling claims for a domain, other domains can claim
 *   and allocate all remaining free pages.
 */
static void test_claim_alloc_cancel(int start_mfn)
{
    struct page_info *expected, *page = frame_table + start_mfn;
    unsigned long heap_pages, claims;
    unsigned int alloc_order;

    /* Create a buddy of order 2 (4 pages) and add it to the heap. */
    test_page_list_add_buddy(page, order3);
    heap_pages = FREE_PAGES;
    claims = heap_pages / 2;

    /* Claim half of the free pages for domain 1 */
    ASSERT(install_host_claims(dom1, claims) == 0);
    ASSERT(TOTAL_CLAIMS == claims);

    /* Allocate an order 1 page for domain 1 */
    alloc_order = order1;
    /* Expect the highest available page to be allocated */
    expected = page + FREE_PAGES - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom1, alloc_order, 0) == expected);
    ASSERT(TOTAL_CLAIMS == (claims -= 1UL << alloc_order, claims));

    /* Allocate an order 0 page for domain 1 */
    alloc_order = order0;
    /* Expect the highest available page to be allocated */
    expected = page + FREE_PAGES - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom1, alloc_order, 0) == expected);
    ASSERT(TOTAL_CLAIMS == (claims -= 1UL << alloc_order, claims));

    /* Claiming more than unclaimed for domain 2 should fail */
    ASSERT(install_host_claims(dom2, heap_pages - claims + 1) == -ENOMEM);
    /* Claiming all free pages for domain 2 should fail (dom1 has a claim) */
    ASSERT(install_host_claims(dom2, FREE_PAGES) == -ENOMEM);
    ASSERT(TOTAL_CLAIMS == claims);

    /*
     * Cancelling claims needs to always work, the checks in place for
     * installing claims should not prevent cancelling claims, which is
     * important for domain_kill() to be able to cancel claims of a dying
     * domain regardless of the state of the domain's configuration.
     *
     * An important check that cancelling claims needs to bypass is the
     * max_pages check, as a domain's max_pages can be set to a low value
     * due to a toolstack process (Xapi's "squeezed" squeezing the domain
     * can set its max_pages to a lower value than domain_tot_pages() by
     * invoking do_domctl(XEN_DOMCTL_max_mem).
     *
     * This should not prevent the claims from being cancelled as required.
     */
    dom1->max_pages = domain_tot_pages(dom1) - 1;

    /* Cancel all remaining claims for domain 1 */
    ASSERT(install_host_claims(dom1, 0) == 0);
    ASSERT(TOTAL_CLAIMS == 0);

    /* Claim all free pages for domain 2, should work */
    claims = FREE_PAGES;
    ASSERT(install_host_claims(dom2, claims) == 0);
    ASSERT(TOTAL_CLAIMS == claims);

    /* Claiming for domain 1 should fail with EINVAL due to max_pages = 0 */
    ASSERT(install_host_claims(dom1, 1) == -EINVAL);

    /* With d->max_pages > domain_tot_pages(), dom1 claims fails with -ENOMEM */
    dom1->max_pages = heap_pages;
    ASSERT(install_host_claims(dom1, 1) == -ENOMEM);

    /* Attempting to allocate a page for domain 1 should likewise fail now */
    ASSERT(alloc_domheap_pages(dom1, order0, 0) == NULL);

    /* Allocating a page for domain 2 still work as it has the claims */
    alloc_order = order0;
    /* Expect the highest available page to be allocated */
    expected = page + FREE_PAGES - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom2, alloc_order, 0) == expected);
    ASSERT(TOTAL_CLAIMS == (claims -= 1UL << alloc_order, claims));

    /* Even allocating the remaining order 2 buddy for domain 2 works */
    alloc_order = order2;
    /* Expect the highest available page to be allocated */
    expected = page + FREE_PAGES - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom2, alloc_order, 0) == expected);
    ASSERT(TOTAL_CLAIMS == (claims -= 1UL << alloc_order, claims));
}

/* Test offlining free pages outside and inside the claimed pages pool */
static void test_offlining_host_claims(int start_mfn)
{
    struct page_info *pages = frame_table + start_mfn;
    unsigned long heap_size, claims;

    test_page_list_add_buddy(pages, order2);

    heap_size = FREE_PAGES;
    claims = heap_size - 1; /* Claim all but one page */
    ASSERT(!install_host_claims(dom1, claims));

    /* Mark a first page as offline */

    mark_page_offline(pages + 3, 0);
    ASSERT(page_state_is(pages + 3, offlined));

    /* Due to the single unclaimed page, the claims should remain unchanged */
    ASSERT(FREE_PAGES == heap_size);
    ASSERT(TOTAL_CLAIMS == heap_size - 1);
    ASSERT(reserve_offlined_page(pages) == 1);
    ASSERT(FREE_PAGES == heap_size - 1); /* One free page is offlined */
    ASSERT(TOTAL_CLAIMS == heap_size - 1);

    /* Offline a second page. Offlines a portion of the claimed pages pool. */

    mark_page_offline(pages + 1, 0);
    ASSERT(page_state_is(pages + 1, offlined));

    /* Assert the effect of offlining a portion of the claimed pages pool */
    ASSERT(FREE_PAGES == heap_size - 1);
    ASSERT(TOTAL_CLAIMS == heap_size - 1);
    ASSERT(reserve_offlined_page(pages) == 1);
    ASSERT(FREE_PAGES == heap_size - 2); /* Two pages are offlined */
    ASSERT(TOTAL_CLAIMS == heap_size - 2); /* One claim is be released */
}

int main(int argc, char *argv[])
{
    const char *topic = "Test host-wide claims with old and new interfaces";

    if ( !parse_args(argc, argv, topic) )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    /*
     * Run the tests on all levels of the claims interface:
     *
     * 1. Direct page_alloc function call used by other code in the hypervisor,
     *    which needs to support claim cancellation, needed by domain_kill().
     *
     * 2. The domctl helper for the hypercall, which is used by the hypercall
     *    handler itself to parse the hypercall arguments before calling the
     *    page_allocator functions.
     *
     * 3. The real DOMCTL handler, do_domctl(), which is the actual handler
     *    function called when invoking the real hypercall.
     */

    /* Test using the direct claim function call used inside the hypervisor */
    install_host_claims = install_host_claims_legacy;
    RUN_TESTCASE("TCCD", test_claim_alloc_cancel, 8);
    RUN_TESTCASE("DCGD", test_alloc_domheap_redeems_claims, 4);

    /* Test claims setup using the actual DOMCTL handler itself, do_domctl() */
    install_host_claims = xc_domain_claim_memory_host;
    RUN_TESTCASE("TCCH", test_claim_alloc_cancel, 8);
    RUN_TESTCASE("DCGH", test_alloc_domheap_redeems_claims, 4);

    /* Test offlining free pages outside and inside the claimed pages pool */
    RUN_TESTCASE("OHCH", test_offlining_host_claims, 4);

    return test_complete();
}
