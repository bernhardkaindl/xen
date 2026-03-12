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
static int rc;

/*
 * CM000: basic single-node claim is tracked in outstanding pages and released
 * when the domain is destroyed.
 *
 * Smoke test: claims half the primary node's free pages, verifies they
 * appear in physinfo.outstanding_pages, then returns.  The fixture destroys
 * the domain, which releases the claim, and verifies outstanding pages returns
 * to the pre-test baseline.
 *
 * Skipped when NUMA is disabled (num_nodes == 1 and no per-node free pages) or
 * when there are fewer than 2 free pages available on the primary node.
 */
static int run_basic_node_claim(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    unsigned long free_pages, total_pages;
    unsigned long claim_pages;
    memory_claim_t claim;
    int rc;

    /* Get the free memory on the test node */
    ctx->node = ctx->env->primary_node;
    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages, &total_pages);

    ctx->alloc_pages = lib_default_alloc_pages(free_pages);
    claim_pages = ctx->alloc_pages;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%u claim_pages=%lu free_pages=%lu total_pages=%lu",
             ctx->node, claim_pages, free_pages, total_pages);

    /*
     * Capture the baseline after domain creation (domain creation doesn't
     * change outstanding pages, so this equals the fixture's baseline).
     */
    rc = lib_get_baseline_outstanding(ctx, &pre_claim_outstanding);
    if ( rc )
        return rc;

    claim = (memory_claim_t){.pages = claim_pages, .node = ctx->node};
    rc = lib_claim_memory(ctx, ctx->domid, 1, &claim, "set basic node claim");
    if ( rc )
        return rc;

    return lib_check_claim(
        ctx, pre_claim_outstanding, claim_pages,
        "check node claim is reflected in outstanding pages");
    /* Domain teardown releases the claim; fixture verifies baseline is
     * restored. */
}

/*
 * CM001: global claim is replaced atomically after an allocation.
 *
 * Sets an initial global claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int run_global_replace_after_alloc(struct test_ctx *ctx)
{
    unsigned long free_pages;
    unsigned long initial_pages;
    int rc;

    /* Get the global free memory for sizing the initial claim */
    lib_get_global_free_pages(ctx->env, &free_pages);

    /*
     * This test needs two valid claim targets: an initial larger claim
     * and a smaller replacement target after consuming one claimed page.
     */
    initial_pages = free_pages;
    ctx->alloc_pages = free_pages / 2;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "alloc_pages=%lu initial=%lu replacement=%lu global_free=%lu",
             ctx->alloc_pages, initial_pages, ctx->alloc_pages, free_pages);

    rc = lib_claim_memory(ctx, ctx->domid, 1, /* one claim */
                          &(memory_claim_t){
                              .pages = initial_pages,
                              .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL,
                          },
                          "set initial global replacement claim");
    if ( rc )
        return rc;

    rc = lib_populate_any(ctx, ctx->domid, 0,
                          "populate one extent to consume part of the claim");
    if ( rc )
        return rc;

    rc = lib_claim_memory(ctx, ctx->domid, 1, /* one claim */
                          &(memory_claim_t){
                              .pages = ctx->alloc_pages,
                              .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL,
                          },
                          "replace global claim with a new absolute target");
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->domid);
}

