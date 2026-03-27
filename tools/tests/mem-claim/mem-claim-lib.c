/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * mem-claim-lib.c - Library of helper functions for memory claim tests
 *
 * This library provides common functions for managing test state, recording
 * failures with detailed diagnostics, querying memory state, performing claim
 * operations.
 *
 * The intent is to keep the test cases in the actual tests focused on
 * the specific claim scenarios being tested, while this library handles
 * the common mechanics of interacting with Xen and recording results.
 *
 * This includes:
 *
 * - Managing the test_ctx structure which holds the test environment,
 *   configuration, and results.
 *
 * - Providing helper functions to:
 *   - Create and destroy domains for testing, which are needed to make claims
 *   - Query the system's memory state in terms of free pages and outstanding
 *     claims, which are used for sizing claims and verifying their effects.
 *   - Perform claim operations and check their effects on the system.
 *   - Populate memory to test the blocking effects of claims.
 *   - Record failures with detailed messages that include the current step,
 *     test parameters, and a snapshot of relevant memory state.
 *
 * - Cleanup the test environment by destroying domains after tests, ensuring
 *   that claims are released and the system is left in a clean state even
 *   if a test fails partway through.
 *
 * - Providing a consistent way to skip tests when preconditions are not met,
 *   such as insufficient free memory or lack of multiple NUMA nodes.
 *
 * - Ensuring that all interactions with Xen are checked for errors, and that
 *   any failures are reported with detailed diagnostics.
 *
 * - Test cases should use the provided helper functions to perform all
 *   operations that interact with Xen or manage test state to ensure
 *   consistent failure reporting and cleanup.
 *
 * It is used by test-claim-memory.c to implement test cases for the
 * xc_domain_claim_memory() API and validate their effects on the system.
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xen-tools/common-macros.h>

#include "mem-claim-lib.h"

int rc;
static int step;
static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"list", no_argument, NULL, 'l'},
    {"test", required_argument, NULL, 't'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0},
};

/* --- diagnostics helpers --- */

/* Append formatted text to a buffer, ensuring it is always null-terminated. */
void lib_appendf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    size_t used = strlen(buf);

    if ( used >= size )
        return;

    va_start(ap, fmt);
    vsnprintf(buf + used, size - used, fmt, ap);
    va_end(ap);
}

/* Print debug information if verbose mode is enabled. */
void lib_debugf(struct test_ctx *ctx, const char *fmt, ...)
{
    va_list ap;

    if ( !ctx->cfg->verbose )
        return;

    fputs("      debug: ", stdout);

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fputc('\n', stdout);
}

/*
 * Set the current test step description, which is included in failure reports.
 * If verbose mode is enabled, also print the step to stdout.
 */
void lib_set_step(struct test_ctx *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(ctx->step, sizeof(ctx->step), fmt, ap);
    va_end(ap);

    if ( ctx->cfg->verbose )
        printf("      step %d: %s\n", ++step, ctx->step);
}

/*
 * Record a test failure with a formatted message and errno, and include the
 * current step, test parameters, and a snapshot of relevant memory state in
 * the details.
 */
static void append_snapshot(struct test_ctx *ctx)
{
    xc_physinfo_t physinfo;
    unsigned int nodes[2] = {ctx->node, ctx->other_node};

    xc_physinfo(ctx->env->xch, &physinfo);
    ctx_appendf(ctx,
                "\n    snapshot: free_pages=%" PRIu64
                ", outstanding_pages=%" PRIu64,
                physinfo.free_pages, physinfo.outstanding_pages);

    /* Include their free/total pages at the time of failure in the snapshot. */
    for ( size_t i = 0; i < ARRAY_SIZE(nodes); i++ )
    {
        unsigned long free_pages, total_pages;
        unsigned int node = nodes[i];

        if ( node == INVALID_NODE )
            continue;
        if ( i == 1 && node == nodes[0] )
            continue;

        lib_get_node_free_pages(ctx, node, &free_pages, &total_pages);
        ctx_appendf(ctx, "\n    snapshot: node%u free=%lu total=%lu", node,
                    free_pages, total_pages);
    }
}

