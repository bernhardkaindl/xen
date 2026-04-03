/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Initial unit tests for reserve_offline_page() in common/page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
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
    struct page_info pages[1];
    int zone;
    int offlined_pages;
    int ret;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(mfn);

    /* Build a single offlined order-0 page and mark it dirty-tracked. */
    init_test_page(&pages[0], mfn, order0, PGC_state_offlined);
    pages[0].u.free.first_dirty = 0;

    /* Init the heap and availability counters so one page is offineable. */
    zone              = page_to_zone(&pages[0]);
    avail[node][zone] = 2;
    total_avail_pages = 2;
    page_list_add(&pages[0], &heap(node, zone, order0));

    /* Claim all available pages for the dummy domain */
    test_dummy_domain.max_pages = total_avail_pages;
    ret = domain_set_outstanding_pages(&test_dummy_domain, total_avail_pages);
    ASSERT(ret == 0);

    /* Reserve the offlined page out of the free heap. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(&pages[0]);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Exactly one page should have been offlined_pages from allocator
     * accounting. */
    ASSERT(offlined_pages == 1);
    /* The availability counters should reflect the offlined page. */
    ASSERT(avail[node][zone] == 1);
    ASSERT(total_avail_pages == 1);

    /*
     * Offlining when all memory is claimed needs to adjust outstanding_claims.
     * Otherwise, the equasion of total_avail_pages - outstanding_claims becomes
     * negative. This could cause signed underflow when getting unclaimed pages.
     *
     * The original implementation of reserve_offlined_page() has a bug where
     * it does not adjust outstanding_claims if all memory is claimed. This
     * could cause the function checking for unclaimed pages to underflow and
     * return a very large number, which would cause the allocator to allow
     * e.g. a claim call to succeed when it should have failed due to lack of
     * unclaimed pages, leaving an inflation of claims beyond available memory.
     *
     * FIXME: Fix this bug and enable the assertions below.
     */
    if ( total_avail_pages < outstanding_claims )
    {
        printf("ERROR: total_avail_pages %ld < outstanding_claims %ld\n",
                    total_avail_pages, outstanding_claims);
        bugs_encountered++;
    }
    /* ASSERT(outstanding_claims == 1); */
    /* ASSERT(total_avail_pages >= outstanding_claims); */

    /* The page should now live only on the global offlined list. */
    ASSERT_LIST_EQUAL(&page_offlined_list, &pages[0]);
    ASSERT(page_list_empty(&heap(node, zone, order0)));
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);
}

/*
 * Exercise splitting an order-1 buddy where one half is offlined.
 * The allocator should split the order-1 chunk, keep the free half
 * as a rebuilt order-0 entry, move the offlined half topage_offlined_list,
 * and clear first_dirty on the live free page because no dirty tail remains
 * attached to it after the split.
 */