/*
 * CM002: node-specific claim is replaced atomically after an allocation.
 *
 * Same as CM001 but scoped to the primary NUMA node:
 * Sets an initial node claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int run_node_replace_after_alloc(struct test_ctx *ctx)
{
    unsigned long free_pages, total_pages, initial_pages;
    int rc;

    /* Node used for claim sizing, claiming and allocation */
    ctx->node = ctx->env->primary_node;

    /* Get the free memory on the test node for sizing the initial claim */
    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages, &total_pages);
    if ( free_pages < 2 )
        return lib_skip_test(ctx, "need at 2 free pages on node %u, got %lu",
                             ctx->node, free_pages);
    /*
     * This test needs two valid claim targets: an initial larger claim
     * and a smaller replacement target after consuming one claimed page.
     */
    initial_pages = free_pages;
    ctx->alloc_pages = free_pages / 2;

    /* Logging of test parameters */
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%u initial=%lu replacement=%lu node_free=%lu", ctx->node,
             initial_pages, ctx->alloc_pages, free_pages);

    /* Create the claim with initial_pages */
    rc = lib_claim_memory(ctx, ctx->domid, 1, /* one claim */
                          &(memory_claim_t){
                              .pages = initial_pages,
                              .node = ctx->node,
                          },
                          "set initial node-specific replacement claim");
    if ( rc )
        return rc;

    rc = lib_populate_exact_node(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->domid,
            .gpfn = 0,
            .nr_extents = 1,
            .order = 0,
            .node = ctx->node,
            .reason = "populate one exact-node extent to consume node claim",
        });
    if ( rc )
        return rc;

    /* Update the claim with ctx->alloc_pages */
    rc = lib_claim_memory(ctx, ctx->domid, 1, /* one claim */
                          &(memory_claim_t){
                              .pages = ctx->alloc_pages,
                              .node = ctx->node,
                          },
                          "replace node claim with a new absolute target");
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->domid);
}

/*
 * CM003: legacy xc_domain_claim_pages() global claim is tracked in outstanding
 * pages and released when the domain is destroyed.
 */
static int run_legacy_global_claim(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    unsigned long free_pages, claim_pages;
    int rc;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx->env, &free_pages);

    ctx->alloc_pages = lib_default_alloc_pages(free_pages);
    claim_pages = ctx->alloc_pages;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim_pages=%lu free_pages=%lu", claim_pages, free_pages);

    rc = lib_get_baseline_outstanding(ctx, &pre_claim_outstanding);
    if ( rc )
        return rc;

    rc = lib_claim_pages_legacy(ctx, ctx->domid, claim_pages,
                                "set legacy global claim");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, pre_claim_outstanding, claim_pages,
                           "check legacy claim in outstanding pages");
    /* Domain teardown releases the claim; fixture verifies baseline is
     * restored. */
}

/*
 * CM004: Test blocking allocation with claims and claim movement between nodes.
 *
 * Claims all free pages on the primary node, verifies allocation is blocked,
 * then moves the claim to the secondary node and verifies the source node
 * is freed while the destination node is now blocked.
 *
 * Requires at least two online NUMA nodes.
 */
static int run_move_claim_between_nodes(struct test_ctx *ctx)
{
    unsigned long free_pages_a, total_pages_a, free_pg_b, total_pg_b;
    memory_claim_t claim;
    int rc;

    if ( !ctx->env->have_secondary_node )
        return lib_skip_test(ctx, "requires at least two online NUMA nodes");

    ctx->node = ctx->env->primary_node;
    ctx->other_node = ctx->env->secondary_node;

    rc = lib_create_domain(ctx, &ctx->helper_domid, "helper");
    if ( rc )
        return rc;

    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages_a, &total_pages_a);
    lib_get_node_free_pages(ctx->env, ctx->other_node, &free_pg_b, &total_pg_b);

    /* Set*/
    ctx->alloc_pages = lib_default_alloc_pages(
        free_pages_a < free_pg_b ? free_pages_a : free_pg_b);

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "from=%u to=%u claim_from=%lu claim_to=%lu probe_order=0",
             ctx->node, ctx->other_node, free_pages_a, free_pg_b);

    claim = (memory_claim_t){
        .pages = free_pages_a,
        .node = ctx->node,
    };
    rc = lib_claim_memory(ctx, ctx->domid, 1, &claim,
                          "claim all currently free pages on the source node");
    if ( rc )
        return rc;

    rc = lib_expect_populate_exact_failure(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->helper_domid,
            .gpfn = 0,
            .nr_extents = 1,
            .order = 0,
            .node = ctx->node,
            .reason = "check source node allocation is blocked by the claim",
        });
    if ( rc )
        return rc;

    claim = (memory_claim_t){
        .pages = free_pg_b,
        .node = ctx->other_node,
    };
    rc = lib_claim_memory(ctx, ctx->domid, 1, &claim,
                          "move the claim to the destination node");
    if ( rc )
        return rc;

    rc = lib_populate_exact_node(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->helper_domid,
            .gpfn = 0,
            .nr_extents = 1,
            .order = 0,
            .node = ctx->node,
            .reason =
                "check source node allocation succeeds after claim moved away",
        });
    if ( rc )
        return rc;

    rc = lib_expect_populate_exact_failure(
        ctx, (struct lib_populate_exact_args){
                 .domid = ctx->helper_domid,
                 .gpfn = 1,
                 .nr_extents = 1,
                 .order = 0,
                 .node = ctx->other_node,
                 .reason = "check destination node allocation is now blocked",
             });
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->domid);
}

