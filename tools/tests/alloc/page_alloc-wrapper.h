/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test framework for testing Xen's memory-allocation functionality.
 *
 * This file wraps xen/common/page_alloc.c for the test framework.
 *
 * Context:
 *
 * The test framework includes the real page_alloc.c directly into its
 * translation unit, along with mocks for the Xen types and functions
 * used by page_alloc.c, and a library for NUMA heap initialisation
 * and asserting the heap status.
 *
 * This file serves as the wrapper around page_alloc.c, providing the
 * necessary definitions and helpers to allow the test framework to
 * include the real page_alloc.c directly into its translation unit.
 *
 * It also provides wrapper functions for key page_alloc.c functions like
 * mark_page_offline() and offline_page() to allow the test scenarios to
 * log the actions being taken and the outcomes observed when these functions
 * are called, which is important for understanding the behavior of the
 * allocator during the test scenarios and for debugging any issues that arise.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#include <stdarg.h>
#include <string.h>

#define TEST_USES_PAGE_ALLOC_SHIM
#include "page_alloc_shim.h"

/* Include the real page_alloc.c for testing */

#pragma GCC diagnostic push
/* TODO: We should fix the remaining sign-compare warnings in page_alloc.c */
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../../xen/common/page_alloc.c"
#pragma GCC diagnostic pop

/* Allows the logging spinlock/unlock mocks to identify the heap lock */
static spinlock_t *heap_lock_ptr = &heap_lock;

/*
 * Global state for the test page allocator shim and helpers.
 *
 * This includes the heap storage and availability counters that the test
 * scenarios manipulate, as well as the domain list and a bug counter for
 * the test program to track any unexpected conditions encountered in the
 * test helpers.
 */
#ifndef PAGES_PER_ZONE
#define PAGES_PER_ZONE 8
#endif

#ifndef MAX_PAGES
#define MAX_PAGES (MAX_NUMNODES * NR_ZONES * PAGES_PER_ZONE)
#endif

/*
 * The test frame table serves as the backing storage for the page_info
 * structures used in the test scenarios, and the page_info structures
 * are indexed by MFN for easy translation between page_info pointers and
 * MFNs in the test helpers and assertions.
 *
 * The frame table is the foundation for the buddy allocator algorithm
 * implemented by page_alloc.c, and the test scenarios manipulate the
 * state of the page_info structures in the frame table to set up test
 * conditions and verify the behavior of the allocator.
 */
struct page_info frame_table[MAX_PAGES];

/* Provide a test pages pointer for the test scenarios */
static struct page_info *test_pages = frame_table;

/*
 * Global state for the test page allocator shim and helpers.
 *
 * This includes the heap storage and availability counters that the test
 * scenarios manipulate, as well as the domain list and a bug counter for
 * the test program to track any unexpected conditions encountered in the
 * test helpers.
 */
static heap_by_zone_and_order_t test_heap_storage[MAX_NUMNODES];
static unsigned long test_avail_storage[MAX_NUMNODES][NR_ZONES];
struct domain *domain_list;
typedef size_t zone_t;

static int __used test_domain_install_claim_set(struct domain *d,
                                                unsigned int nr_claims,
                                                memory_claim_t *claim_set)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;
    char target_str[16];

    /* Avoid logging verbose logging while marking a page offline */
    testcase_assert_verbose_assertions = false;

    printf("%s => Installing claim set for domain %u:\n", __func__,
           d->domain_id);
    for ( unsigned int i = 0; i < nr_claims; i++ )
    {
        switch ( claim_set[i].target )
        {
        case XEN_DOMCTL_CLAIM_MEMORY_GLOBAL:
            snprintf(target_str, sizeof(target_str), "GLOBAL");
            break;
        case XEN_DOMCTL_CLAIM_MEMORY_LEGACY:
            snprintf(target_str, sizeof(target_str), "LEGACY");
            break;
        default:
            snprintf(target_str, sizeof(target_str), "NODE %u",
                     claim_set[i].target);
            break;
        }
        printf("  Claim %u: pages=%lu, target=%s\n", i, claim_set[i].pages,
               target_str);
    }

    int ret = domain_install_claim_set(d, nr_claims, claim_set);
    printf("%s <= domain_install_claim_set() returned %d\n", __func__, ret);
    testcase_assert_verbose_assertions = save_verbose_asserts;
    return ret;
}
#define domain_install_claim_set(d, nr_claims, claim_set) \
        test_domain_install_claim_set(d, nr_claims, claim_set)

static unsigned long __used test_mark_page_offline(struct page_info *page,
                                                   int flag,
                                                   const char *caller_func)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    /* Avoid logging verbose logging while marking a page offline */
    testcase_assert_verbose_assertions = false;

    printf("%s => Marking page at MFN %lu as %s.\n", caller_func,
           page_to_mfn(page), flag ? "broken" : "offlined");

    mark_page_offline(page, flag);

    testcase_assert_verbose_assertions = save_verbose_asserts;
    return 0;
}
#define mark_page_offline(pg, flag) test_mark_page_offline(pg, flag, __func__)

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
    return 0;
}
#define offline_page(mfn, broken, status) test_offline_page(mfn, broken, status)

