/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration tests for offlining pages in common/page_alloc.c.
 *
 * These tests offline pages from buddies of different orders and verify that
 * reserve_offlined_page() rebuilds the surviving free pages correctly.
 * They also check allocator accounting and preserve the integrity of the
 * buddy structure on both the heap and the offlined list.
 *
 * One workflow covered here is offlining a free page:
 *
 * 1. offline_page() calls mark_page_offlined() to mark the page.
 * 2. It calls reserve_heap_page() to find the containing buddy.
 * 3. It calls reserve_offlined_page() to reserve the marked pages within
 *    that buddy.
 *
 * reserve_offlined_page() then:
 *
 * 1. Removes the buddy, a 2^order group of pages, from the free list.
 * 2. Finds size-aligned spans of healthy pages within it.
 * 3. Rebuilds healthy buddies from those spans and returns them to the free
 *    list via page_list_add_scrub().
 * 4. Moves offlined subpages to the offlined page lists.
 *
 * Another workflow marks an in-use page for offlining and then
 * relies on free_heap_pages() to call reserve_offlined_page()
 * when that page is eventually freed.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

/*
 * This test suite uses get_outstanding_claims() in ASSERTs to inspect the
 * number of free pages, so enable sysctl support in page_alloc.c.
 */
#define CONFIG_SYSCTL

/*
 * Include the test library that sets up scenarios, asserts allocator state,
 * and wraps page_alloc.c with the definitions and shims needed to call the
 * real allocator code from the tests.
 */
#include "libtest-page-alloc.h"

/*
 * Verify that reserve_offlined_page() can split an order-1 buddy with the
 * tail page offlined and dirty-tracked, then rebuild the surviving head as
 * an order-0 page on the heap with first_dirty cleared.
 *
 * This test covers:
 * 1. splitting the order-1 buddy,
 * 2. returning the surviving head as an order-0 entry on the heap,
 * 3. moving the offlined tail page to page_offlined_list, and
 * 4. clearing first_dirty on the survivor because the dirty tail is no
 *    longer attached after the split.
 */
