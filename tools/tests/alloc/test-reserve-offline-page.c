/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration tests for offline_page() verifying reserve_offlined_page()
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
 * 3. Rebuilds healthy buddies from those spans and
 *    adds them back to the free list via page_list_add_scrub().
 * 4. Moves offlined subpages to the offlined page lists.
 *
 * Another workflow marks an in-use page for offlining and then
 * relies on free_heap_pages() to call reserve_offlined_page()
 * when that page is eventually freed.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#include "libtest-page-alloc.h"

uint32_t status;

static void test_offline_head_page(int start_mfn)
{
    struct page_info *page   = test_pages + start_mfn;

    test_page_list_add_buddy(page, order1);
    ASSERT(offline_page(start_mfn, 0, &status) == 0);
    ASSERT(FREE_PAGES == 1);
    ASSERT(PFN_ORDER(page + 1) == 0);

    EXPECTED_TO_FAIL_BEGIN();
    CHECK(PFN_ORDER(page) == 0, "Single offlined head page should be order-0");
    EXPECTED_TO_FAIL_END(1);
}

static void test_offline_dirty_tail(int start_mfn)
{
    struct page_info *page   = test_pages + start_mfn;

    /* PREPARE */
    test_page_list_add_buddy(page, order1);
    page[1].count_info |= PGC_need_scrub; /* Mark the tail page dirty */
    page[0].u.free.first_dirty = 1; /* Tail page at index 1 marked is dirty */

    /* ACT */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline the tail */

    /* ASSERT */
    ASSERT(FREE_PAGES == 1);
    CHECK(PFN_ORDER(page + 0) == 0, "After split, the 1st page is order-0");
    ASSERT(PFN_ORDER(page + 1) == 0);
    /* The tail page was split and offlined: Head's first_dirty is cleared. */
    ASSERT(page[0].u.free.first_dirty == INVALID_DIRTY_IDX);
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
    struct page_info *pages  = test_pages + start_mfn;

    test_page_list_add_buddy(pages, order2);
    pages[2].count_info |= PGC_need_scrub; /* Mark the third page dirty. */
    pages[0].u.free.first_dirty = 2;

    /*
     * Reserving the offlined pages should split the original order-2 buddy
     * into order-0 fragments, move the offlined pages to the offlined list,
     * and adjust first_dirty on the survivors to track the dirty page in the
     * rebuilt structure.
     */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline 2nd page */
    ASSERT(offline_page(start_mfn + 3, 0, &status) == 0); /* Offline 4th page */

    ASSERT(FREE_PAGES == 2);
    ASSERT(PFN_ORDER(pages + 0) == 0);
    ASSERT(PFN_ORDER(pages + 2) == 0);
    /*
     * The fragment covering the original dirty tail should retain first_dirty.
     */
    ASSERT(pages[2].u.free.first_dirty == 0);
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);
}

/* Test merging a surviving tail pair into an order-1 buddy. */
static void test_merge_tail_pair(int start_mfn)
{
    unsigned int zone;
    struct page_info *pages  = test_pages + start_mfn;

    /* PREPARE */
    /* Seed a single order-2 buddy onto the heap. */
    zone = test_page_list_add_buddy(pages, order2);

    /* Mark the fourth page dirty to verify dirty-state preservation. */
    pages[3].count_info |= PGC_need_scrub;
    pages[0].u.free.first_dirty = 3;

    /* ACT */
    ASSERT(offline_page(start_mfn + 1, 0, &status) == 0); /* Offline 2nd page */
    ASSERT(status & PG_OFFLINE_OFFLINED);
    ASSERT(FREE_PAGES == 3);

    /*
     * The surviving tail pair should merge into one order-1 buddy covering
     * pages[2] and pages[3]; offlining page 1 causes the split.
     *
     * The code should use '>' to allow the merge when the next_order end is
     * exactly at the buddy boundary.
     */
    CHECK(PFN_ORDER(pages + 0) == 0, "Former head page, now order-0");

    /* The surviving tail pair is merged into one order-1 buddy. */
    CHECK(PFN_ORDER(pages + 2) == 1, "Tail pair should be merged into order-1");
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

    /* pages[2] + 1 is the final page, which remains the dirty page. */
    CHECK(pages[2].u.free.first_dirty == 1, "In tail buddy, the 2nd is dirty");

    /* The tail page of the merged buddy does not use first_dirty. */
    CHECK(pages[3].u.free.first_dirty == INVALID_DIRTY_IDX,
          "Tail page of the merged buddy should not use first_dirty");
}

int main(int argc, char *argv[])
{
    const char *topic = "Integration test of offline_page()";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();
    RUN_TESTCASE(TOHP, test_offline_head_page, 2);
    RUN_TESTCASE(TODT, test_offline_dirty_tail, 2);
    RUN_TESTCASE(TTOP, test_two_offlined_pages_order_two, 4);
    RUN_TESTCASE(TMTP, test_merge_tail_pair, 4);
    return testcase_print_summary(program_name);
}
