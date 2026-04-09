/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Initial unit tests for reserve_offline_page() in common/page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

/* Enable sysctl support in page_alloc.c for testing get_outstanding_claims() */
#define CONFIG_SYSCTL
#include "libtest-page_alloc.h"

/*
 * Exercise the trivial case where the candidate page is already an order-0
 * offlined page and observe that the allocator accounts for it correctly.
 *
 * Also, claim all available memory for the dummy domain to verify that
 * reserve_offlined_page() adjusts the outstanding_claims to not exceed
 * total_avail_pages after offlining the page, which is important to prevent
 * underflow of unclaimed pages and an inflation of claims beyond memory.
 *
 * The allocator should remove it from the free heap, move it to the offlined
 * list, and decrement both the per-zone and global availability counters by
 * exactly one page.
 */
static void test_single_offlined_page(int mfn)
{
    int              zone, offlined_pages, ret;
    struct page_info pages[1], *page = pages;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(mfn);

    /* Seed a single order-0 buddy onto the heap */
    zone = test_page_list_add_buddy(pages, mfn, order0);
    mark_page_offline(page, 0); /* Mark the page for offlining. */

    /* Claim all available pages for the dummy domain */
    ret = domain_set_outstanding_pages(&test_dummy_domain1, total_avail_pages);
    ASSERT(ret == 0);

    /* Reserve the offlined page out of the free heap. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(page);
    ASSERT_HEAP_CONSISTENCY(pages);

    CHECK(offlined_pages == 1, "Expect one page offlined");
    ASSERT_LIST_EQUAL(&page_offlined_list, page);       /* page is offlined */
    ASSERT(page_list_empty(&heap(node, zone, order0))); /* free list emptied */

    /* The availability counters should reflect the offlined page. */
    ASSERT(FREE_PAGES == 0);
    ASSERT(avail_heap_pages(zone, zone, node) == 0);

    /*
     * When offlining a free page, total_avail_pages is decremented.
     *
     * If needed, reserve_offlined_page() must recall claims to ensure that
     * the invariant of `total_avail_pages >= outstanding_claims` holds.
     */
    ASSERT(TOTAL_CLAIMS <= FREE_PAGES);
}

/*
 * Exercise reserving an offlined page from an order-1 buddy where the tail
 * page is offlined and was also dirty-tracked in the original buddy head.
 *
 * This checks that reserve_offlined_page() rebuilds the surviving free
 * half as an order-0 entry and verifies that first_dirty is cleared on
 * the survivor because the dirty tail is no longer attached to it.
 *
 * This test asserts the following:
 * 1.) splitting the order-1 chunk,
 * 2.) returning the surviving head as an order-0 entry on the heap,
 * 3.) moving the offlined tail page to page_offlined_list, and
 * 4.) clearing first_dirty on the survivor because the dirty tail
 *     no longer remains attached to it after the split.
 */
static void test_mixed_order_one_buddy(unsigned int start_mfn)
{
    unsigned int     zone, offlined_pages;
    struct page_info pages[2];

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* pages[0] is the order-1 head; pages[1] is the offlined subpage. */
    init_test_page(pages + 0, start_mfn++, order1, PGC_state_free);
    init_test_page(pages + 1, start_mfn++, order0, PGC_state_offlined);

    /* first_dirty points at the offlined tail page within the order-1 chunk. */
    pages[0].u.free.first_dirty = 1;

    /* Seed the free list with these pages as a single order-1 buddy. */
    zone = page_to_zone(pages + 0);
    page_list_add(pages + 0, &heap(node, zone, order1));
    avail[node][zone] = 2;
    total_avail_pages = 2;

    /* Reserve the offlined subpage by splitting the order-1 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(pages);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* One page becomes unavailable and one free order-0 page survives. */
    ASSERT(offlined_pages == 1);
    /* The availability counters should reflect the offlined page. */
    ASSERT(FREE_PAGES == 1);
    ASSERT(avail_heap_pages(zone, zone, node) == 1);

    /* No higher-order chunks should remain after rebuilding the survivor. */
    ASSERT(page_list_empty(&heap(node, zone, order1)));

    /* The surviving half should is standalone order-0 free page on the heap. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages + 0);

    /* The offlined half should be tracked on the offlined list. */
    ASSERT_LIST_EQUAL(&page_offlined_list, pages + 1);

    /* Both pages should now be order-0. */
    ASSERT(PFN_ORDER(pages + 0) == 0);
    ASSERT(PFN_ORDER(pages + 1) == 0);

    /*
     * As the dirty tail is now offlined, first_dirty
     * should be cleared on the standalone surviving page.
     */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* first_dirty would be cleared on the offlined page as it is offlined */
    ASSERT(pages[1].u.free.first_dirty == INVALID_DIRTY_IDX);
}

