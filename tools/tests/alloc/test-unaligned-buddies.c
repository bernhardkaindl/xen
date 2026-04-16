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

#if 0
static bool page_aligned(struct page_info *pg)
{
    return !(page_to_mfn(pg) & ((1UL << PFN_ORDER(pg)) - 1));
}
#endif

static void test_unaligned_order_two_buddy(int start_mfn)
{
    struct page_info *page = test_pages + start_mfn, *pg, *predecessor;
    uint32_t status = 0;

    /* PREPARE */
    test_page_list_add_buddy(page, order2); /* Seed the heap */

    /* ACT */
    offline_page(start_mfn, 0, &status);

    /* ASSERT */
    CHECK(page_aligned(page + 1),
          "MFN %ld violates the invariant of !(mfn & 2^order) <= 0 (order=%d)!",
          page_to_mfn(page + 1), PFN_ORDER(page + 1));

    /* Allocate and free a page to trigger buddy merging on free. */
    free_domheap_pages(alloc_domheap_pages(dom1, order0, 0), order0);

    /* Allocate the freed page again so the merged page is next. */
    CHECK(alloc_domheap_pages(dom1, order0, 0), "Allocated the 1st page again");

    /* Allocate a second page. */
    CHECK((pg = alloc_domheap_pages(dom1, order0, 0)), "Allocated 2nd page");

    /* Inspect the predecessor in case pg is the tail of an unaligned buddy. */
    predecessor = pg - 1;
    CHECK(PFN_ORDER(predecessor) != 1,
          "If an unaligned buddy uses pg as its tail, the next alloc will BUG");

    CHECK(page_to_mfn(pg) != 6, "In the failure case, we just allocated MFN 6");

    /*
     * Log the heap state. It should contain MFN 6 as an order-0 page:
     *
     * Heap for zone 3, order 0:
     *   mfn 6:
     *     flags: PGC_state_free
     */
    CHECK_BUDDY(page, "If MFN 6 is in use here, the next alloc will hit BUG()");

    /* Allocate the remaining page; a clean heap should not hit BUG(). */
    assert(page_to_mfn(alloc_domheap_pages(dom1, order0, 0)) == 6);
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