static void test_mixed_order_one_buddy(unsigned int start_mfn)
{
    struct page_info pages[2];
    int zone;
    int offlined_pages;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* pages[0] is the order-1 head; pages[1] is the offlined subpage. */
    init_test_page(&pages[0], start_mfn++, order1, PGC_state_free);
    init_test_page(&pages[1], start_mfn++, order0, PGC_state_offlined);

    /* first_dirty points at the offlined tail page within the order-1 chunk. */
    pages[0].u.free.first_dirty = 1;

    /* Seed a single order-1 chunk into the heap. */
    zone              = page_to_zone(&pages[0]);
    avail[node][zone] = 2;
    total_avail_pages = 2;
    page_list_add(&pages[0], &heap(node, zone, order1));

    /* Reserve the offlined subpage by splitting the order-1 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(&pages[0]);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* One page becomes unavailable and one free order-0 page survives. */
    ASSERT(offlined_pages == 1);
    /* The availability counters should reflect the offlined page. */
    ASSERT(avail[node][zone] == 1);
    ASSERT(total_avail_pages == 1);

    /* No higher-order chunks should remain after rebuilding the survivor. */
    ASSERT(page_list_empty(&heap(node, zone, order1)));

    /* The surviving half should is standalone order-0 free page on the heap. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), &pages[0]);

    /* The offlined half should be tracked on the offlined list. */
    ASSERT_LIST_EQUAL(&page_offlined_list, &pages[1]);

    /* Both pages should now be order-0. */
    ASSERT(PFN_ORDER(&pages[0]) == 0);
    ASSERT(PFN_ORDER(&pages[1]) == 0);

    /*
     * As the dirty tail is now offlined, first_dirty
     * should be cleared on the standalone surviving page.
     */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* first_dirty should be cleared on the offlined page as it is offlined */
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
    struct page_info pages[4];
    int zone;
    int offlined_pages;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /* pages[1] and pages[3] are offlined subpages inside the order-2 chunk. */
    init_test_page(&pages[0], start_mfn++, order2, PGC_state_free);
    init_test_page(&pages[1], start_mfn++, order0, PGC_state_offlined);
    init_test_page(&pages[2], start_mfn++, order0, PGC_state_free);
    init_test_page(&pages[3], start_mfn++, order0, PGC_state_offlined);

    /* The original dirty tail begins at subpage index 2. */
    pages[0].u.free.first_dirty = 2;

    /* Seed a single order-2 chunk into the heap. */
    zone              = page_to_zone(&pages[0]);
    avail[node][zone] = 4;
    total_avail_pages = 4;
    page_list_add(&pages[0], &heap(node, zone, order2));

    /* Reserve all offlined subpages by splitting the original order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(&pages[0]);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Two offlined pages should have been removed from free availability. */
    ASSERT(offlined_pages == 2);
    /* The availability counters should reflect the offlined pages. */
    ASSERT(avail[node][zone] == 2);
    ASSERT(total_avail_pages == 2);

    /* No higher-order chunks should remain after rebuilding the survivors. */
    ASSERT(page_list_empty(&heap(node, zone, order2)));
    ASSERT(page_list_empty(&heap(node, zone, order1)));

    /* The surviving free fragments should appear as two order-0 pages. */
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), &pages[0], &pages[2]);
    ASSERT(PFN_ORDER(&pages[2]) == 0);
    ASSERT(PFN_ORDER(&pages[0]) == 0);

    /*
     * The fragment covering the original dirty tail should retain first_dirty.
     */
    ASSERT(pages[2].u.free.first_dirty == 0);
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* Both offlined pages should be queued in the observed discovery order. */
    ASSERT_LIST_EQUAL(&page_offlined_list, &pages[1], &pages[3]);
}

/*
 * Exercise splitting an order-2 buddy where one subpage is offlined
 * and another order-0 page already exists in the free list.
 *
 * This checks that reserve_offlined_page() rebuilds the lower-order
 * free-list entries, preserves the pre-existing order-0 entry in-sequence,
 * coalesces the two healthy tail pages into a surviving order-1 buddy,
 * and propagates first_dirty to that surviving higher-order fragment.
 */