/*
 * Exercise splitting an order-2 buddy where two subpages are already offlined.
 *
 * This verifies that allocator accounting drops by two pages, the surviving
 * free pages are rebuilt as order-0 entries in the observed order, first_dirty
 * follows the surviving tail fragment, and the offlined list preserves the
 * order in which the offlined subpages are discovered during the split.
 */
static void test_two_offlined_pages_order_two(unsigned int start_mfn)
{
    unsigned int     zone, offlined_pages;
    struct page_info pages[4];

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* Seed a single order-2 buddy onto the heap */
    zone = test_page_list_add_buddy(pages, start_mfn, order2);
    mark_page_offline(pages + 1, 0); /* Mark the 2nd page for offlining. */
    mark_page_offline(pages + 3, 0); /* Mark the 4th page for offlining. */

    /* In the head, mark the 3rd page as the first dirty page in the buddy */
    pages[0].u.free.first_dirty = 2;

    /* Reserve all offlined subpages by splitting the original order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(pages);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Two offlined pages should have been removed from free availability. */
    ASSERT(offlined_pages == 2);
    /* The availability counters should reflect the offlined pages. */
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

/* Test merging a tail pair into a surviving order-1 buddy */
static void test_merge_tail_pair(unsigned int start_mfn)
{
    unsigned int     zone, offlined_pages;
    struct page_info pages[4];

    /*
     * PREPARE:
     */

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* Seed a single order-2 buddy onto the heap */
    zone = test_page_list_add_buddy(pages, start_mfn, order2);
    /*
     * Mark the 2nd page for offlining, page 2+3 is the healthy
     * tail pair which should now survive as one order-1 buddy.
     */
    mark_page_offline(pages + 1, 0);
    /* first_dirty points at the last subpage in the original order-2 chunk. */
    pages[0].u.free.first_dirty = 3;

    /* Reserve the offlined subpage out of the larger order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);

    /*
     * ACT:
     */

    offlined_pages = reserve_offlined_page(pages);

    /*
     * ASSERT:
     */

    ASSERT_HEAP_CONSISTENCY(pages);

    /* Only the offlined page should disappear from allocator accounting. */
    ASSERT(offlined_pages == 1);
    ASSERT(FREE_PAGES == 3);
    ASSERT(avail_heap_pages(zone, zone, node) == 3);

    /* Checks for buddy splitting and merging */

    /* The original order-2 entry has been removed from the heap */
    ASSERT(page_list_empty(&heap(node, zone, order2)));

    /* Page 1 was offlined and has been moved to the offlined list */
    ASSERT_LIST_EQUAL(&page_offlined_list, pages + 1);

    /*
     * The surviving tail pair should have been merged into one order-1 buddy
     * covering pages[2] and pages[3] (the offlined page 1 causes the split).
     *
     * The code should use '>' to allow the merge when the next_order end
     * is exactly at the buddy boundary.
     */
    /* Only the existing_order0 page and page-9 are in the order-0 heap. */
    EXPECTED_TO_FAIL_BEGIN();
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages + 0);
    EXPECTED_TO_FAIL_END(12);

    /* The merged order-1 buddy of page-2+3 should be in the order-1 heap. */
    EXPECTED_TO_FAIL_BEGIN();
    ASSERT_LIST_EQUAL(&heap(node, zone, order1), pages + 2);
    EXPECTED_TO_FAIL_END(3);

    CHECK(PFN_ORDER(pages + 0) == 0, "Former head page, now order-0");
    /* The surviving tail pair is merged into one order-1 buddy */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(PFN_ORDER(pages + 2) == 1, "Tail pair should be merged into order-1");
    EXPECTED_TO_FAIL_END(1);
    CHECK(PFN_ORDER(pages + 3) == 0, "page[3] should be order-0 (subpage)");
    CHECK(PFN_ORDER(pages + 1) == 0, "Offlined page should be order-0");

    /* Checks for first_dirty propagation */

    /* pages[0] and pages[1] were prepared as clean pages and still are. */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);
    ASSERT(pages[1].u.free.first_dirty == INVALID_DIRTY_IDX);

    /*
     * Before reserve_offlined_page(), the order-2 chunk had first_dirty=3
     * which means that the page at index 3 was the first dirty page in the
     * chunk. After the split, it is the buddy of pages[2] at index 1 of it:
     */

    EXPECTED_TO_FAIL_BEGIN();
    /* pages[2]+1 is the final page, which was marked dirty, it still is. */
    CHECK(pages[2].u.free.first_dirty == 1, "In tail buddy, the 2nd is dirty");

    /* The tail page of the merged buddy does not use first_dirty */
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
static void test_unaligned_order_one_buddy(unsigned int start_mfn)
{
    unsigned int     zone, offlined_pages;
    struct page_info pages[4];

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* Seed a single order-2 buddy onto the heap */
    zone = test_page_list_add_buddy(pages, start_mfn, order2);
    mark_page_offline(pages + 0, 0); /* Mark the 1st page for offlining. */
    mark_page_offline(pages + 3, 0); /* Mark the 4th page for offlining. */

    /* Point first_dirty at the 3rd subpage in the original order-2 buddy. */
    pages[0].u.free.first_dirty = 2;

    /* Reserve the offlined subpages out of the larger order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(pages);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Only the offlined pages should disappear from allocator accounting. */
    ASSERT(offlined_pages == 2);
    ASSERT(FREE_PAGES == 2);
    ASSERT(avail_heap_pages(zone, zone, node) == 2);

    /* pages 0 and 3 are offlined */
    ASSERT_LIST_EQUAL(&page_offlined_list, pages + 0, pages + 3);

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
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), pages + 1, pages + 2);
    CHECK(PFN_ORDER(pages + 1) == 0, "page[1] should be order-0");
    EXPECTED_TO_FAIL_END(4);
    CHECK(PFN_ORDER(pages + 2) == 0, "page[2] should be order-0");

    /* Checks for first_dirty propagation */

    /* The 1st offlined page should have invalid first_dirty */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /*
     * Before reserve_offlined_page(), the order-2 chunk had first_dirty=2
     * which means that the page at index 2 was the first dirty page in the
     * chunk.
     */
    EXPECTED_TO_FAIL_BEGIN();
    CHECK(pages[1].u.free.first_dirty == INVALID_DIRTY_IDX,
          "page[1] clean: first_dirty invalid");

    CHECK(pages[2].u.free.first_dirty == 0,
          "page[2] dirty: first_dirty refers to itself");
    EXPECTED_TO_FAIL_END(2);

    /* The 2nd offlined page should have invalid first_dirty */
    ASSERT(pages[3].u.free.first_dirty == INVALID_DIRTY_IDX);
}

int main(int argc, char *argv[])
{
    const char *program_name = parse_args(argc, argv);

    if ( !program_name )
        return EXIT_FAILURE;

    printf("Running %s:\n", program_name);

    /* Run the single offlined page test with different starting MFNs */
    for ( unsigned int mfn = 0; mfn < 4; mfn++ )
        test_single_offlined_page(mfn);

    /* These tests use order-2 buddies: Their start must be a multiple of 4 */
    test_mixed_order_one_buddy(4);
    test_two_offlined_pages_order_two(4);
    test_merge_tail_pair(4);
    test_unaligned_order_one_buddy(4);

#ifdef CONFIG_NUMA
    printf("\n%s: CONFIG_NUMA enabled, nodes: %d of %d\n", program_name,
           num_online_nodes(), MAX_NUMNODES);
#else
    printf("\n%s: CONFIG_NUMA disabled\n", program_name);
#endif
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