/*
 * Record a test failure with a formatted message and the given errno.
 *
 * Include the current step, test parameters, and a snapshot of relevant
 * memory state in the details.
 */
int lib_fail_with_errno(struct test_ctx *ctx, int errnum, const char *fmt, ...)
{
    va_list ap;

    ctx->result->status = TEST_FAILED;
    ctx->result->details[0] = '\0';

    ctx_appendf(ctx, "step=%s", ctx->step[0] ? ctx->step : "(not set)");
    ctx_appendf(ctx, "\n    domid=%u helper_domid=%u node=%s other_node=%s",
                ctx->domid, ctx->helper_domid,
                ctx->node == INVALID_NODE ? "n/a" : "set",
                ctx->other_node == INVALID_NODE ? "n/a" : "set");

    if ( ctx->node != INVALID_NODE )
        ctx_appendf(ctx, " (%u)", ctx->node);
    if ( ctx->other_node != INVALID_NODE )
        ctx_appendf(ctx, " (%u)", ctx->other_node);

    ctx_appendf(ctx, "\n    alloc_pages=%lu", ctx->alloc_pages);

    ctx_appendf(ctx, "\n    cause: ");
    va_start(ap, fmt);
    vsnprintf(ctx->result->details + strlen(ctx->result->details),
              sizeof(ctx->result->details) - strlen(ctx->result->details), fmt,
              ap);
    va_end(ap);

    if ( errnum )
        ctx_appendf(ctx, "\n    errno=%d (%s)", errnum, strerror(errnum));

    append_snapshot(ctx);
    return -1;
}

/*
 * Record a test failure with a formatted message and the current errno.
 *
 * Calls lib_fail_with_errno() to do the actual recording, passing the current
 * errno.
 */
int lib_fail(struct test_ctx *ctx, const char *fmt, ...)
{
    va_list ap;
    int saved_errno = errno;
    char message[1024];

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    return lib_fail_with_errno(ctx, saved_errno, "%s", message);
}

/*
 * Record that a test was skipped with a formatted message.
 *
 * Include the message in the details to explain why the test was skipped.
 */
int lib_skip_test(struct test_ctx *ctx, const char *fmt, ...)
{
    va_list ap;

    ctx->result->status = TEST_SKIPPED;
    ctx->result->details[0] = '\0';

    va_start(ap, fmt);
    vsnprintf(ctx->result->details, sizeof(ctx->result->details), fmt, ap);
    va_end(ap);

    return 1;
}

/* --- memory-state queries --- */

/* Get the number of free and total pages for a specific NUMA node. */
int lib_get_node_free_pages(struct test_ctx *ctx, unsigned int node,
                            uint64_t *free_pages, uint64_t *total_pages)
{
    struct test_env *env = ctx->env;

    if ( node >= env->num_nodes ) /* Check node validity */
        return lib_fail(ctx, "Invalid node %u/%u", node, env->num_nodes);

    if ( xc_numainfo(env->xch, &env->num_nodes, env->meminfo, NULL) )
        return lib_fail(ctx, "xc_numainfo failed to get node memory info");

    *free_pages = env->meminfo[node].memfree / XC_PAGE_SIZE;
    if ( total_pages )
        *total_pages = env->meminfo[node].memsize / XC_PAGE_SIZE;
    return 0;
}

/* Get the total number of free pages available across all nodes. */
int lib_get_global_free_pages(struct test_ctx *ctx, uint64_t *free_pages)
{
    struct test_env *env = ctx->env;
    uint64_t free_bytes;

    if ( xc_availheap(env->xch, 0, 0, -1, &free_bytes) )
        return lib_fail(ctx, "xc_availheap failed to get global pages");

    *free_pages = free_bytes / XC_PAGE_SIZE;
    return 0;
}