static int __used test_set_outstanding_pages(struct domain *dom,
                                             unsigned long pages,
                                             const char *caller_func)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    /* Avoid logging verbose logging while setting outstanding claims */
    testcase_assert_verbose_assertions = false;
    printf("%s => domain_set_outstanding_pages(dom=%u, pages=%lu)\n",
           caller_func, dom->domain_id, pages);

    int ret = domain_set_outstanding_pages(dom, pages);

    printf("%s <= domain_set_outstanding_pages() = %d\n", caller_func, ret);
    testcase_assert_current_func = NULL;
    testcase_assert_verbose_assertions = save_verbose_asserts;
    return ret;
}
#define domain_set_outstanding_pages(dom, pages) \
        test_set_outstanding_pages(dom, pages, __func__)

static struct page_info *__used test_alloc_domheap(struct domain *dom,
                                                   unsigned int order,
                                                   unsigned int memflags,
                                                   const char *caller_func)
{
    bool save_verbose_asserts = testcase_assert_verbose_assertions;

    /* Avoid logging verbose logging while allocating domheap pages */
    testcase_assert_verbose_assertions = false;
    printf("%s => alloc_domheap_pages(dom=%u, order=%u, memflags=%x)\n",
           caller_func, dom->domain_id, order, memflags);
    testcase_assert_current_func = "alloc_domheap_pages";
    testcase_assert_verbose_indent_level++;
    struct page_info *pg = alloc_domheap_pages(dom, order, memflags);
    testcase_assert_verbose_indent_level--;
    testcase_assert_current_func = NULL;

    testcase_assert_verbose_assertions = save_verbose_asserts;
    return pg;
}
#define alloc_domheap_pages(dom, order, memflags) \
        test_alloc_domheap(dom, order, memflags, __func__)

#ifdef CONFIG_SYSCTL
/* Helper for just getting the number of free pages for ASSERTs */
static uint64_t __used free_pages(void)
{
    uint64_t free_pages, total_claims;
    bool verbose_asserts_save = testcase_assert_verbose_assertions;

    /* Avoid logging spinlock actions while getting the free page count */
    testcase_assert_verbose_assertions = false;
    get_outstanding_claims(&free_pages, &total_claims);
    testcase_assert_verbose_assertions = verbose_asserts_save;
    return free_pages;
}
#define FREE_PAGES free_pages()

/* Helper for just getting the total number of claimed pages for ASSERTs */
static uint64_t __used total_claims(void)
{
    uint64_t free_pages, total_claims;
    bool verbose_asserts_save = testcase_assert_verbose_assertions;

    /* Avoid logging spinlock actions while getting the total claims */
    testcase_assert_verbose_assertions = false;
    get_outstanding_claims(&free_pages, &total_claims);
    testcase_assert_verbose_assertions = verbose_asserts_save;
    return total_claims;
}
#define TOTAL_CLAIMS total_claims()

#define DOM_GLOBAL_CLAIMS(d)  ((d)->global_claims)
#define DOM_NODE_CLAIMS(d, n) ((d)->claims[n])

#endif /* CONFIG_SYSCTL */

static void print_order_list(nodeid_t node, zone_t zone, size_t order)
{
    struct page_info *pos = page_list_first(&heap(node, zone, order));

    if ( pos )
        printf("    Heap for zone %zu, order %zu:\n", zone, order);

    while ( pos )
    {
        size_t page_order = PFN_ORDER(pos);

        print_page_info(pos);
        /* Print the subpages of the buddy head */
        for ( size_t sub_pg = 1; sub_pg < (1U << page_order); sub_pg++ )
        {
            struct page_info *sub_pos = pos + sub_pg;

            printf("  ");
            print_page_info(sub_pos);
            /* Assert the subpages of a buddy to have order-0. */
            ASSERT(PFN_ORDER(sub_pos) == 0);
        }
        /* Assert that the page_order matches the heap order. */
        if ( page_order != order )
        {
            printf("ERROR:mfn %lu has order %zu but expected %zu "
                   "based on heap position\n",
                   page_to_mfn(pos), page_order, order);
            ASSERT(page_order == order);
        }
        pos = pos->list_next;
    }
}