/*
 * CM005 - CM012: invalid claims are rejected with appropriate error codes.
 *
 * Tests various invalid claim parameters (non-present node, too many claims,
 * node id above UINT8_MAX, pages above INT32_MAX, non-zero pad, zero claim
 * count, null claims pointer with non-zero count, and non-null claims pointer
 * with zero count) and verify they are rejected with the expected error code.
 */
static int run_reject_non_present_node(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .node = ctx->env->num_nodes,
    };

    ctx->node = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%u num_nodes=%u", claim.node, ctx->env->num_nodes);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, &claim, ENOENT,
        "reject claim on a non-present NUMA node");
}

static int run_reject_too_many_claims(struct test_ctx *ctx)
{
    memory_claim_t claims[XEN_DOMCTL_CLAIM_MEMORY_MAX_CLAIMS + 1] = {0};
    const uint32_t nr_claims = XEN_DOMCTL_CLAIM_MEMORY_MAX_CLAIMS + 1;

    ctx->node = ctx->env->primary_node;
    for ( uint32_t i = 0; i < nr_claims; i++ )
        claims[i] = (memory_claim_t){
            .pages = 1,
            .node = ctx->env->primary_node,
        };

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=%u max_claims=%u", nr_claims,
             XEN_DOMCTL_CLAIM_MEMORY_MAX_CLAIMS);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, nr_claims, claims, E2BIG,
        "reject claim list larger than the supported maximum");
}

static int run_reject_node_gt_uint8_max(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .node = UINT8_MAX + 1U,
    };

    ctx->node = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u",
             claim.node);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, &claim, ENOENT,
        "reject claim with node id above UINT8_MAX");
}

static int run_reject_pages_gt_int32_max(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = INT32_MAX + 1UL,
        .node = ctx->env->primary_node,
    };

    ctx->node = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "pages=%lu node=%u", (unsigned long)claim.pages, claim.node);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, &claim, ENOMEM,
        "reject claim with pages larger than INT32_MAX");
}

static int run_reject_nonzero_pad(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .node = ctx->env->primary_node,
        .pad = 1,
    };

    ctx->node = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u pad=%u",
             claim.node, claim.pad);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, &claim, EINVAL,
        "reject claim with non-zero padding");
}

static int run_reject_zero_claim_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=0");

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 0, NULL, EINVAL,
        "reject xc_domain_claim_memory() with nr_claims == 0");
}

static int run_reject_null_claims_with_nonzero_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=1 claims=NULL");

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, NULL, EFAULT,
        "reject xc_domain_claim_memory() with claims=NULL and nr_claims == 1");
}

static int run_reject_zero_count_with_valid_pointer(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .node = ctx->env->primary_node,
    };

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=0 claims=valid node=%u", claim.node);

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 0, &claim, EINVAL,
        "reject xc_domain_claim_memory() with !nr_claims but a claims pointer");
}

/*
 * CM013: Check both xc_domain_claim_pages() and xc_domain_claim_memory()
 * with pages > free pages fail with ENOMEM.
 */
