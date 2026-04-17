/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration test for offlining pages with outstanding claims in page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

/* Enable sysctl support in page_alloc.c for testing get_outstanding_claims() */
#define CONFIG_SYSCTL
#define CONFIG_NUMA
#include "libtest-page-alloc.h"

/*
 * Claim 3 of 4 pages globally, offline two pages, and the 2nd offline should
 * recall one claim to prevent over-claiming beyond the available memory.
 *
 * As part of offline_page(), reserve_offlined_page() should recall the
 * needed claims to not exceed the number of pages that are are remaining.
 */
static void test_offlining_with_global_claims(int mfn)
{
    struct page_info *page = test_pages + mfn;
    uint32_t status = 0;
    memory_claim_t claims[1] = {
        {.pages = 3, .target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL}
    };

    /* PREPARE */
    test_page_list_add_buddy(page, order2); /* Add a buddy with 4 free pages*/
    ASSERT(domain_install_claim_set(dom1, ARRAY_SIZE(claims), claims) == 0);
    offline_page(mfn + 3, 0, &status); /* Offline the 1st page */
    ASSERT(status & PG_OFFLINE_OFFLINED);
    CHECK(TOTAL_CLAIMS == 3, "Still 3 claims before offlining the 2nd page");

    /* ACT */
    /* Offlining the 2nd page must reduce the free pages and claims to 2 */
    status = 0;
    offline_page(mfn + 1, 0, &status); /* Offline the 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(page, "After offlining the 2nd page");
    CHECK(FREE_PAGES == 2, "Expect 2 free pages after offlining two pages");
    CHECK(TOTAL_CLAIMS == 2, "Expect 2 claims after offlining two pages");
}


/*
 * Claim 3 of 4 pages on node0, offline two pages, and the 2nd offline should
 * recall one claim to prevent over-claiming beyond the available memory.
 *
 * As part of offline_page(), reserve_offlined_page() should recall the
 * needed claims to not exceed the number of pages that are are remaining.
 */
static void test_offlining_with_node_claims(int mfn)
{
    struct page_info *page = test_pages + mfn;
    uint32_t status = 0;
    memory_claim_t claims[1] = { {.pages = 3, .target = node0} };

    /* PREPARE */
    test_page_list_add_buddy(page, order2); /* Add a buddy with 4 free pages*/
    ASSERT(domain_install_claim_set(dom1, ARRAY_SIZE(claims), claims) == 0);
    ASSERT(offline_page(mfn + 3, 0, &status) == 0); /* Offline the 1st page */
    ASSERT(status & PG_OFFLINE_OFFLINED);
    CHECK(TOTAL_CLAIMS == 3, "Still 3 claims before offlining the 2nd page");

    /* ACT */
    /* Offlining the 2nd page must reduce the free pages and claims to 2 */
    ASSERT(offline_page(mfn + 1, 0, &status) == 0); /* Offline the 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(page, "After offlining the 2nd page");
    CHECK(FREE_PAGES == 2, "Expect 2 free pages after offlining two pages");
    CHECK(TOTAL_CLAIMS == 2, "Expect 2 claims after offlining two pages");
}

int main(int argc, char *argv[])
{
    const char *topic = "Test offlining with memory claims";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    RUN_TESTCASE(OWGC, test_offlining_with_global_claims, 4);
    RUN_TESTCASE(OWNC, test_offlining_with_node_claims, 4);

    return testcase_print_summary(program_name);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