/* Get the current number of outstanding pages. */
int lib_get_baseline_outstanding(struct test_ctx *ctx,
                                 uint64_t *baseline_outstanding)
{
    xc_physinfo_t physinfo;

    lib_set_step(ctx, "Query the number of outstanding claims on the system.");
    if ( xc_physinfo(ctx->env->xch, &physinfo) )
        return lib_fail(ctx, "xc_physinfo failed to get outstanding pages");
    *baseline_outstanding = physinfo.outstanding_pages;
    return 0;
}

/* --- claim check operations --- */

/* Check the current outstanding pages against the expected value. */
int lib_check_claim(struct test_ctx *ctx, uint64_t baseline_outstanding,
                    uint64_t expected_delta, const char *reason)
{
    xc_physinfo_t physinfo;
    uint64_t expected = baseline_outstanding + expected_delta;

    lib_set_step(ctx, "%s", reason);
    if ( xc_physinfo(ctx->env->xch, &physinfo) )
        return lib_fail(ctx, "xc_physinfo failed to get outstanding pages");

    if ( physinfo.outstanding_pages != expected )
        return lib_fail_with_errno(
            ctx, 0, "expected outstanding_pages=%" PRIu64 ", got %" PRIu64,
            expected, physinfo.outstanding_pages);
    return 0;
}

/* --- domain lifecycle --- */

/*
 * Create a domain with the specified configuration and label.
 * Record a failure if the creation or maxmem setting fails.
 *
 * On success, the new domain ID is stored in *domid.
 */
int lib_create_domain(struct test_ctx *ctx, uint32_t *domid, const char *label)
{
    struct xen_domctl_createdomain create = ctx->env->create_template;

    lib_set_step(ctx, "create %s domain", label);
    *domid = DOMID_INVALID;
    if ( xc_domain_create(ctx->env->xch, domid, &create) )
        return lib_fail(ctx, "xc_domain_create(%s) failed", label);

    lib_set_step(ctx, "set maxmem for %s domain", label);
    if ( xc_domain_setmaxmem(ctx->env->xch, *domid, -1) )
    {
        lib_destroy_domain(ctx, domid, label);
        return lib_fail(ctx, "xc_domain_setmaxmem(%s) failed", label);
    }

    return 0;
}

/*
 * Destroy the specified domain, if it is valid.
 * Add the destroy step with the given label to the current test description.
 * Record a failure if the destroy operation fails.
 *
 * This should be called during test cleanup to ensure domains are destroyed
 * and claims are released even if a test fails partway through.
 */
int lib_destroy_domain(struct test_ctx *ctx, uint32_t *domid, const char *label)
{
    if ( *domid == DOMID_INVALID )
        return 0;

    lib_set_step(ctx, "destroy %s domain", label);
    rc = xc_domain_destroy(ctx->env->xch, *domid);
    *domid = DOMID_INVALID;
    if ( rc )
    {
        if ( ctx->result->status == TEST_FAILED )
        {
            ctx_appendf(ctx,
                        "\n    cleanup: xc_domain_destroy(%s) failed: %d (%s)",
                        label, errno, strerror(errno));
            return -1;
        }

        return lib_fail(ctx, "xc_domain_destroy(%s) failed", label);
    }

    return 0;
}

/* --- claim operations --- */

/*
 * Attempt to claim memory with the specified parameters.
 * Record the failure if the claim operation fails.
 */
int lib_claim_memory(struct test_ctx *ctx, uint32_t domid, uint32_t nr_claims,
                     memory_claim_t *claims, const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_memory(ctx->env->xch, domid, nr_claims, claims);
    if ( rc )
    {
        unsigned long free_pages, total_pages;
        uint64_t outstanding_pages;

        lib_get_node_free_pages(ctx, ctx->node, &free_pages, &total_pages);
        lib_get_baseline_outstanding(ctx, &outstanding_pages);

        return lib_fail(ctx,
                        "xc_domain_claim_memory failed: node=%u "
                        "outstanding_pages=%lu free_pages=%lu total_pages=%lu",
                        ctx->node, outstanding_pages, free_pages, total_pages);
    }
    return rc;
}

