/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * input-phase1.h - Test Phase 1 argument validation for memory claims,
 * including invalid claim parameters and claiming more pages than are free.
 *
 * I1-1 - I1-9: invalid claims are rejected with appropriate error codes.
 *
 * This file contains test cases to validate argument handling when dealing
 * with NUMA-aware claim sets.
 *
 * Tests various invalid claim parameters (non-present node, too many claims,
 * node id above UINT8_MAX, pages above INT32_MAX, non-zero pad, zero claim
 * count, null claims pointer with non-zero count, and non-null claims pointer
 * with zero count) and verify they are rejected with the expected error code.
 */
#include "libtestclaims.h"

static int test_reject_non_present_node(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1, .target = ctx->env->num_nodes, /* Out-of-range node id */
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%u num_nodes=%u", claim.target, ctx->env->num_nodes);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, ENOENT,
        "reject claim on a non-present NUMA node");
}

static int test_reject_too_many_claims(struct test_ctx *ctx)
{
    const uint32_t nr_claims = 0x100; /* Xen does not support such nr_claims */
    memory_claim_t claims[nr_claims];

    ctx->target1 = ctx->env->primary_node;
    for ( uint32_t i = 0; i < nr_claims; i++ )
        claims[i] = (memory_claim_t){
            .pages = 1,
            .target = ctx->env->primary_node,
        };

    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=%u",
             nr_claims);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, nr_claims, claims, E2BIG,
        "reject claim list larger than the supported maximum");
}

static int test_reject_node_gt_uint8_max(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .target = UINT8_MAX + 1U,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u",
             claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, ENOENT,
        "reject claim with node id above UINT8_MAX");
}

static int test_reject_pages_gt_int32_max(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = INT32_MAX + 1UL,
        .target = ctx->env->primary_node,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "pages=%" PRIu64 " node=%u", claim.pages, claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, ENOMEM,
        "reject claim with pages larger than INT32_MAX");
}

static int test_reject_nonzero_pad(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .target = ctx->env->primary_node,
        .pad = 1,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u pad=%u",
             claim.target, claim.pad);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, EINVAL,
        "reject claim with non-zero padding");
}

static int test_reject_zero_claim_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=0");

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 0, NULL, EINVAL,
        "reject xc_domain_claim_memory() with nr_claims == 0");
}

static int test_null_claims_nonzero_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=1 claims=NULL");

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, NULL, EFAULT,
        "reject xc_domain_claim_memory() with claims=NULL and nr_claims == 1");
}

static int test_zero_count_valid_pointer(struct test_ctx *ctx)
{
    memory_claim_t claim = {
        .pages = 1,
        .target = ctx->env->primary_node,
    };

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=0 claims=valid node=%u", claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 0, &claim, EINVAL,
        "reject xc_domain_claim_memory() with !nr_claims but a claims pointer");
}

/*
 * I1-9: Check both xc_domain_claim_pages() and xc_domain_claim_memory()
 * with pages > free pages fail with ENOMEM.
 */
static int test_claim_pages_gt_free_enomem(struct test_ctx *ctx)
{
    uint64_t free_pages;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx, &free_pages);
    ctx->alloc_pages = free_pages + 1;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim=%" PRIu64 " global=%" PRIu64, ctx->alloc_pages, free_pages);

    rc = lib_claim_pages_legacy_failure(
        ctx, ctx->dom_1, ctx->alloc_pages, ENOMEM,
        "reject xc_domain_claim_pages() with pages > global free page");
    if ( rc )
        return rc;

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1,
        &(memory_claim_t){.pages = ctx->alloc_pages,
                          .target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        ENOMEM, "reject claim_memory() with pages > global free pages");

    /* Get the free pages on the primary node and fail to claim more than it */
    lib_get_node_free_pages(ctx, ctx->env->primary_node, &free_pages, NULL);
    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1,
        &(memory_claim_t){.pages = free_pages + 1,
                          .target = ctx->env->primary_node},
        ENOMEM, "reject claim_memory() with pages > node free pages");
    return rc;
}
