/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around xen/common/page_alloc.c for the allocator test framework.
 *
 * The test framework includes the real page_alloc.c directly in its
 * translation unit, together with mocks for the Xen types and functions it
 * uses and helper code for NUMA heap initialisation and heap-state checks.
 *
 * This file provides the definitions needed for that setup. It also wraps
 * selected page_alloc.c entry points, such as mark_page_offline() and
 * offline_page(), so test scenarios can log allocator actions and resulting
 * state during execution.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_ALLOC_PAGE_ALLOC_WRAPPER_H
#define TOOLS_TESTS_ALLOC_PAGE_ALLOC_WRAPPER_H

#define TEST_USES_PAGE_ALLOC_SHIM
#include "page-alloc-shim.h"

/* Include the real page_alloc.c for testing */

#pragma GCC diagnostic push
/* TODO: We should fix the remaining sign-compare warnings in page_alloc.c */
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
/*
 * Instrumenting the BUG() macro asserting to hit it means it is no longer
 * noreturn, and a function expects it to be noreturn, so disable this warning
 */
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "../../xen/common/page_alloc.c"
#pragma GCC diagnostic pop

/* Allow the logging spinlock mocks to identify the allocator heap lock. */
static spinlock_t *heap_lock_ptr = &heap_lock;

/* Backing storage for the synthetic allocator state used by the tests. */
#ifndef PAGES_PER_ZONE
#define PAGES_PER_ZONE 8
#endif

#ifndef MAX_PAGES
#define MAX_PAGES (MAX_NUMNODES * NR_ZONES * PAGES_PER_ZONE)
#endif

/*
 * The synthetic frame table backs the page_info entries used by the tests.
 * It is indexed by MFN so helper code and the imported allocator can
 * translate directly between MFNs and page_info pointers.
 */
struct page_info frame_table[MAX_PAGES];

/* Convenience pointer used by test scenarios. */
static struct page_info *test_pages = frame_table;

#define TOTAL_CLAIMS ((unsigned long)outstanding_claims)
#define FREE_PAGES \
        avail_heap_pages(MEMZONE_XEN + 1, NR_ZONES - 1, -1)

#define DOM_GLOBAL_CLAIMS(d)  ((d)->global_claims)
#define DOM_NODE_CLAIMS(d, n) ((d)->claims[n])

static const char *offline_state_name(uint32_t offline_status)
{
    switch ( offline_status )
    {
    case PG_OFFLINE_FAILED:
        return "PG_OFFLINE_FAILED";
    case PG_OFFLINE_PENDING:
        return "PG_OFFLINE_PENDING";
    case PG_OFFLINE_OFFLINED:
        return "PG_OFFLINE_OFFLINED";
    case PG_OFFLINE_AGAIN:
        return "PG_OFFLINE_AGAIN";
    default:
        return "PG_OFFLINE_UNKNOWN_STATUS";
    }
}

static struct page_info *__used test_alloc_domheap_pages(struct domain *dom,
                                                         unsigned int order,
                                                         unsigned int memflags,
                                                         const char *caller)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    testcase_assert_verbose_assertions = false;
    printf("%s => alloc_domheap_pages(dom=%u, order=%u, memflags=%x)\n",
           caller, dom->domain_id, order, memflags);
    testcase_assert_current_func = "alloc_domheap_pages";
    testcase_assert_verbose_indent_level++;
    struct page_info *pg = alloc_domheap_pages(dom, order, memflags);
    testcase_assert_verbose_indent_level--;
    testcase_assert_current_func = NULL;

    testcase_assert_verbose_assertions = save_verbose_asserts;
    printf("%s <= alloc_domheap_pages() = pg MFN %lu\n", caller,
           page_to_mfn(pg));
    return pg;
}
#define alloc_domheap_pages(dom, order, memflags) \
        test_alloc_domheap_pages(dom, order, memflags, __func__)

static void test_free_heap_pages(struct page_info *pg, unsigned int order,
                                 bool need_scrub, const char *caller)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    testcase_assert_verbose_assertions = false;
    printf("%s => free_heap_pages(pg MFN %lu, order=%u, need_scrub=%d)\n",
           caller, page_to_mfn(pg), order, need_scrub);
    free_heap_pages(pg, order, need_scrub);
    testcase_assert_verbose_assertions = save_verbose_asserts;
    printf("%s <= free_heap_pages() completed\n", caller);
}
#define free_heap_pages(pg, order, need_scrub) \
        test_free_heap_pages(pg, order, need_scrub, __func__)

static void __used test_free_domheap_pages(struct page_info *pg,
                                           unsigned int order,
                                           const char *caller)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    /* Avoid logging verbose logging while freeing domheap pages */
    testcase_assert_verbose_assertions = false;
    printf("%s => free_domheap_pages(pg MFN %lu, order=%u)\n", caller,
           page_to_mfn(pg), order);
    free_domheap_pages(pg, order);
    testcase_assert_verbose_assertions = save_verbose_asserts;
    printf("%s <= free_domheap_pages() completed\n", caller);
}
#define free_domheap_pages(pg, order) test_free_domheap_pages(pg, order, \
                                                              __func__)

static int __used test_offline_page(mfn_t mfn, int broken, uint32_t *status)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    testcase_assert_verbose_assertions = false;
    printf("%s => Offlining page at MFN %lu with broken=%d\n", __func__, mfn,
           broken);

    int ret = offline_page(mfn, broken, status);

    printf("%s <= offline_page() returned %d, status=0x%x (%s)\n", __func__,
           ret, *status, offline_state_name(*status));
    testcase_assert_verbose_assertions = save_verbose_asserts;
    return ret;
}
#define offline_page(mfn, broken, status) test_offline_page(mfn, broken, status)
#endif