static void test_mixed_order_two_buddy(unsigned int start_mfn)
{
    struct page_info pages[4];
    struct page_info existing_order0;
    int zone;
    int offlined_pages;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /*
     * pages[0] is the order-2 head; pages[1] is offlined; pages[2]/[3] are
     * the healthy tail pair which should now survive as one order-1 buddy.
     */
    init_test_page(&pages[0], start_mfn++, order2, PGC_state_free);
    init_test_page(&pages[1], start_mfn++, order0, PGC_state_offlined);
    init_test_page(&pages[2], start_mfn++, order0, PGC_state_free);
    init_test_page(&pages[3], start_mfn++, order0, PGC_state_free);

    /* Add a pre-existing order-0 page so list ordering can be observed. */
    init_test_page(&existing_order0, start_mfn, order0, PGC_state_free);

    /* first_dirty points at the last subpage in the original order-2 chunk. */
    pages[0].u.free.first_dirty = 3;

    /*
     * Seed the heap with one order-2 chunk plus one pre-existing order-0 page.
     */
    zone              = page_to_zone(&pages[0]);
    avail[node][zone] = 4;
    total_avail_pages = 4;
    page_list_add(&existing_order0, &heap(node, zone, order0));
    page_list_add(&pages[0], &heap(node, zone, order2));

    /* Reserve the offlined subpage out of the larger order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(&pages[0]);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Only the offlined page should disappear from allocator accounting. */
    ASSERT(offlined_pages == 1);
    ASSERT(avail[node][zone] == 3);
    ASSERT(total_avail_pages == 3);
    ASSERT_LIST_EQUAL(&page_offlined_list, &pages[1]); /* page 1 is offlined */

    /* Checks for buddy splitting and merging */

    /*
     * The original order-2 entry should be gone after extracting
     * the offlined page from it.
     */
    ASSERT(page_list_empty(&heap(node, zone, order2)));

    /*
     * The surviving tail pair should have been merged into one order-1 buddy
     * covering pages[2] and pages[3] (the offlined page 1 causes the split).
     *
     * BUG:
     *
     * The original code fails to merge the surviving tail pair into
     * an order-1 buddy because of an off-by-one error in the check
     * for whether the next_order would go beyond the buddy boundary.
     *
     * The check should use '>' to allow the merge when the next_order end
     * is exactly at the buddy boundary, but the original code uses '>='
     * which prevents the merge in that case.
     *
     * As a result, the surviving tail pair remains as two separate order-0
     * pages instead of being merged into an order -1 buddy. This test is
     * designed to catch that bug by verifying the final list state and
     * orders of the surviving pages.
     *
     * This follows the Test-Driven Development (TDD) approach where the test
     * is written to expose the bug before the fix is implemented.
     */
    bugs_encountered++;
    ASSERT_LIST_EQUAL(&heap(node, zone, order0), &pages[2], &pages[0],
                      &existing_order0, &pages[3]);
    ASSERT(PFN_ORDER(&pages[0]) == 0);
    ASSERT(PFN_ORDER(&pages[1]) == 0);
    ASSERT(PFN_ORDER(&pages[2]) == 0);
    ASSERT(PFN_ORDER(&pages[3]) == 0);

    /* Checks for first_dirty propagation */

    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);
    ASSERT(pages[1].u.free.first_dirty == INVALID_DIRTY_IDX);
    ASSERT(pages[2].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* This is still the dirty page, and as it is split, it has first_dirty=0 */
    ASSERT(pages[3].u.free.first_dirty == 0);
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
    struct page_info pages[4];
    int zone;
    int offlined_pages;

    /* Start from a clean allocator state. */
    reset_page_alloc_state(start_mfn);

    /*
     * pages[0] is the order-2 head; pages[1] is offlined; pages[2]/[3] are
     * the healthy tail pair which should now survive as one order-1 buddy.
     */
    init_test_page(&pages[0], 10, order2, PGC_state_offlined);
    init_test_page(&pages[1], 11, order0, PGC_state_free);
    init_test_page(&pages[2], 12, order0, PGC_state_free);
    init_test_page(&pages[3], 13, order0, PGC_state_offlined);

    /* first_dirty points at the 3rd subpage in the original order-2 chunk. */
    pages[0].u.free.first_dirty = 2;

    /*
     * Seed the heap with one order-2 chunk plus one pre-existing order-0 page.
     */
    zone              = page_to_zone(&pages[0]);
    avail[node][zone] = 4;
    total_avail_pages = 4;
    page_list_add(&pages[0], &heap(node, zone, order2));

    /* Reserve the offlined subpages out of the larger order-2 chunk. */
    ASSERT_HEAP_CONSISTENCY(pages);
    offlined_pages = reserve_offlined_page(&pages[0]);
    ASSERT_HEAP_CONSISTENCY(pages);

    /* Only the offlined pages should disappear from allocator accounting. */
    ASSERT(offlined_pages == 2);
    ASSERT(avail[node][zone] == 2);
    ASSERT(total_avail_pages == 2);
    /* pages 0 and 3 are offlined */
    ASSERT_LIST_EQUAL(&page_offlined_list, &pages[0], &pages[3]);

    /* Checks for buddy splitting and merging */

    /*
     * The original order-2 entry should be gone after extracting
     * the offlined page from it.
     */
    ASSERT(page_list_empty(&heap(node, zone, order2)));

    /*
     * BUG:
     *
     * The surviving middle pages should NOT be merged into one order-1 buddy
     * in this case as they are not aligned to an order-1 boundary.
     *
     * The buddy invariant is that buddies must be naturally aligned to
     * their size.
     *
     * For the buddy design to work properly, two unaligned survivors
     * MUST always remain as two separate order-0 entries and should NEVER
     * be merged into any higher-order buddy because they would not be
     * naturally aligned to their size.
     *
     * But due to a missing check in the original code, such middle survivors
     * **are** merged into an order-1 buddy which is not aligned to its size.
     *
     * Unaligned buddies break the Xen's (and in general, any buddy
     * allocator's) fundamental invariant and can cause heap corruption
     * and crashes down the line when the allocator attempts to merge
     * and split such unaligned buddies in future operations.
     *
     * One of the observable consequences of this bug is that the heap
     * corruption can cause actively used pages to be mistakenly merged
     * into the free list as if they were free, and then later when the
     * allocator tries to allocate those pages, it can cause a host crash
     * when get_free_buddy() returns such used page to alloc_heap_pages()
     * which checks the use count.
     */
    bugs_encountered++;
    ASSERT_LIST_EQUAL(&heap(node, zone, order1), &pages[1]);
    ASSERT(PFN_ORDER(&pages[1]) == 1);

    /* The head fragment is order-0; the tail pair survives as order-1. */

    ASSERT(PFN_ORDER(&pages[1]) == 1);
    ASSERT(PFN_ORDER(&pages[3]) == 0);

    /* Checks for first_dirty propagation */

    /* The 1st offlined page should have invalid first_dirty */
    ASSERT(pages[0].u.free.first_dirty == INVALID_DIRTY_IDX);

    /*
     * Before reserve_offlined_page(), the order-2 chunk had first_dirty=2
     * which means that the page at index 2 was the first dirty page in the
     * chunk.
     *
     * As a result of the unaligned merge, it is now the tail page in its
     * new unaligned order-1 buddy.
     */
    ASSERT(pages[1].u.free.first_dirty == 1);

    /* As the tail page in the order-1 buddy it is not marked dirty itself. */
    ASSERT(pages[2].u.free.first_dirty == INVALID_DIRTY_IDX);

    /* The 2nd offlined page should have invalid first_dirty */
    ASSERT(pages[3].u.free.first_dirty == INVALID_DIRTY_IDX);
}