static int run_claim_pages_gt_free_enomem(struct test_ctx *ctx)
{
    unsigned long free_pages;
    int rc;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx->env, &free_pages);
    if ( free_pages < 2 )
        return lib_skip_test(
            ctx, "need at least 2 free pages globally, got %lu", free_pages);

    ctx->alloc_pages = free_pages + 1;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim_pages=%lu global_free=%lu", ctx->alloc_pages, free_pages);

    rc = lib_claim_pages_legacy_failure(
        ctx, ctx->domid, ctx->alloc_pages, ENOMEM,
        "reject xc_domain_claim_pages() with pages > global free page");
    if ( rc )
        return rc;

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = ctx->alloc_pages,
                          .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        ENOMEM, "reject claim_memory() with pages > global free pages");

    return rc;
}

/*
 * CM0014: Check that a claim_pages=0 resets the claims to the baseline.
 */
static int run_zero_claim_pages_resets_outstanding(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    int rc;

    rc = lib_get_baseline_outstanding(ctx, &pre_claim_outstanding);
    if ( rc )
        return rc;

    /* Make a claim first to move outstanding away from the baseline. */
    rc = lib_claim_pages_legacy(ctx, ctx->domid, 8,
                                "zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    /* Now set a zero claim to reset outstanding back to the baseline. */
    rc = lib_claim_pages_legacy(ctx, ctx->domid, 0,
                                "zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, pre_claim_outstanding, 0,
                           "check zero claim resets outstanding to baseline");
}

/*
 * CM0015: Check that a claim_memory=0 resets the claims to the baseline.
 */
static int run_zero_claim_memory_resets_outstanding(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    int rc;

    rc = lib_get_baseline_outstanding(ctx, &pre_claim_outstanding);
    if ( rc )
        return rc;

    /* Make a claim first to move outstanding away from the baseline. */
    rc = lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = 8, .node = ctx->env->primary_node},
        "make a claim to move outstanding away from baseline");
    if ( rc )
        return rc;

    /* Now set a zero claim to reset outstanding back to the baseline. */
    rc = lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = 0, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        "set a zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, pre_claim_outstanding, 0,
                           "check zero claim resets outstanding to baseline");
}

/*
 * CM016: Check how offlining memory interacts with claims.
 * When nearly all free pages are claimed, and offlining memory starts to eat
 * into the claim, the outstanding claim count should be reduced as some
 * claimed pages are no longer effectively claimed (since some are offline).
 * Mathematically, the claims can never exceed the free pages, which are
 * reduced by offlining memory after the claim is made. If Xen does not
 * keep this invariant by reducing the effective claim when free pages are
 * reduced beyond the claims, usable = total_avail_pages - oustanding_claims
 * would become negative (or very large depending on if the arithmetic wraps),
 * which without checks, could cause global claims to be ignored or else even
 * a panic of the hypervisor due to integer overflow if a BUG_ON() checks it.
 * The same logic applies to node-specific claims, but this test focuses on
 * global claims for simplicity.
 */