/*
 * Attempt to claim memory with the specified parameters, expecting it to fail
 * with the specified errno. Record a failure if it does not fail as expected.
 */
int lib_expect_claim_memory_failure(struct test_ctx *ctx, uint32_t domid,
                                    uint32_t nr_claims, memory_claim_t *claims,
                                    int expected_errno, const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_memory(ctx->env->xch, domid, nr_claims, claims);
    if ( rc == -1 && errno == expected_errno )
        return 0;

    return lib_fail_with_errno(ctx, errno,
                               "expected xc_domain_claim_memory() to fail with "
                               "errno=%d (%s), got rc=%d",
                               expected_errno, strerror(expected_errno), rc);
}

/*
 * Release all claims for the specified domain by setting a global claim with
 * zero pages. Record the failure if the claim release operation fails.
 */
int lib_release_all_claims(struct test_ctx *ctx, uint32_t domid)
{
    memory_claim_t claim = {
        .pages = 0,
        .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL,
    };

    lib_set_step(ctx, "release all claims with global zero claim");
    rc = xc_domain_claim_memory(ctx->env->xch, domid, 1, &claim);
    if ( rc )
        return lib_fail(ctx, "xc_domain_claim_memory(..., global=0) failed");
    return 0;
}

/*
 * Claim all available memory on the host except for a specified number
 * of pages to spare. Record the failure if the claim operation fails.
 */
int lib_claim_all_on_host(struct test_ctx *ctx, uint32_t domid,
                          unsigned int spare)
{
    unsigned long free_pages;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx, &free_pages);

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim all pages except %u pages on host claim=%lu free=%lu",
             spare, free_pages - spare, free_pages);

    return lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = free_pages - spare,
                          .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        ctx->result->params);
}

/*
 * Claim all available memory on the specified node except for a specified
 * number of pages to spare. Record the failure if the claim operation fails.
 */
int lib_claim_all_on_node(struct test_ctx *ctx, uint32_t domid,
                          unsigned int node, unsigned int spare)
{
    unsigned long free_pages, total_pages;

    lib_get_node_free_pages(ctx, node, &free_pages, &total_pages);

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim all pages except %u pages on node=%u claim=%lu total=%lu",
             spare, node, free_pages, total_pages);

    return lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = free_pages - spare, .node = node},
        ctx->result->params);
}

/*
 * Attempt to claim memory with the legacy xc_domain_claim_pages() API.
 * Record the failure if the claim operation fails.
 */
int lib_claim_pages_legacy(struct test_ctx *ctx, uint32_t domid,
                           unsigned long nr_pages, const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_pages(ctx->env->xch, domid, nr_pages);
    if ( rc )
        return lib_fail(ctx, "xc_domain_claim_pages(%lu) failed", nr_pages);
    return 0;
}

/*
 * Attempt to claim memory with the legacy xc_domain_claim_pages() API.
 * Expect it to fail with the specified errno.
 * Record a failure on success or if it fails with an unexpected errno.
 */
int lib_claim_pages_legacy_failure(struct test_ctx *ctx, uint32_t domid,
                                   unsigned long nr_pages, int expected_errno,
                                   const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_pages(ctx->env->xch, domid, nr_pages);
    if ( rc == -1 && errno == expected_errno )
        return 0;

    return lib_fail_with_errno(ctx, errno,
                               "expected xc_domain_claim_pages() to fail "
                               "with errno=%d(%s), got rc=%d",
                               expected_errno, strerror(expected_errno), rc);
}

/* --- physmap population --- */

