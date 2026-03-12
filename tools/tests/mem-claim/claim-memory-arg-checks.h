/*
 * Check argument validation of xc_domain_claim_memory() and its handler
 * in Xen.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mem-claim-lib.h"

/*
 * C-5 - C-12: invalid claims are rejected with appropriate error codes.
 *
 * Tests various invalid claim parameters (non-present node, too many claims,
 * node id above UINT8_MAX, pages above INT32_MAX, non-zero pad, zero claim
 * count, null claims pointer with non-zero count, and non-null claims pointer
 * with zero count) and verify they are rejected with the expected error code.
 */
static int test_reject_non_present_node(struct test_ctx *ctx)
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

static int test_reject_too_many_claims(struct test_ctx *ctx)
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

static int test_reject_node_gt_uint8_max(struct test_ctx *ctx)
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

static int test_reject_pages_gt_int32_max(struct test_ctx *ctx)
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

static int test_reject_nonzero_pad(struct test_ctx *ctx)
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

static int test_reject_zero_claim_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=0");

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 0, NULL, EINVAL,
        "reject xc_domain_claim_memory() with nr_claims == 0");
}

static int test_null_claims_nonzero_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=1 claims=NULL");

    return lib_expect_claim_memory_failure(
        ctx, ctx->domid, 1, NULL, EFAULT,
        "reject xc_domain_claim_memory() with claims=NULL and nr_claims == 1");
}

static int test_zero_count_valid_pointer(struct test_ctx *ctx)
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
 * C-13: Check both xc_domain_claim_pages() and xc_domain_claim_memory()
 * with pages > free pages fail with ENOMEM.
 */
static int test_claim_pages_gt_free_enomem(struct test_ctx *ctx)
{
    unsigned long free_pages;

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


