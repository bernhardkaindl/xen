/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test basic host-wide functionality of memory claims, including
 * installing and redeeming claims, and that claims are respected
 * by allocations and protected against other allocations.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#define TEST_ENABLE_XC_DOMAIN_C /* Enable xc_domain.c APIs */
#include <native/init.h>

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
    xen_domctl_memory_claim_t claim_set[] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = pages },
    };

    return xc_domain_set_memory_claims(xch, d->domain_id,
                                       ARRAY_SIZE(claim_set), claim_set);
}

static void test_alloc_domheap_redeems_claims(int start_mfn)
{
    int ret;
    struct page_info *pages = frame_table + start_mfn, *pg;

    test_page_list_add_buddy(pages, order2);
    ASSERT(!install_host_claims(dom1, 3));
    ASSERT(alloc_domheap_pages(dom1, order1, 0) == pages + 2);
    ASSERT(alloc_domheap_pages(dom1, order0, 0) == pages + 1);
    ENSURE(outstanding_claims == 0, "claims consumed after allocations");
    ENSURE(total_avail_pages == 1, "one free page after allocations");

    ASSERT(!install_host_claims(dom2, total_avail_pages));

    /* Claim more than dom1 already has fails with ENOMEM (claimed by dom2) */
    ret = install_host_claims(dom1, domain_tot_pages(dom1) + 1);
    ENSURE(ret == -ENOMEM, "dom1 claim +1 fails due to insufficient pages");

    /* Claim more than dom1's d->max_pages fails with EINVAL */
    ret = install_host_claims(dom1, dom1->max_pages + 1);
    ENSURE(ret == -EINVAL, "dom1 claim fails due to exceeding max_pages");

    /* Attempt to allocate an order-0 page with a foreign claim present */
    pg = alloc_domheap_pages(dom1, order0, 0);
    ENSURE(pg == NULL, "dom 1 allocation fails because of domain 2's claim");
    ENSURE(outstanding_claims == 1, "dom2's claims still present");
    ENSURE(total_avail_pages == 1, "one free page after failed alloc");
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
    heap_pages = total_avail_pages;
    claims = heap_pages / 2;

    /* Claim half of the free pages for domain 1 */
    ASSERT(install_host_claims(dom1, claims) == 0);
    ASSERT(outstanding_claims == claims);

    /* Allocate an order 1 page for domain 1 */
    alloc_order = order1;
    /* Expect the highest available page to be allocated */
    expected = page + total_avail_pages - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom1, alloc_order, 0) == expected);
    ASSERT(outstanding_claims == (claims -= 1UL << alloc_order, claims));

    /* Allocate an order 0 page for domain 1 */
    alloc_order = order0;
    /* Expect the highest available page to be allocated */
    expected = page + total_avail_pages - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom1, alloc_order, 0) == expected);
    ASSERT(outstanding_claims == (claims -= 1UL << alloc_order, claims));

    /* Claiming more than unclaimed for domain 2 should fail */
    ASSERT(install_host_claims(dom2, heap_pages - claims + 1) == -ENOMEM);
    /* Claiming all free pages for domain 2 should fail (dom1 has a claim) */
    ASSERT(install_host_claims(dom2, total_avail_pages) == -ENOMEM);
    ASSERT(outstanding_claims == claims);

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
    ASSERT(outstanding_claims == 0);

    /* Claim all free pages for domain 2, should work */
    claims = total_avail_pages;
    ASSERT(install_host_claims(dom2, claims) == 0);
    ASSERT(outstanding_claims == claims);

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
    expected = page + total_avail_pages - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom2, alloc_order, 0) == expected);
    ASSERT(outstanding_claims == (claims -= 1UL << alloc_order, claims));

    /* Even allocating the remaining order 2 buddy for domain 2 works */
    alloc_order = order2;
    /* Expect the highest available page to be allocated */
    expected = page + total_avail_pages - (1UL << alloc_order);
    ASSERT(alloc_domheap_pages(dom2, alloc_order, 0) == expected);
    ASSERT(outstanding_claims == (claims -= 1UL << alloc_order, claims));
}

int main(void)
{
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
    run_test(test_claim_alloc_cancel, 8);
    run_test(test_alloc_domheap_redeems_claims, 4);

    /* Test claims setup using the actual DOMCTL handler itself, do_domctl() */
    install_host_claims = xc_domain_claim_memory_host;
    run_test(test_claim_alloc_cancel, 8);
    run_test(test_alloc_domheap_redeems_claims, 4);

    return test_complete();
}
