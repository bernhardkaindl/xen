/* SPDX-License-Identifier: GPL-2.0-only */
/* Test host-wide memory claims */

#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

typedef int (*set_global_claims)(struct domain *d, unsigned long pages);
set_global_claims install_host_claims;

/* test wrapper around xc_domain_claim_pages() */
static int install_host_claims_legacy(struct domain *d, unsigned long pages)
{
    if ( pages == 0 )
        return xc_domain_claim_pages(xch, d->domain_id, 0);

    xc_domain_claim_pages(xch, d->domain_id, 0);
    pages += domain_tot_pages(d);
    return xc_domain_claim_pages(xch, d->domain_id, pages);
}

/* test wrapper around xc_domain_set_memory_claims() */
static int xc_domain_claim_memory_host(struct domain *d, unsigned long pages)
{
    xen_domctl_memory_claim_t claim_set[] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = pages },
    };

    return xc_domain_set_memory_claims(xch, d->domain_id, 1, claim_set);
}

static void test_alloc_domheap_pages(int start_mfn)
{
    int ret;
    struct page_info *pages = frame_table + start_mfn, *pg;

    test_page_list_add_buddy(pages, order2);
    EQ(install_host_claims(dom1, 3), 0);
    EQ(alloc_domheap_pages(dom1, order1, 0), pages + 2);
    EQ(alloc_domheap_pages(dom1, order0, 0), pages + 1);
    EQ(outstanding_claims, 0);
    EQ(total_avail_pages, 1);
    EQ(install_host_claims(dom2, total_avail_pages), 0);
    EQ(install_host_claims(dom1, domain_tot_pages(dom1) + 1), -ENOMEM);
    EQ(install_host_claims(dom1, dom1->max_pages + 1), -EINVAL);
    EQ(alloc_domheap_pages(dom1, order0, 0), NULL);
    EQ(outstanding_claims, 1);
    EQ(total_avail_pages, 1);
}

static void test_claim_and_alloc_mechanics(int start_mfn)
{
    struct page_info *expected_page, *allocated_page;
    struct page_info *page = frame_table + start_mfn;
    unsigned long heap_pages, installed_claims;
    unsigned int alloc_order;

    /*
     * Start with an eight-page buddy. Domain 1 claims half of the free
     * memory and then redeems that claim with order-1 and order-0
     * allocations. The remaining claim must track only unallocated pages.
     */
    test_page_list_add_buddy(page, order3);
    heap_pages = total_avail_pages;
    installed_claims = heap_pages / 2;

    /* Claim half of the free pages for domain 1 */
    EQ(install_host_claims(dom1, installed_claims), 0);
    EQ(outstanding_claims, installed_claims);

    /* Allocate an order 1 page for domain 1 */
    alloc_order = order1;
    /* Expect the highest available page to be allocated */
    expected_page = page + total_avail_pages - (1UL << alloc_order);
    EQ(alloc_domheap_pages(dom1, alloc_order, 0), expected_page);
    EQ(outstanding_claims, (installed_claims -= 1UL << alloc_order));
    EQ(outstanding_claims, installed_claims);

    /* Allocate an order 0 page for domain 1 */
    alloc_order = order0;
    /* Expect the highest available page to be allocated */
    expected_page = page + total_avail_pages - (1UL << alloc_order);
    EQ(test_dummy_domain1.outstanding_pages, 2);
    EQ(alloc_domheap_pages(dom1, alloc_order, 0), expected_page);
    EQ(test_dummy_domain1.outstanding_pages, 1);
    EQ(outstanding_claims, (installed_claims -= 1UL << alloc_order));

    /*
     * Domain 1 still has one outstanding claim. Domain 2 cannot claim more
     * than the unclaimed free pages, nor all currently free pages, because
     * either request would overlap domain 1's remaining claim.
     */
    EQ(install_host_claims(dom2, heap_pages - installed_claims + 1), -ENOMEM);
    EQ(install_host_claims(dom2, total_avail_pages), -ENOMEM);

    /* Test that cancelling claims even when max_pages is set to a low value */
    dom1->max_pages = domain_tot_pages(dom1) - 1;
    EQ(install_host_claims(dom1, 0), 0); /* Cancel remaining claims of dom1 */
    EQ(outstanding_claims, 0);

    installed_claims = total_avail_pages;
    EQ(install_host_claims(dom2, installed_claims), 0); /* Claim all for dom2 */
    EQ(outstanding_claims, installed_claims);

    /*
     * Claiming for domain 1 should fail with EINVAL because max_pages is
     * below domain_tot_pages().
     */
    EQ(install_host_claims(dom1, 1), -EINVAL);

    /*
     * With max_pages above domain_tot_pages(), the claim fails
     * with -ENOMEM because domain 2 owns all currently free pages.
     */
    dom1->max_pages = heap_pages;
    EQ(install_host_claims(dom1, 1), -ENOMEM);
    EQ(alloc_domheap_pages(dom1, order0, 0), NULL);

    /*
     * Domain 2 owns every free page. Domain 1 cannot allocate from those
     * pages, while domain 2 can redeem its claim with an order-0 allocation.
     * Allocating a page for domain 2 still works as it has the claims.
     */
    alloc_order = order0;
    /* Expect the highest available page to be allocated */
    expected_page = page + total_avail_pages - (1UL << alloc_order);
    EQ(alloc_domheap_pages(dom2, alloc_order, 0), expected_page);;
    EQ(outstanding_claims, (installed_claims -= 1UL << alloc_order));

    /*
     * After redeeming one page, the remaining order-2 buddy is exactly
     * the rest of domain 2's claim and must also be allocatable.
     */
    alloc_order = order2;
    /* Expect the highest available page to be allocated */
    expected_page -= (1UL << alloc_order);
    EQ(alloc_domheap_pages(dom2, alloc_order, 0), expected_page);
    EQ(outstanding_claims, (installed_claims -= 1UL << alloc_order));
}

int main(void)
{
    /* Test using the libxc interface xc_domain_claim_pages() */
    install_host_claims = install_host_claims_legacy;
    run_test(test_claim_and_alloc_mechanics, 8);
    run_test(test_alloc_domheap_pages, 4);

    /* Test using the libxc interface xc_domain_set_memory_claims() */
    install_host_claims = xc_domain_claim_memory_host;
    run_test(test_claim_and_alloc_mechanics, 8);
    run_test(test_alloc_domheap_pages, 4);
    return test_complete();
}
