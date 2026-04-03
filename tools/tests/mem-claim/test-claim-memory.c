/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * test-claim-memory.c - Test xc_domain_claim_memory() API
 *
 * Tests for the xc_domain_claim_memory() API, which allows a domain to
 * claim a certain number of pages from a specific NUMA node or globally.
 */
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <xen-tools/common-macros.h>

#include "libtestclaims.h"
#include "accounting-1.h"
#include "input-phase1.h"
#include "input-phase2.h"

/* Short helper to declare test cases more concisely. */
#define CASE(ID, NAME, FN)                       \
    {                                            \
        .id = (ID), .name = (NAME), .test = (FN) \
    }

/*
 * List of test cases.  lib_run_tests() iterates over this list to run tests.
 *
 * Tests are identified by their id (e.g. "A1-1") and have a descriptive name
 * and a function pointer to the test implementation.
 */
static const struct test_case cases[] = {
    CASE("A1-1", "basic_node_claim", test_basic_node_claim),
    CASE("A1-2", "global_replace_after_alloc", test_global_replace_after_alloc),
    CASE("A1-3", "node_replace_after_alloc", test_node_replace_after_alloc),
    CASE("A1-4", "legacy_global_claim", test_legacy_global_claim),
    CASE("A1-5", "move_claim_between_nodes", test_move_claim_between_nodes),
    CASE("A1-6", "zero_claim_resets_claim", test_zero_claim_resets_claim),
    CASE("A1-7", "zero_claim_memory_reset", test_zero_claim_memory_resets),
    CASE("I1-1", "reject_non_present_node", test_reject_non_present_node),
    CASE("I1-2", "reject_too_many_claims", test_reject_too_many_claims),
    CASE("I1-3", "reject_node_gt_uint8_max", test_reject_node_gt_uint8_max),
    CASE("I1-4", "reject_pages_gt_int32_max", test_reject_pages_gt_int32_max),
    CASE("I1-5", "reject_nonzero_pad", test_reject_nonzero_pad),
    CASE("I1-6", "reject_zero_claim_count", test_reject_zero_claim_count),
    CASE("I1-7", "null_claims_nonzero_count", test_null_claims_nonzero_count),
    CASE("I1-8", "zero_count_with_pointer", test_zero_count_valid_pointer),
    CASE("I1-9", "claim_pages_gt_free_enomem", test_claim_pages_gt_free_enomem),
    CASE("I2-1", "claim_pages_causes_enomem", test_claim_pages_causes_enomem),
    CASE("I2-2", "claim_memory_causes_enomem", test_claim_memory_causes_enomem),
    CASE("I2-3", "claim_prima_causes_enomem", test_claim_prima_causes_enomem),
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