static int run_offline_memory_with_claims(struct test_ctx *ctx)
{
    xc_physinfo_t physinfo;
    /* The test claims all but a few spare pages */
    unsigned long spare_pages = 9UL;
    /* Then it offlines more pages than the spare pages to eat into the claim */
    unsigned long free_pages, offline_pages = spare_pages + 1;
    int debug_rc;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx->env, &free_pages);

    ctx->alloc_pages = free_pages - spare_pages;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim_pages = %lu free = %lu", ctx->alloc_pages, free_pages);

    debug_rc = xc_physinfo(ctx->env->xch, &physinfo);
    if ( !debug_rc )
        lib_debugf(ctx,
                   "CM016 before claim global_free=%lu outstanding=%" PRIu64,
                   free_pages, physinfo.outstanding_pages);

    rc = lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = ctx->alloc_pages,
                          .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        "make a global claim for all but a few spare pages");
    if ( rc )
        return rc;

    debug_rc = lib_get_global_free_pages(ctx->env, &free_pages);
    if ( !debug_rc && !xc_physinfo(ctx->env->xch, &physinfo) )
        lib_debugf(ctx,
                   "CM016 after claim global_free=%lu outstanding=%" PRIu64,
                   free_pages, physinfo.outstanding_pages);

    rc = lib_offline_memory(ctx, ctx->domid, offline_pages,
                            "offline memory to reduce free pages below claim");
    if ( rc )
        return rc;

    /*
     * We expect that as the offline reduces the free pages, the outstanding
     * claim count is also reduced to reflect the new effective claim.
     */
    lib_get_global_free_pages(ctx->env, &free_pages);
    if ( !xc_physinfo(ctx->env->xch, &physinfo) )
        lib_debugf(ctx,
                   "CM016 after offlining global_free=%lu outstanding=%" PRIu64
                   " expected_free=%lu",
                   free_pages, physinfo.outstanding_pages,
                   ctx->alloc_pages - offline_pages);

    if ( free_pages != ctx->alloc_pages - offline_pages )
        return lib_fail(ctx,
                        "expected free pages to be %lu (reduced by %lu) after "
                        "offlining memory (when all is outstanding), got %lu",
                        ctx->alloc_pages - offline_pages, offline_pages,
                        free_pages);

    return lib_check_claim(
        ctx, ctx->alloc_pages - offline_pages, 0,
        "check outstanding claim count after offlining memory");
}

/*
 * CM017: Check how offlining memory found from a primary-node allocation
 * interacts with a primary-node claim.
 *
 * There is no API to query outstanding claims per NUMA node, so this test
 * checks the indirect global effect instead: a claim on the primary node
 * contributes to physinfo.outstanding_pages, and if offlining pages on that
 * node eats into the claim, the global outstanding total must drop by the
 * amount of the node claim that is no longer effectively backed by free pages.
 *
 * To find candidate MFNs on the selected node without relying on the e820 map,
 * the test first populates a single page on the primary node, resolves the
 * resulting GPFN to an MFN via the domain P2M, and then tries to offline
 * subsequent MFNs starting from there.
 */
static int run_offline_primary_node_claims_with_mfn_lookup(struct test_ctx *ctx)
{
    const xen_pfn_t anchor_gpfn = 0;
    const unsigned long spare_pages = 9UL;
    const unsigned long offline_pages = spare_pages + 1;
    uint64_t baseline_outstanding;
    unsigned long free_pages, total_pages;
    unsigned long expected_reduction;
    xen_pfn_t anchor_mfn;
    xen_pfn_t start_mfn;
    int rc;

    ctx->node = ctx->env->primary_node;

    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages, &total_pages);
    if ( free_pages < offline_pages + 2 )
        return lib_skip_test(ctx,
                             "need at least %lu free pages on primary node %u,"
                             " got %lu",
                             offline_pages + 2, ctx->node, free_pages);

    rc = lib_populate_exact_node(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->domid,
            .gpfn = anchor_gpfn,
            .nr_extents = 1,
            .order = 0,
            .node = ctx->node,
            .reason = "populate one page on the primary node to discover an MFN",
        });
    if ( rc )
        return rc;

    rc = lib_get_domain_mfn(ctx, ctx->domid, anchor_gpfn, &anchor_mfn,
                            "resolve the populated primary-node page to an MFN");
    if ( rc )
        return rc;

    start_mfn = anchor_mfn + 1;
    if ( start_mfn <= anchor_mfn )
        return lib_skip_test(ctx,
                             "cannot scan subsequent MFNs from anchor=%" PRI_xen_pfn,
                             anchor_mfn);

    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages, &total_pages);
    if ( free_pages <= spare_pages )
        return lib_skip_test(ctx,
                             "need more than %lu free pages on node %u after "
                             "anchor allocation, got %lu",
                             spare_pages, ctx->node, free_pages);

    ctx->alloc_pages = free_pages - spare_pages;
    expected_reduction = offline_pages - spare_pages;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%u anchor_gpfn=%" PRI_xen_pfn " anchor_mfn=%" PRI_xen_pfn
             " claim_pages=%lu node_free=%lu total_pages=%lu offline_pages=%lu",
             ctx->node, anchor_gpfn, anchor_mfn, ctx->alloc_pages, free_pages,
             total_pages, offline_pages);

    rc = lib_get_baseline_outstanding(ctx, &baseline_outstanding);
    if ( rc )
        return rc;

    rc = lib_claim_memory(
        ctx, ctx->domid, 1,
        &(memory_claim_t){.pages = ctx->alloc_pages, .node = ctx->node},
        "make a primary-node claim for all but a few spare pages");
    if ( rc )
        return rc;

    rc = lib_check_claim(ctx, baseline_outstanding, ctx->alloc_pages,
                         "check primary-node claim is reflected globally");
    if ( rc )
        return rc;

    rc = lib_offline_memory_from_mfn(
        ctx, ctx->domid, start_mfn, offline_pages,
        "offline subsequent pages after the discovered primary-node MFN");
    if ( rc )
        return rc;

    return lib_check_claim(
        ctx, baseline_outstanding,
        ctx->alloc_pages - expected_reduction,
        "check global outstanding after offlining into the primary-node claim");
}

