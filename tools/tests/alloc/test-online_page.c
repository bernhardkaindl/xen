/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Initial unit test for online_page() in common/page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

/* Sysctl support in page_alloc.c needed for online_page() */
#define CONFIG_SYSCTL
#include "libtest-page_alloc.h"

static void test_online_offlined_clean_page(int start_mfn)
{
    struct page_info *page = test_pages + start_mfn;
    uint32_t status = 0;
    int ret;

    /*
     * PREPARE:
     */

    /* Seed a single order-0 buddy onto the heap */
    test_page_list_add_buddy(page, order0);

    /* Set it as clean and offline it  */
    page->count_info &= ~PGC_need_scrub;
    page->u.free.first_dirty = INVALID_DIRTY_IDX;
    mark_page_offline(page, 0);
    reserve_offlined_page(page);
    CHECK_BUDDY(page, "Clean offline Page prepared");

    /*
     * ACT:
     */
    ret = online_page(page_to_mfn(page), &status);

    /*
     * ASSERT:
     */
    CHECK(ret == 0, "online_page should succeed");
    ASSERT_LIST_EMPTY(&page_offlined_list);
    CHECK(status == PG_ONLINE_ONLINED, "Page should be onlined");
    CHECK(page_state_is(page, free), "Page should be in free state");
    CHECK_BUDDY(page, "Page onlined again and is still clean, check its flags");

    CHECK(!(page->count_info & PGC_need_scrub), "Page should still be clean");
    CHECK(page->u.free.first_dirty == INVALID_DIRTY_IDX, "first_dirty invalid");
}

static void test_online_offlined_dirty_page(int start_mfn)
{
    struct page_info *page = test_pages + start_mfn;
    uint32_t status = 0;
    int ret;

    /*
     * PREPARE:
     */

    /* Seed a single order-0 buddy onto the heap */
    test_page_list_add_buddy(page, order0);

    /* Set it as dirty and offline it  */
    page->u.free.first_dirty = 0;
    page->count_info |= PGC_need_scrub;
    mark_page_offline(page, 0);
    reserve_offlined_page(page);
    CHECK_BUDDY(page, "Prepared a dirty, offlined page");

    /*
     * ACT:
     */
    ret = online_page(page_to_mfn(page), &status);

    /*
     * ASSERT:
     */
    CHECK(ret == 0, "online_page should succeed");
    ASSERT_LIST_EMPTY(&page_offlined_list);
    CHECK(status == PG_ONLINE_ONLINED, "Page should be onlined");
    CHECK(page_state_is(page, free), "Page should be in free state");
    CHECK_BUDDY(page, "Page onlined again, but check its dirty status");

    /*
     * PGC_need_scrub should still be set and first_dirty field should again
     * refer to the page itself to match it after being onlined again.
     */
    CHECK(page->count_info & PGC_need_scrub, "Page should still be dirty");
    CHECK(page->u.free.first_dirty == 0, "first_dirty should refer to itself");
}

int main(int argc, char *argv[])
{
    const char *topic = "Test offlining pages with reserve_offline_page()";
    const char *program_name = parse_args(argc, argv, topic);

    if ( !program_name )
        return EXIT_FAILURE;

    init_page_alloc_tests();

    RUN_TESTCASE(OOCP, test_online_offlined_clean_page, 1);
    RUN_TESTCASE(OODP, test_online_offlined_dirty_page, 4);

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
