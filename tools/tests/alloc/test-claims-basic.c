/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Cloud Software Group
 */
#include "libtest-page-alloc.h"

int test_set_global_claims(struct domain *d, unsigned long pages)
{
    memory_claim_t claim_set[] = {
        {.target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL, .pages = pages},
    };

    return domain_install_claim_set(d, ARRAY_SIZE(claim_set), claim_set);
}
typedef int (*set_global_claims)(struct domain *d, unsigned long pages);

set_global_claims install_global_claims = test_set_global_claims;

static void test_alloc_domheap_redeems_claims(int start_mfn)
{
    int ret;
    struct page_info *pages = test_pages + start_mfn, *pg;

    test_page_list_add_buddy(pages, order2);
    ASSERT(!test_set_global_claims(dom1, 3));
    ASSERT(alloc_domheap_pages(dom1, order1, 0) == pages + 2);
    ASSERT(alloc_domheap_pages(dom1, order0, 0) == pages + 1);
    CHECK(TOTAL_CLAIMS == 0, "Expect all claims consumed after allocations");
    CHECK(FREE_PAGES == 1, "Expect one free page after allocations");

    ASSERT(!test_set_global_claims(dom2, FREE_PAGES));

    /* Claim more than dom1 already has fails with ENOMEM (claimed by dom2) */
    ret = test_set_global_claims(dom1, domain_tot_pages(dom1) + 1);
    CHECK(ret == -ENOMEM, "dom 1 claim +1 fails due to insufficient pages");

    /* Claim more than dom1's d->max_pages fails with EINVAL */
    ret = test_set_global_claims(dom1, dom1->max_pages + 1);
    CHECK(ret == -EINVAL, "dom 1 claim fails due to exceeding max_pages");

    /* Attempt to allocate an order-0 page with a foreign claim present */
    pg = alloc_domheap_pages(dom1, order0, 0);
    CHECK(pg == NULL, "dom 1 allocation fails because of domain 2's claim");
    CHECK(TOTAL_CLAIMS == 1, "Expect domain 2's claim to be still present");
    CHECK(FREE_PAGES == 1, "Expect one free page after failed alloc");
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
    const char *topic = "Test global claims with old and new interfaces";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    RUN_TESTCASE(TCCL, test_cancel_claims, 4);

    /* Confirm the baseline of using domain_set_outstanding_pages() */
    install_global_claims = domain_set_outstanding_pages;
    RUN_TESTCASE(ADCL, test_alloc_domheap_redeems_claims, 4);

    /* Repeat the same test case using test_set_global_claims() */
    install_global_claims = test_set_global_claims;
    RUN_TESTCASE(ADCG, test_alloc_domheap_redeems_claims, 4);

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