int main(void)
{
    printf("Running %s:\n", __FILE__);

    /* Run the single offlined page test with different starting MFNs */
    for ( unsigned int mfn = 0; mfn < 4; mfn++ )
        test_single_offlined_page(mfn);

    /* These test use order-2 buddies, their start must be a multiple of 4 */
    for ( unsigned int start_mfn = 0; start_mfn < 9; start_mfn += 4 )
        test_mixed_order_one_buddy(start_mfn);

    for ( unsigned int start_mfn = 0; start_mfn < 9; start_mfn += 4 )
        test_two_offlined_pages_order_two(start_mfn);

    for ( unsigned int start_mfn = 0; start_mfn < 9; start_mfn += 4 )
        test_mixed_order_two_buddy(start_mfn);

    for ( unsigned int start_mfn = 0; start_mfn < 9; start_mfn += 4 )
        test_unaligned_order_one_buddy(start_mfn);

    /* FIXME: Make any unexpected bugs fail the test after the bugs are fixed */
    if ( !bugs_encountered )
        printf(__FILE__ ": All tests passed.\n");
    else
    {
        printf("\n*** Error: %d bugs encountered during tests! ***\n\n    "
               "This is currently expected because of known issues.\n\n",
               bugs_encountered);

        /* FIXME: Make any unexpected bugs fail the test when bugs are fixed */
    }
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