#define CHECK_BUDDY(pages, fmt, ...) \
        check_buddy(pages, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* Function to print the order and first_dirty of each page for debugging. */
static void check_buddy(struct page_info *pages, const char *file, int line,
                        const char *fmt, ...)
{
    size_t size = 1U << PFN_ORDER(pages);
    bool verbose_asserts_save = testcase_assert_verbose_assertions;
    va_list args;

    if ( fmt ) /* Print the given message for context in the logs.*/
    {
        va_start(args, fmt);
        printf("  %s:%d: ", file, line);
        vprintf(fmt, args);
        puts(":");
        va_end(args);
    }
    else
        printf("  %s:%d: %s():\n", file, line, __func__);

    /* Avoid logging internal assertions while logging the free list status */
    testcase_assert_verbose_assertions = false;

    /*
     * Inside pages, first_dirty must (if not INVALID_DIRTY_IDX) index the
     * (first) page itself or a subpage within the page's range (<= 2^order).
     */
    for ( size_t i = 0; i < size; i++ )
    {
        unsigned long first_dirty = pages[i].u.free.first_dirty;
        unsigned int tail_offset = (1U << PFN_ORDER(&pages[i])) - 1;

        if ( first_dirty != INVALID_DIRTY_IDX && first_dirty > tail_offset )
        {
            printf("page at index %zu has first_dirty %lx but expected <= %u "
                   "based on its order\n",
                   i, first_dirty, tail_offset);
            ASSERT(pages[i].u.free.first_dirty == tail_offset);
        }
    }

    /* Traverse the offlined list, print and assert errors in it. */
    struct page_info *pos = page_list_first(&page_offlined_list);
    if ( pos )
        puts("    Offlined list:");
    while ( pos )
    {
        print_and_assert_offlined_page(pos);
        pos = pos->list_next;
    }

    /* Traverse the broken list, print and assert errors in it. */
    pos = page_list_first(&page_broken_list);
    if ( pos )
        puts("    Broken list:");
    while ( pos )
    {
        print_and_assert_offlined_page(pos);
        pos = pos->list_next;
    }

    /*
     * Traverse the _heap[node] for each order and zone and print and assert
     * the order and first_dirty of each page for each heap for debugging.
     *
     * This is to help verify that the heap structure is consistent with the
     * page_info order fields after operations that manipulate both, such as
     * reserve_offlined_page().
     */
    for ( nodeid_t node = 0; node < MAX_NUMNODES; node++ )
        for ( size_t order = 0; order <= MAX_ORDER; order++ )
            for ( size_t zone = 0; zone < NR_ZONES; zone++ )
                print_order_list(node, zone, order);
    testcase_assert_verbose_assertions = verbose_asserts_save;
}

/*
 * Failure reporting helper that prints the provided message, the test
 * caller context and a native backtrace before aborting.
 */
static void fail_with_ctx(const char *caller_file, const char *caller_func,
                          int caller_line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "\n- %s: Assertion failed: ", caller_func);
    va_start(ap, fmt);
    testcase_assert(false, caller_file, caller_line, caller_func, fmt, ap);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/*
 * Assert that a page_list matches the provided sequence of page pointers.
 *
 * The public helper below is a macro so call sites can provide a simple list
 * of page pointers while the implementation works over an ordinary array.
 */
static void __used assert_list_eq_array(struct page_list_head *list,
                                        struct page_info *const expected[],
                                        unsigned int nr_expected,
                                        const char *call_file,
                                        const char *caller_func,
                                        int caller_line)
{
    struct page_info *pos;
    int fails_before = testcase_assert_expected_failures;
    unsigned int index = 0;

    if ( list->count != nr_expected )
        fail_with_ctx(call_file, caller_func, caller_line,
                      "list count mismatch: expected %u, got %u", nr_expected,
                      list->count);

    if ( nr_expected == 0 )
    {
        if ( page_list_first(list) != NULL )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "expected empty list but head != NULL");
        else
            testcase_assert_successful_assert_total++;
        return;
    }

    if ( page_list_first(list) != expected[0] )
        fail_with_ctx(call_file, caller_func, caller_line,
                      "list head mismatch: expected %p, got %p", expected[0],
                      page_list_first(list));

    for ( pos = page_list_first(list); pos; pos = pos->list_next, index++ )
    {
        if ( index >= nr_expected )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list contains more elements than expected");

        if ( pos != expected[index] )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "element %u mismatch: expected %p, got %p", index,
                          expected[index], pos);

        if ( pos->list_prev != (index ? expected[index - 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_prev mismatch at index %u", index);

        if ( pos->list_next !=
             (index + 1 < nr_expected ? expected[index + 1] : NULL) )
            fail_with_ctx(call_file, caller_func, caller_line,
                          "list_next mismatch at index %u", index);
    }

    if ( index != nr_expected )
        fail_with_ctx(
            call_file, caller_func, caller_line,
            "list element count consumed mismatch: expected %u, got %u",
            nr_expected, index);

    if ( testcase_assert_expected_failures == fails_before )
        testcase_assert_successful_assert_total++;
}

/** Assert that a page_list matches the provided sequence of page pointers. */
#define ASSERT_LIST_EQUAL(list, ...)                                     \
        do                                                               \
        {                                                                \
            struct page_info *const expected[] = {__VA_ARGS__};          \
            assert_list_eq_array((list), expected, ARRAY_SIZE(expected), \
                                 __FILE__,                               \
                                 __func__, __LINE__);                    \
        } while ( 0 )
/** Assert that a page_list is empty. */
#define ASSERT_LIST_EMPTY(list) ASSERT(page_list_empty(list))