static void test_mixed_order_one_buddy(int start_mfn)
{
    unsigned int zone;
    struct page_info *page   = test_pages + start_mfn;
    uint32_t status = 0;

    /* PREPARE */
    /* Create an order-1 buddy and add it to the heap. */
    zone = test_page_list_add_buddy(page, order1);

    /* Mark the 2nd page as dirty. */
    page[1].count_info |= PGC_need_scrub;
    page[0].u.free.first_dirty = 1;

    /* ACT */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(page, "After offlining MFN %u", start_mfn + 1);

    /* Of the two seeded pages, one remains online. */
    ASSERT(FREE_PAGES == 1);
    ASSERT(avail_heap_pages(zone, zone, node) == 1);

    /* No higher-order chunks should remain after rebuilding the survivor. */
    ASSERT(page_list_empty(&heap(node, zone, order1)));

    /* The surviving half should be a standalone order-0 free page. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), page + 0);

    /* The offlined half should be tracked on the offlined list. */
    ASSERT_LIST_EQUAL(&page_offlined_list, page + 1);

    /* Both pages should now be order-0. */
    ASSERT(PFN_ORDER(page + 0) == 0);
    ASSERT(PFN_ORDER(page + 1) == 0);

    /*
     * As the dirty tail is now offlined, first_dirty
     * should be cleared on the standalone surviving page.
     */
    ASSERT(page[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* first_dirty should also be cleared on the offlined page. */
    ASSERT(page[1].u.free.first_dirty == INVALID_DIRTY_IDX);
}

/*
 * Exercise splitting an order-2 buddy where two subpages are already offlined.
 *
 * This verifies that allocator accounting drops by two pages, the surviving
 * free pages are rebuilt as order-0 entries in the observed order, first_dirty
 * follows the surviving tail fragment, and the offlined list preserves the
 * order in which the offlined subpages are discovered during the split.
 */
static void test_two_offlined_pages_order_two(int start_mfn)
{
    unsigned int zone;
    struct page_info *pages  = test_pages + start_mfn;
    uint32_t status = 0;

    /* PREPARE */
    /* Seed a single order-2 buddy with four pages. */
    zone = test_page_list_add_buddy(pages, order2);

    /* Mark the third page dirty. */
    pages[2].count_info |= PGC_need_scrub;
    pages[0].u.free.first_dirty = 2;

    /* ACT */
    /*
     * Reserving the offlined pages should split the original order-2 buddy
     * into order-0 fragments, move the offlined pages to the offlined list,
     * and adjust first_dirty on the survivors to track the dirty page in the
     * rebuilt structure.
     */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);
    status = 0;
    ASSERT(offline_page(start_mfn + 3, 0, &status) == 0); /* Offline 4th page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(pages, "After offlining the 2nd and 4th page in the buddy");

    /* Of the four seeded pages, two remain online. */
    ASSERT(FREE_PAGES == 2);
    ASSERT(avail_heap_pages(zone, zone, node) == 2);

    /* No higher-order chunks should remain after rebuilding the survivors. */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    ASSERT(page_list_empty(&heap(node, zone, order1)));

    /* The surviving free fragments should appear as two order-0 pages. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages + 0, pages + 2);
    ASSERT(PFN_ORDER(pages + 0) == 0);
    ASSERT(PFN_ORDER(pages + 2) == 0);

    /*
     * The fragment covering the original dirty tail should retain first_dirty.
     */
    ASSERT(pages[2].u.free.first_dirty == 0);
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* Both offlined pages should be queued in the observed discovery order. */
    ASSERT_LIST_EQUAL(&page_offlined_list, pages + 1, pages + 3);
}

/* Test merging a surviving tail pair into an order-1 buddy. */
static void test_merge_tail_pair(int start_mfn)
{
    unsigned int zone;
    struct page_info *pages  = test_pages + start_mfn;
    uint32_t status = 0;

    /* PREPARE */
    /* Seed a single order-2 buddy onto the heap. */
    zone = test_page_list_add_buddy(pages, order2);

    /* Mark the fourth page dirty to verify dirty-state preservation. */
    pages[3].count_info |= PGC_need_scrub;
    pages[0].u.free.first_dirty = 3;

    /* ACT */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(pages, "After offlining the 2nd page in the buddy");

    /* Of the four seeded pages, three remain online. */
    ASSERT(FREE_PAGES == 3);
    ASSERT(avail_heap_pages(zone, zone, node) == 3);

    /* Check buddy splitting and merging. */

    /* The original order-2 entry has been removed from the heap. */
    ASSERT(page_list_empty(&heap(node, zone, order2)));

    /* Page 1 was offlined and moved to the offlined list. */
    ASSERT_LIST_EQUAL(&page_offlined_list, pages + 1);

    /*
     * The surviving tail pair should merge into one order-1 buddy covering
     * pages[2] and pages[3]; offlining page 1 causes the split.
     *
     * The code should use '>' to allow the merge when the next_order end is
     * exactly at the buddy boundary.
     */
#if ASAN_ENABLED
    /*
     * Temporarily skip this check under ASAN until the broken buddy issues
     * caused by reserve_offlined_page() are resolved.
     */
    printf("ASAN is enabled, skip heap check due to stack-buffer-overflow\n");
#else
    EXPECTED_TO_FAIL_BEGIN();
    /* Only the former buddy head page remains in the order-0 heap. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages + 0);
    EXPECTED_TO_FAIL_END(12);
#endif

    /* The merged order-1 buddy of page-2+3 should be in the order-1 heap. */
    EXPECTED_TO_FAIL_BEGIN();
    ASSERT_LIST_EQUAL(&heap(node, zone, order1), pages + 2);
    EXPECTED_TO_FAIL_END(3);

    CHECK(PFN_ORDER(pages + 0) == 0, "Former head page, now order-0");
    /* The surviving tail pair is merged into one order-1 buddy. */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(PFN_ORDER(pages + 2) == 1, "Tail pair should be merged into order-1");
    EXPECTED_TO_FAIL_END(1);
    CHECK(PFN_ORDER(pages + 3) == 0, "page[3] should be order-0 (subpage)");
    CHECK(PFN_ORDER(pages + 1) == 0, "Offlined page should be order-0");

    /* Check first_dirty propagation. */

    /* pages[0] and pages[1] were prepared as clean pages and still are. */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);
    ASSERT(pages[1].u.free.first_dirty == INVALID_DIRTY_IDX);

    /*
     * Before reserve_offlined_page(), the order-2 chunk had first_dirty = 3,
     * meaning page index 3 was the first dirty page in the chunk. After the
     * split, it becomes index 1 within the buddy headed by pages[2].
     */

    EXPECTED_TO_FAIL_BEGIN();
    /* pages[2] + 1 is the final page, which remains the dirty page. */
    CHECK(pages[2].u.free.first_dirty == 1, "In tail buddy, the 2nd is dirty");

    /* The tail page of the merged buddy does not use first_dirty. */
    CHECK(pages[3].u.free.first_dirty == INVALID_DIRTY_IDX,
          "Tail page of the merged buddy should not use first_dirty");
    EXPECTED_TO_FAIL_END(2);
}