/*
 * Private helper function to populate extents at the specified GPFN
 * with the xc_domain_populate_physmap() API, and return the result code.
 *
 * Tests may use its callers lib_populate_success() or lib_populate_failure()
 * which record the failure the actual result did not match the expectation.
 */
static int lib_populate_physmap(struct test_ctx *ctx, lib_populate_args_t args)
{
    xen_pfn_t *frames;

    frames = calloc(args.nr_extents, sizeof(*frames));
    if ( !frames )
        return lib_fail(ctx, "calloc(%lu) failed", args.nr_extents);

    for ( unsigned long i = 0; i < args.nr_extents; i++ )
        frames[i] = args.start + i;

    errno = 0;
    rc = xc_domain_populate_physmap_exact(ctx->env->xch, args.domid,
                                          args.nr_extents, args.order,
                                          args.flags, frames);
    free(frames);
    return rc;
}

/*
 * Populate extents at the specified GPFN with checking if it
 * succeeded. Record the failure with diagnostics if it did not.
 */
int lib_populate_success(struct test_ctx *ctx, lib_populate_args_t args)
{
    rc = lib_populate_physmap(ctx, args);
    if ( rc )
        return lib_fail(ctx, "expected populate to succeed for node %u",
                        XENMEMF_get_node(args.flags));
    return 0;
}

/*
 * Attempt to populate extents at the specified GPFN with checking if it
 * failed. Record a failure with diagnostics if it did not fail as expected.
 */
int lib_populate_failure(struct test_ctx *ctx, lib_populate_args_t args)
{
    rc = lib_populate_physmap(ctx, args);
    if ( rc == 0 )
        return lib_fail_with_errno(
            ctx, 0, "expected exact-node populate to fail for node %u",
            XENMEMF_get_node(args.flags));
    return 0;
}

/* --- test runner --- */

static double timespec_diff_ms(const struct timespec *start,
                               const struct timespec *end)
{
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);

    return sec * 1000.0 + nsec / 1e6;
}

static void usage(FILE *stream, const char *prog)
{
    fprintf(stream,
            "Usage: %s [OPTIONS]\n\n"
            "Options:\n"
            "  -l, --list         List available test IDs and exit\n"
            "  -t, --test ID      Run only the specified test ID (repeatable)\n"
            "  -v, --verbose      Print per-step progress\n"
            "  -h, --help         Show this help text\n",
            prog);
}

int lib_print_available_tests(const struct test_case *cases, size_t num_cases)
{
    puts("Available tests:");
    for ( size_t i = 0; i < num_cases; i++ )
        printf("  %s  %s\n", cases[i].id, cases[i].name);
    return 0;
}

/*
 * Parse command-line arguments to configure the test run.
 * It populates the runtime_config struct with the parsed configuration,
 * including test IDs and the verbose flag.
 *
 * It supports filtering tests by test ID and enabling verbose output.
 * If --list is specified, prints available tests and exits.
 * By default, all tests will be run with concise output.
 * If cfg.list_only is set, the caller should exit after this function returns.
 *
 * Returns 0 on success, or 1 on failure (invalid arguments)
 */
