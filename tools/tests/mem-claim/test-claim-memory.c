/*
 * test-claim-memory.c - Test xc_domain_claim_memory() API
 *
 * Tests for the xc_domain_claim_memory() API, which allows a domain to
 * claim a certain number of pages from a specific NUMA node or globally.
 *
 * Outstanding claims are tracked in physinfo.outstanding_pages and should be
 * released when the domain is destroyed.  These tests verify that claims are
 * reflected in physinfo, that they can be replaced atomically, and that
 * invalid claims are rejected.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <xen-tools/common-macros.h>

#include "mem-claim-lib.h"

static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"list", no_argument, NULL, 'l'},
    {"test", required_argument, NULL, 't'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0},
};

#include "claim-memory-allocations.h"
#include "claim-memory-arg-checks.h"

/*
 * Offline offline_pages globally, then verify that the global free-page count
 * dropped by at least that many relative to a fresh pre-offline snapshot.
 * The baseline free count is written to free_baseline_out so the caller can
 * use it for a final lib_check_claim() call after the offline.
 *
 * Returns  0  on success,
 *          1  if concurrent host activity made the result inconclusive
 *             (caller should re-online the pages and retry), or
 *         <0  on a hard error from lib_offline_global_memory().
 */
static int offline_and_check_free_pages(struct test_ctx *ctx,
                                        unsigned long offline_pages,
                                        uint64_t *mfns,
                                        unsigned long *free_baseline_out)
{
    xc_physinfo_t physinfo;
    unsigned long free_before, free_after;
    int rc;

    /*
     * Take a snapshot immediately before the offline so that concurrent host
     * activity between the claim and here does not skew the check.
     */
    lib_get_global_free_pages(ctx->env, &free_before);
    xc_physinfo(ctx->env->xch, &physinfo);
    lib_debugf(ctx,
               "C-16 pre-offline snapshot global_free=%lu outstanding=%" PRIu64,
               free_before, physinfo.outstanding_pages);

    lib_set_step(ctx, "offline more pages than spare to eat into the claim");
    rc = lib_offline_global_memory(ctx, ctx->domid, offline_pages, mfns);
    if ( rc )
        return rc;

    /*
     * We expect that as the offline reduces the free pages, the outstanding
     * claim count is also reduced to reflect the new effective claim.
     */
    lib_get_global_free_pages(ctx->env, &free_after);
    xc_physinfo(ctx->env->xch, &physinfo);
    lib_debugf(ctx,
               "C-16 after offlining global_free=%lu outstanding=%" PRIu64
               " expected_free=%lu",
               free_after, physinfo.outstanding_pages,
               free_before - offline_pages);

    *free_baseline_out = free_before;

    /*
     * When there is concurrent host activity the free count may not have
     * dropped far enough; return 1 so the caller can re-online and retry.
     */
    if ( free_after > free_before - offline_pages )
        return 1;

    return 0;
}

/*
 * C-16: When memory is offlined below the total claimed pages, claims must be
 * released to ensure (total_avail_pages - outstanding_claims) never wraps. The
 * result would cause claims to be ignored or if a BUG_ON triggers, a Xen panic.
 */
static int test_offline_memory_with_claims(struct test_ctx *ctx)
{
    /* The test claims all but a few spare pages */
    unsigned long spare_pages = 9UL;
    /* Then it offlines more pages than the spare pages to eat into the claim */
    unsigned long free_after_claim, offline_pages = spare_pages + 1;
    uint64_t mfns[offline_pages];

    for ( int try = 0, rc = 1; rc > 0 && try < 12; try++ )
    {
        lib_claim_all_on_host(ctx, ctx->domid, spare_pages);

        rc = offline_and_check_free_pages(ctx, offline_pages, mfns,
                                          &free_after_claim);

        /* Revert offlined pages to online state */
        for ( unsigned long i = 0; i < offline_pages; i++ )
            online_page(ctx, mfns[i]);

        if ( rc < 0 )
            return rc;

    } /* Inconclusive: retry. */

    return lib_check_claim(
        ctx, free_after_claim - offline_pages, 0,
        "check outstanding claim count after offlining memory");
}

/*
 * List of test cases.  The fixture iterates over this list to run tests.
 *
 * Tests are identified by their id (e.g. "C-1") and have a descriptive name
 * and a function pointer to the test implementation.
 */
/* Short helper to declare test cases more concisely. */
#define CASE(ID, NAME, FN)                       \
    {                                            \
        .id = (ID), .name = (NAME), .test = (FN) \
    }