/*
 * Exercise splitting an order-2 buddy where the first and the last subpage
 * are offlined and the middle two subpages are healthy.
 *
 * This checks if reserve_offlined_page() correctly splits the order-2 chunk
 * into four order-0 fragments: It should move first and last offlined pages
 * to the offlined list and the middle two healthy pages in the middle should
 * not be merged into an order-1 buddy because the invariant is that buddies
 * must be naturally aligned to their size, and the middle two pages are not
 * aligned to an order-1 boundary.
 */
static void test_unaligned_order_one_buddy(int start_mfn)
{
    unsigned int zone;
    struct page_info *page = test_pages + start_mfn;
    uint32_t status = 0;

    /* PREPARE */
    /* Seed a single order-2 buddy onto the heap */
    zone = test_page_list_add_buddy(page, order2);

    /*
     * To test updating first_dirty on the surviving pages after the split,
     * we mark the 3rd page as the first dirty page in the buddy. After the
     * split, the first_dirty index of the 3rd page should be updated to 0.
     */
    page[2].count_info |= PGC_need_scrub;
    page[0].u.free.first_dirty = 2;

    /* ACT */
    ASSERT(offline_page(start_mfn + 0, 0, &status) == 0); /* Offline 1st page */
    ASSERT(status & PG_OFFLINE_OFFLINED);
    status = 0;
    ASSERT(offline_page(start_mfn + 3, 0, &status) == 0); /* Offline 4th page */
    ASSERT(status & PG_OFFLINE_OFFLINED);

    /* ASSERT */
    CHECK_BUDDY(page, "After reserving the offlined subpage");

    /* Out of the four pages seeded onto the heap, two remain online*/
    ASSERT(FREE_PAGES == 2);
    ASSERT(avail_heap_pages(zone, zone, node) == 2);

    /* pages 0 and 3 are offlined */
    ASSERT_LIST_EQUAL(&page_offlined_list, page + 0, page + 3);

    /* Checks for buddy splitting and merging */

    /*
     * The original order-2 entry should be gone after extracting
     * the offlined page from it.
     */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    /*
     * The order-1 heap should also be empty because the middle pages are
     * not aligned to an order-1 boundary and should not be merged.
     */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(page_list_empty(&heap(node, zone, order1)),
          "order-1 heap empty as pages aren't aligned");
    EXPECTED_TO_FAIL_END(1);

    /*
     * The two middle pages must NOT be coalesced into an order-1 buddy.
     *
     * Buddies must be naturally aligned to their size: an order-k block
     * must start at a MFN that is a multiple of 2^k pages. The middle
     * pair of subpages in this test are not aligned to an order-1
     * boundary, so merging them would create an unaligned order-1
     * buddy and violate that invariant.
     *
     * This test asserts that after removing the first and last subpages
     * from an order-2 chunk, the two surviving middle pages remain as
     * separate order-0 free pages and must not be merged.
     */
    EXPECTED_TO_FAIL_BEGIN();
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), page + 1, page + 2);
    CHECK(PFN_ORDER(page + 1) == 0, "page[1] should be order-0");
    EXPECTED_TO_FAIL_END(4);
    CHECK(PFN_ORDER(page + 2) == 0, "page[2] should be order-0");

    /* Checks for first_dirty propagation */

    /* The 1st offlined page should have invalid first_dirty */
    ASSERT(page[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /*
     * Before reserve_offlined_page(), the order-2 chunk had first_dirty=2
     * which means that the page at index 2 was the first dirty page in the
     * chunk.
     */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(page[1].u.free.first_dirty == INVALID_DIRTY_IDX,
          "page[1] clean: first_dirty invalid");

    CHECK(page[2].u.free.first_dirty == 0,
          "page[2] dirty: first_dirty refers to itself");
    EXPECTED_TO_FAIL_END(2);

    /* The 2nd offlined page should have invalid first_dirty */
    ASSERT(page[3].u.free.first_dirty == INVALID_DIRTY_IDX);
}

int main(int argc, char *argv[])
{
    const char *topic = "Test offlining pages with reserve_offline_page()";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    RUN_TESTCASE(TMOB, test_mixed_order_one_buddy, 4);
    RUN_TESTCASE(TTOP, test_two_offlined_pages_order_two, 4);
    RUN_TESTCASE(TMTP, test_merge_tail_pair, 4);
    RUN_TESTCASE(TUOB, test_unaligned_order_one_buddy, 4);

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
