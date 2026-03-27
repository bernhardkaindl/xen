/* SPDX-License-Identifier: GPL-2.0-only */
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
 */
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <xen-tools/common-macros.h>

#include "mem-claim-lib.h"
#include "claim-memory-allocations.h"
#include "claim-memory-arg-checks.h"

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

static const struct test_case cases[] = {
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
};

/* Test entry point */
int main(int argc, char **argv)
{
    struct runtime_config cfg = {0};
    struct test_env env = {0};
    struct test_result results[ARRAY_SIZE(cases)] = {0};
    int retval;

    retval = lib_parse_args(argc, argv, &cfg);
    if ( cfg.list_only )
        return lib_print_available_tests(cases, ARRAY_SIZE(cases));
    if ( !retval )
    {
        lib_initialise_test_env(&env);
        lib_run_tests(&env, argv[0], &cfg, cases, ARRAY_SIZE(cases), results);
        retval = lib_summary(results, ARRAY_SIZE(results));
        lib_release_test_env(&env);
    }
    return retval ? EXIT_FAILURE : EXIT_SUCCESS;
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