/*
 * List of test cases.  The fixture iterates over this list to run tests.
 *
 * Tests are identified by their id (e.g. "CM000") and have a descriptive name
 * and a function pointer to the test implementation.
 */
static const struct test_case test_cases[] = {
    {
        .id = "CM000",
        .name = "basic_node_claim",
        .run = run_basic_node_claim,
    },
    {
        .id = "CM001",
        .name = "global_replace_after_alloc",
        .run = run_global_replace_after_alloc,
    },
    {
        .id = "CM002",
        .name = "node_replace_after_alloc",
        .run = run_node_replace_after_alloc,
    },
    {
        .id = "CM003",
        .name = "legacy_global_claim",
        .run = run_legacy_global_claim,
    },
    {
        .id = "CM004",
        .name = "move_claim_between_nodes",
        .run = run_move_claim_between_nodes,
    },
    {
        .id = "CM005",
        .name = "reject_non_present_node",
        .run = run_reject_non_present_node,
    },
    {
        .id = "CM006",
        .name = "reject_too_many_claims",
        .run = run_reject_too_many_claims,
    },
    {
        .id = "CM007",
        .name = "reject_node_gt_uint8_max",
        .run = run_reject_node_gt_uint8_max,
    },
    {
        .id = "CM008",
        .name = "reject_pages_gt_int32_max",
        .run = run_reject_pages_gt_int32_max,
    },
    {
        .id = "CM009",
        .name = "reject_nonzero_pad",
        .run = run_reject_nonzero_pad,
    },
    {
        .id = "CM010",
        .name = "reject_zero_claim_count",
        .run = run_reject_zero_claim_count,
    },
    {
        .id = "CM011",
        .name = "reject_null_claims_with_nonzero_count",
        .run = run_reject_null_claims_with_nonzero_count,
    },
    {
        .id = "CM012",
        .name = "reject_zero_count_with_valid_pointer",
        .run = run_reject_zero_count_with_valid_pointer,
    },
    {
        .id = "CM013",
        .name = "claim_pages_gt_free_enomem",
        .run = run_claim_pages_gt_free_enomem,
    },
    {
        .id = "CM014",
        .name = "zero_claim_pages_resets_outstanding",
        .run = run_zero_claim_pages_resets_outstanding,
    },
    {
        .id = "CM015",
        .name = "zero_claim_memory_resets_outstanding",
        .run = run_zero_claim_memory_resets_outstanding,
    },
    {
        .id = "CM016",
        .name = "offline_memory_with_claims",
        .run = run_offline_memory_with_claims,
    },
    {
        .id = "CM017",
        .name = "offline_primary_node_claims_with_mfn_lookup",
        .run = run_offline_primary_node_claims_with_mfn_lookup,
    },
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
