/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Functional system test suite for testing memory claims in Xen.
 *
 * It is designed to test the xc_domain_claim_memory() API and
 * to reconfirm the xc_domain_claim_pages() API and interacts
 * with the running Xen hypervisor in Dom0 using libxenctrl.
 *
 * The verifications performed by the test cases include:
 *
 * - Validating that claims can be successfully made with valid parameters
 *   and that they have the expected effects on the system's memory state,
 *   such as increasing the number of outstanding claimed pages.
 *
 * - Validating that invalid claim attempts are rejected with the expected
 *   error codes, such as EINVAL for invalid parameters or ENOMEM when
 *   claiming more pages than are free.
 *
 * - Validating the effects of memory claims on the system, such as blocking
 *   effects when claiming more pages than are free or left unclaimed by
 *   other domains, and the guarantees provided by claims such as reserved
 *   claimed pages not being allocated to other domains.
 *
 * For the need to perform these verifications, the test cases interact
 * with the Xen hypervisor to query the system's memory state, create and
 * destroy test domains, perform claim operations, and populate memory to
 * test the blocking effects of claims.
 *
 * As the act of testing the blocking effects of claims involves allocating
 * memory from the system, other operations that interact with the system's
 * memory state should be avoided or kept to a minimum during the test run
 * to avoid interference with the test results.
 *
 * During these interactions, the test cases record successes and failures
 * with detailed messages that include the current step, test parameters,
 * and a snapshot of relevant memory state to aid in diagnosing issues
 * when a test fails.
 *
 * The test suite also ensures that domains are destroyed after tests to
 * clean up claims and leave the system in a clean state, even if a test
 * fails partway through.
 *
 * Some test cases that require multiple NUMA nodes can be skipped if the
 * system does not have a 2nd NUMA node, allowing the test suite to be run
 * on single-node systems as well.
 *
 * It is designed to run on a quiet system as it stakes claims on the system's
 * memory and verifies their effects, by allocating against the running system
 * Xen hypervisor in Dom0 using libxenctrl.
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
#define CASE(ID, NAME, FN)                           \
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
    struct test_result results[ARRAY_SIZE(cases)] = {};
    struct runtime_config cfg = {};
    struct test_env env = {};
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
