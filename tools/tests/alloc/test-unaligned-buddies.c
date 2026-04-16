/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Cloud Software Group
 */

/*
 * This test uses get_outstanding_claims() in ASSERTs to inspect the number
 * of free pages, so enable sysctl support in page_alloc.c.
 */
#define CONFIG_SYSCTL

/*
 * Include the test library that sets up scenarios, asserts allocator state,
 * and provides the definitions and shims needed to call the wrapped
 * page_alloc.c code.
 */
#include "libtest-page-alloc.h"

static void test_unaligned_order_two_buddy(int start_mfn)
{
    struct page_info *page = test_pages + start_mfn, *pg, *predecessor;
    uint32_t status = 0;

    /* PREPARE */
    test_page_list_add_buddy(page, order2); /* Seed the heap */

    /* ACT */
    offline_page(start_mfn, 0, &status);

    /* ASSERT */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(page_aligned(page + 1),
          "MFN %d not aligned: order-%d\n", start_mfn + 1, PFN_ORDER(page + 1));
    EXPECTED_TO_FAIL_END(1);

    /* Allocate and free a page to trigger buddy merging on free. */
    free_domheap_pages(alloc_domheap_pages(dom1, order0, 0), order0);

    /* Allocate the freed page again so the merged page is next. */
    CHECK(alloc_domheap_pages(dom1, order0, 0), "Allocated the 1st page again");

    /* Allocate a second page. */
    CHECK((pg = alloc_domheap_pages(dom1, order0, 0)), "Allocated 2nd page");

    /* Inspect the predecessor in case pg is the tail of an unaligned buddy. */
    predecessor = pg - 1;

    EXPECTED_TO_FAIL_BEGIN();
    CHECK(PFN_ORDER(predecessor) != 1,
          "If an unaligned buddy uses pg as its tail, the next alloc will BUG");

    CHECK(page_to_mfn(pg) != 6, "In the failure case, we just allocated MFN 6");
    EXPECTED_TO_FAIL_END(2);

    /*
     * The heap should contain MFN 6 as an order-0 page:
     *
     * Heap for zone 3, order 0:
     *   mfn 6:
     *     flags: PGC_state_free
     *
     * With an unaligned MFN 5+6 buddy, the heap would instead look like this,
     * with MFN 6 marked in use even though it had appeared twice on the free
     * list after page 7 was freed and merged with its aligned predecessor:
     *
     *  Heap for zone 3, order 1:
     *   mfn 5: not aligned to order 1!
     *     flags: PGC_state_free, subpages of mfn 5 below:
     *     mfn 6: first_dirty 0 ()
     *       flags: PGC_state_inuse <= This will cause a BUG() on the next alloc
     */
    PRINT_HEAP();

    /* Allocate the remaining page; a clean heap should not hit BUG(). */
    testcase_assert_expect_to_hit_bug = 1;
    assert(page_to_mfn(alloc_domheap_pages(dom1, order0, 0)) == 6);
    assert(testcase_assert_expect_to_hit_bug == 0);
}

int main(int argc, char *argv[])
{
    const char *topic = "Test offlining pages with reserve_offline_page()";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();
    RUN_TESTCASE(TUOB, test_unaligned_order_two_buddy, 4);

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