static const struct test_case test_cases[] = {
    CASE("C-0", "basic_node_claim", test_basic_node_claim),
    CASE("C-1", "global_replace_after_alloc", test_global_replace_after_alloc),
    CASE("C-2", "node_replace_after_alloc", test_node_replace_after_alloc),
    CASE("C-3", "legacy_global_claim", test_legacy_global_claim),
    CASE("C-4", "move_claim_between_nodes", test_move_claim_between_nodes),
    CASE("C-5", "reject_non_present_node", test_reject_non_present_node),
    CASE("C-6", "reject_too_many_claims", test_reject_too_many_claims),
    CASE("C-7", "reject_node_gt_uint8_max", test_reject_node_gt_uint8_max),
    CASE("C-8", "reject_pages_gt_int32_max", test_reject_pages_gt_int32_max),
    CASE("C-9", "reject_nonzero_pad", test_reject_nonzero_pad),
    CASE("C-10", "reject_zero_claim_count", test_reject_zero_claim_count),
    CASE("C-11", "null_claims_nonzero_count", test_null_claims_nonzero_count),
    CASE("C-12", "zero_count_with_pointer", test_zero_count_valid_pointer),
    CASE("C-13", "claim_pages_gt_free_enomem", test_claim_pages_gt_free_enomem),
    CASE("C-14", "zero_claim_resets_claim", test_zero_claim_resets_claim),
    CASE("C-15", "zero_claim_memory_reset", test_zero_claim_memory_resets),
    CASE("C-16", "offline_memory_with_claims", test_offline_memory_with_claims),
};

static void usage(FILE *stream, const char *prog)
{
    fprintf(stream,
            "Usage: %s [OPTIONS]\n\n"
            "Dedicated xc_domain_claim_memory() tests.\n\n"
            "Options:\n"
            "  -l, --list         List available test IDs and exit\n"
            "  -t, --test ID      Run only the specified test ID (repeatable)\n"
            "  -v, --verbose      Print per-step progress\n"
            "  -h, --help         Show this help text\n",
            prog);
}

int main(int argc, char **argv)
{
    struct runtime_config cfg = {0};
    struct test_env env = {0};
    struct test_result results[ARRAY_SIZE(test_cases)] = {0};
    unsigned int passed = 0, failed = 0, skipped = 0;
    int opt;

    while ( (opt = getopt_long(argc, argv, "hlt:v", long_options, NULL)) != -1 )
    {
        switch ( opt )
        {
        case 'h':
            usage(stdout, argv[0]);
            return 0;

        case 'l':
            cfg.list_only = true;
            break;

        case 't':
            if ( cfg.nr_selected_ids >= ARRAY_SIZE(cfg.selected_ids) )
                errx(1, "too many --test selectors (max %zu)",
                     ARRAY_SIZE(cfg.selected_ids));
            cfg.selected_ids[cfg.nr_selected_ids++] = optarg;
            break;

        case 'v':
            cfg.verbose = true;
            break;

        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if ( cfg.list_only )
    {
        puts("Available tests:");
        for ( size_t i = 0; i < ARRAY_SIZE(test_cases); i++ )
            printf("  %s  %s\n", test_cases[i].id, test_cases[i].name);
        return 0;
    }

    printf("========= testcase program: test-claim-memory ==========\n");
    printf("selected=%zu\n", cfg.nr_selected_ids);

    lib_initialise_test_env(&env);

    for ( size_t i = 0; i < ARRAY_SIZE(test_cases); i++ )
    {
        if ( !test_is_selected(&cfg, &test_cases[i]) )
            continue;

        lib_run_one_test(&env, &cfg, &test_cases[i], &results[i]);

        printf("%s::%s[%s] %s (%.2f ms)\n", argv[0], results[i].test->id,
               results[i].params[0] ? results[i].params : "default",
               status_name(results[i].status), results[i].duration_ms);

        if ( results[i].status == TEST_FAILED )
            printf("    %s\n", results[i].details);
        else if ( results[i].status == TEST_SKIPPED )
            printf("    %s\n", results[i].details);
    }

    puts("================== short test summary info =================");
    for ( size_t i = 0; i < ARRAY_SIZE(test_cases); i++ )
    {
        if ( !results[i].test )
            continue;

        printf("%s %s %s\n", status_name(results[i].status),
               results[i].test->id, results[i].test->name);

        switch ( results[i].status )
        {
        case TEST_PASSED:
            passed++;
            break;
        case TEST_FAILED:
            failed++;
            printf("    %s\n", results[i].details);
            break;
        case TEST_SKIPPED:
            skipped++;
            printf("    %s\n", results[i].details);
            break;
        }
    }

    printf("============ %u passed, %u failed, %u skipped ============\n",
           passed, failed, skipped);

    lib_release_test_env(&env);
    return failed ? 1 : 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
