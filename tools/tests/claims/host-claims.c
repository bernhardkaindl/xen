/* SPDX-License-Identifier: GPL-2.0-only */
/* Test host-wide memory claims */
#include "claims-xsm-denials.h"
#include "claims-guest-copy.h"

typedef int (*set_global_claims)(struct domain *d, unsigned long pages);
set_global_claims install_host_claims;

/* wrapper around xc_domain_claim_pages() */
static int set_host_claims_legacy(struct domain *d, unsigned long pages)
{
    return xc_domain_claim_pages(xch, d->domain_id,
                                 pages ? pages + domain_tot_pages(d) : 0);
}

/* wrapper around xc_domain_set_memory_claims() */
static int set_host_claims(struct domain *d, unsigned long pages)
{
    xen_domctl_memory_claim_t claim_set[] = {
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = pages },
    };
    uint32_t nr = (pages ? 1 : 0);

    return xc_domain_set_memory_claims(xch, d->domain_id, claim_set, nr);
}

static void test_alloc_domheap_pages(int start_mfn)
{
    int ret;
    struct page_info *pages = frame_table + start_mfn, *pg;
    xen_domctl_memory_claim_t set[] = {
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 3 },
    };

    test_page_list_add_buddy(pages, order2);
    XE(install_host_claims(dom1, 3), 0);
    EQ_CLAIMS(dom1, set);
    EQ(alloc_domheap_pages(dom1, order1, 0), pages + 2);
    EQ(alloc_domheap_pages(dom1, order0, 0), pages + 1);
    EQ(outstanding_claims, 0);
    EQ(total_avail_pages, 1);
    XE(install_host_claims(dom2, total_avail_pages), 0);
    XE(install_host_claims(dom1, domain_tot_pages(dom1) + 1), -ENOMEM);
    XE(install_host_claims(dom1, dom1->max_pages + 1), -EINVAL);
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
    XE(install_host_claims(dom1, installed_claims), 0);
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
    XE(install_host_claims(dom2, heap_pages - installed_claims + 1), -ENOMEM);
    XE(install_host_claims(dom2, total_avail_pages), -ENOMEM);

    /* Test claims reset when max_pages is set below domain_tot_pages(d) */
    dom1->max_pages = domain_tot_pages(dom1) - 1;
    XE(install_host_claims(dom1, 0), 0);
    EQ(outstanding_claims, 0);

    installed_claims = total_avail_pages;
    EQ(install_host_claims(dom2, installed_claims), 0);
    EQ(outstanding_claims, installed_claims);

    /*
     * Claiming for domain 1 should fail with EINVAL because max_pages is
     * below domain_tot_pages().
     */
    XE(install_host_claims(dom1, 1), -EINVAL);

    /*
     * With max_pages above domain_tot_pages(), the claim fails
     * with -ENOMEM because domain 2 owns all currently free pages.
     */
    dom1->max_pages = heap_pages;
    XE(install_host_claims(dom1, 1), -ENOMEM);
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

static void test_claims_after_creation(int start_mfn)
{
    test_page_list_add_buddy(frame_table + start_mfn, order2);

    XE(set_host_claims(dom1, 1), 0);
    EQ(outstanding_claims, 1);
    XE(set_host_claims(dom1, 0), 0);
    EQ(outstanding_claims, 0);

    dom1->creation_finished = true;
    XE(set_host_claims(dom1, 2), -EEXIST);
    EQ(outstanding_claims, 0);

    XE(set_host_claims_legacy(dom1, 2), 0);
    EQ(outstanding_claims, 2);
    XE(set_host_claims(dom1, 3), -EEXIST);
    EQ(outstanding_claims, 2);

    XE(set_host_claims(dom1, 0), 0);
    EQ(outstanding_claims, 0);
}

/* Test setting claims when claims have been set for the domain before */
static void test_claims_set_once(int start_mfn)
{
    test_page_list_add_buddy(frame_table + start_mfn, order2);

    XE(set_host_claims(dom1, 1), 0);
    EQ(outstanding_claims, 1);
    XE(set_host_claims(dom1, 2), -EINVAL); /* 2nd call is expected to fail */
    EQ(outstanding_claims, 1);

    XE(set_host_claims(dom1, 0), 0);
    EQ(outstanding_claims, 0);
    XE(set_host_claims(dom1, 2), 0); /* Success after resetting claims to 0 */
    EQ(outstanding_claims, 2);
}

int main(void)
{
    run_test(test_claim_xsm_denials, 4);
    run_test(test_guest_copy_faults, 4);
    run_test(test_domctl_copyback_faults, 4);

    /* Test using the libxc interface xc_domain_claim_pages() */
    install_host_claims = set_host_claims_legacy;
    run_test(test_claim_and_alloc_mechanics, 8);
    run_test(test_alloc_domheap_pages, 4);

    /* Test using the libxc interface xc_domain_set_memory_claims() */
    install_host_claims = set_host_claims;
    run_test(test_claim_and_alloc_mechanics, 8);
    run_test(test_alloc_domheap_pages, 4);
    run_test(test_claims_after_creation, 4);
    run_test(test_claims_set_once, 4);
    return test_complete();
}