int lib_parse_args(int argc, char *argv[], struct runtime_config *cfg)
{
    int opt;

    while ( (opt = getopt_long(argc, argv, "hlt:v", long_options, NULL)) != -1 )
    {
        switch ( opt )
        {
        case 'h':
            usage(stdout, argv[0]);
        case 'l':
            cfg->list_only = true;
            break;

        case 't':
            if ( cfg->nr_selected_ids >= ARRAY_SIZE(cfg->selected_ids) )
                errx(1, "too many --test selectors (max %zu)",
                     ARRAY_SIZE(cfg->selected_ids));
            cfg->selected_ids[cfg->nr_selected_ids++] = optarg;
            break;

        case 'v':
            cfg->verbose = true;
            break;

        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if ( cfg->list_only )
        return 0;

    printf("========= testcase program: %s ==========\n", argv[0]);
    if ( cfg->nr_selected_ids )
    {
        printf("Selected %zu test(s):\n", cfg->nr_selected_ids);
        for ( size_t i = 0; i < cfg->nr_selected_ids; i++ )
            printf("  %s\n", cfg->selected_ids[i]);
    }
    return 0;
}

/*
 * Run a single test case, capturing results and ensuring cleanup.
 * Returns 0 on success, or -1 on failure with result details populated.
 */
int lib_run_one_test(struct test_env *env, const struct runtime_config *cfg,
                     const struct test_case *test, struct test_result *result)
{
    struct test_ctx ctx = {
        .env = env,
        .cfg = cfg,
        .result = result,
        .domid = DOMID_INVALID,
        .helper_domid = DOMID_INVALID,
        .node = INVALID_NODE,
        .other_node = INVALID_NODE,
        .alloc_pages = 0,
        .step = "",
    };
    uint64_t baseline_outstanding;
    struct timespec start, end;

    result->test = test;
    result->status = TEST_PASSED;
    result->params[0] = '\0';
    result->details[0] = '\0';
    result->duration_ms = 0.0;

    /*
     * Fixture: capture baseline, create the primary domain, run the test
     * body, then always destroy any remaining test domains and verify
     * outstanding pages returned to baseline.
     */
    rc = lib_get_baseline_outstanding(&ctx, &baseline_outstanding);
    if ( rc )
        goto out;

    rc = lib_create_domain(&ctx, &ctx.domid, "primary");
    if ( rc )
        goto out;

    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = test->test(&ctx); /* Run the test body */

    clock_gettime(CLOCK_MONOTONIC, &end);
    result->duration_ms = timespec_diff_ms(&start, &end);

    if ( rc > 0 && result->status == TEST_SKIPPED )
        rc = 0;

out:
    /* Cleanup test domains without affecting the return code if rc != 0 */
    if ( lib_destroy_domain(&ctx, &ctx.helper_domid, "helper") && !rc )
        rc = -1;

    if ( lib_destroy_domain(&ctx, &ctx.domid, "primary") && !rc )
        rc = -1;

    if ( !rc &&
         lib_check_claim(&ctx, baseline_outstanding, 0,
                         "check cleanup restored baseline claimed pages") )
        rc = -1;

    if ( rc < 0 )
        result->status = TEST_FAILED;

    return rc;
}

/*
 * Run all test cases, filtering based on the runtime configuration, and print
 * results to stdout. Each test case is run with lib_run_one_test() which
 * captures detailed diagnostics on failure.
 */
void lib_run_tests(struct test_env *env, char *argv0,
                   const struct runtime_config *cfg,
                   const struct test_case *test_cases,
                   unsigned int num_test_cases, struct test_result *results)
{
    for ( size_t i = 0; i < num_test_cases; i++ )
    {
        struct test_result *result = &results[i];

        if ( !test_is_selected(cfg, &test_cases[i]) )
            continue;

        lib_run_one_test(env, cfg, &test_cases[i], result);

        /* Print a summary: test, result, including parameters and duration. */
        printf("%s::%s [%s] %s (%.2f ms)\n", argv0, result->test->id,
               result->params[0] ? result->params : "default",
               status_name(result->status), result->duration_ms);

        if ( result->status == TEST_FAILED || result->status == TEST_SKIPPED )
            printf("    %s\n", result->details);
    }
}

/*
 * Print a concise summary of test results, including counts of passed, failed,
 * and skipped tests, and details for any failures or skips.
 */
int lib_summary(const struct test_result *results, unsigned int num_results)
{
    unsigned int passed = 0, failed = 0, skipped = 0;

    puts("================== short test summary info =================");
    for ( size_t i = 0; i < num_results; i++ )
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
    return failed;
}

/* Update the create_template structure based on the host's capabilities */
static void fixup_create_template(struct xen_domctl_createdomain *create,
                                  const xc_physinfo_t *physinfo)
{
#if defined(__x86_64__) || defined(__i386__)
    if ( !(physinfo->capabilities & XEN_SYSCTL_PHYSCAP_hap) )
        create->flags &= ~XEN_DOMCTL_CDF_hap;

    if ( !(physinfo->capabilities &
           (XEN_SYSCTL_PHYSCAP_hap | XEN_SYSCTL_PHYSCAP_shadow)) ||
         !(physinfo->capabilities & XEN_SYSCTL_PHYSCAP_hvm) )
    {
        create->flags &= ~XEN_DOMCTL_CDF_hvm;
        create->arch.emulation_flags = 0;
    }
#else
    (void)physinfo;
#endif
}

/*
 * Initialise the test environment by opening the Xen control interface,
 * querying the number of NUMA nodes, and populating memory information.
 * Returns 0 on success, or -1 on failure with errno set.
 */
int lib_initialise_test_env(struct test_env *env)
{
    xc_physinfo_t physinfo;

    env->xch = xc_interface_open(NULL, NULL, 0);
    if ( !env->xch )
        err(1, "xc_interface_open");

    /*
     * Get the number of nodes to allocate xc_meminfo_t structures for.
     * If NUMA is disabled, this will return one node, so we can still
     * run tests that don't require > 1 NUMA node on non-NUMA hosts.
     */
    xc_numainfo(env->xch, &env->num_nodes, NULL, NULL);

    /* Allocate memory for xc_meminfo_t structures */
    env->meminfo = calloc(env->num_nodes, sizeof(*env->meminfo));
    if ( !env->meminfo )
        err(1, "calloc");

    /* Populate meminfo structures with current data */
    xc_numainfo(env->xch, &env->num_nodes, env->meminfo, NULL);
    xc_physinfo(env->xch, &physinfo);

    /* Initialise the create_template structure */
    env->create_template = (struct xen_domctl_createdomain){
        .flags = XEN_DOMCTL_CDF_hvm | XEN_DOMCTL_CDF_hap,
        .max_vcpus = 1,
        .max_grant_frames = 1,
        .grant_opts = XEN_DOMCTL_GRANT_version(1),
        .arch =
            {
#if defined(__x86_64__) || defined(__i386__)
                .emulation_flags = XEN_X86_EMU_LAPIC,
#endif
            },
    };
    /* Update the create_template structure based on the host's capabilities */
    fixup_create_template(&env->create_template, &physinfo);

    env->primary_node = 0;
    env->secondary_node = 0;
    env->have_secondary_node = false;

    /*
     * Pick the node with the most free memory as the primary node, and if
     * there's a second node, pick the one with the next most free memory as
     * the secondary.
     */
    for ( unsigned int i = 1; i < env->num_nodes; i++ )
    {
        if ( env->meminfo[i].memfree > env->meminfo[env->primary_node].memfree )
        {
            env->secondary_node = env->primary_node;
            env->primary_node = i;
            env->have_secondary_node = true;
        }
        else if ( !env->have_secondary_node ||
                  env->meminfo[i].memfree >
                      env->meminfo[env->secondary_node].memfree )
        {
            env->secondary_node = i;
            env->have_secondary_node = true;
        }
    }

    if ( env->num_nodes < 2 )
        env->have_secondary_node = false;
    else if ( env->secondary_node == env->primary_node )
    {
        for ( unsigned int i = 0; i < env->num_nodes; i++ )
        {
            if ( i != env->primary_node )
            {
                env->secondary_node = i;
                env->have_secondary_node = true;
                break;
            }
        }
    }

    return 0;
}

/* Free allocated memory and close the Xen control interface */
void lib_release_test_env(struct test_env *env)
{
    free(env->meminfo);
    env->meminfo = NULL;

    if ( env->xch )
    {
        xc_interface_close(env->xch);
        env->xch = NULL;
    }
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
