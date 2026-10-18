/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * input-phase1.h - Test Phase 1 argument validation for memory claims,
 * including invalid claim parameters and claiming more pages than are free.
 *
 * I1-1 - I1-10: invalid claims are rejected with appropriate error codes,
 * and nr_claims == 0 releases all claims.
 *
 * This file contains test cases to validate argument handling when dealing
 * with per-node NUMA memory claims.
 *
 * Tests various invalid claim parameters (non-present node, too many claims,
 * out-of-range node id, pages above UINT32_MAX, non-zero pad, entries
 * with zero pages, null claims pointer with non-zero count) and verify they
 * are rejected with the expected error code.  An empty claim set, with or
 * without a claims pointer, is accepted and releases all claims.
 */
#ifndef MEM_CLAIM_INPUT_PHASE1_H
#define MEM_CLAIM_INPUT_PHASE1_H

#include "libtestclaims.h"

static int test_reject_non_present_node(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {
        .pages = 1, .target = ctx->env->num_nodes, /* Out-of-range node id */
    };

    for ( unsigned int node = 0; node < ctx->env->num_nodes; node++ )
        if ( ctx->env->meminfo[node].memsize == XEN_INVALID_MEM_SZ )
        {
            claim.target = node;
            break;
        }

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
    xen_domctl_memory_claim_t claims[nr_claims];

    ctx->target1 = ctx->env->primary_node;
    for ( uint32_t i = 0; i < nr_claims; i++ )
        claims[i] = ((xen_domctl_memory_claim_t){
                         .pages  = 1,
                         .target = ctx->env->primary_node,
                     });

    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=%u",
             nr_claims);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, nr_claims, claims, E2BIG,
        "reject claim list larger than the supported maximum");
}

static int test_reject_out_of_range_node(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {
        .pages  = 1,
        .target = UINT32_MAX >> 1,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u",
             claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, ENOENT,
        "reject claim with an out-of-range node id");
}

static int test_reject_pages_gt_uint32_max(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {
        .pages  = UINT32_MAX + 1ULL,
        .target = ctx->env->primary_node,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "pages=%" PRIu64 " node=%u", claim.pages, claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, EINVAL,
        "reject claim with pages larger than the domain page limit");
}

static int test_reject_nonzero_pad(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {
        .pages = 1,
        .target = ctx->env->primary_node,
        .pad = 1,
    };

    ctx->target1 = ctx->env->primary_node;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "node=%u",
             claim.target);

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &claim, EINVAL,
        "reject claim with non-zero padding");
}

static int test_release_zero_claim_count(struct test_ctx *ctx)
{
    uint64_t baseline;

    snprintf(ctx->result->params, sizeof(ctx->result->params), "nr_claims=0");

    rc = lib_get_total_claims(ctx, &baseline);
    if ( rc )
        return rc;

    rc = lib_claim_memory(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages = 1, .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST
        },
        "make a host claim before the zero-count release");
    if ( rc )
        return rc;

    rc = lib_check_claim(ctx, baseline, 1, "check the host claim exists");
    if ( rc )
        return rc;

    rc = lib_claim_memory(
        ctx, ctx->dom_1, 0, NULL,
        "accept xc_domain_set_memory_claims() with nr_claims == 0");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, baseline, 0, "check zero-count release");
}

static int test_null_claims_nonzero_count(struct test_ctx *ctx)
{
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=1 claims=NULL");

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, NULL, EFAULT,
        "reject xc_domain_set_memory_claims(nr=1, claims=NULL)");
}

static int test_zero_count_ignores_pointer(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {
        .target = UINT32_MAX,
        .pad = 1,
    };
    uint64_t baseline;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "nr_claims=0 claims=valid pointer, invalid entry");

    rc = lib_get_total_claims(ctx, &baseline);
    if ( rc )
        return rc;

    rc = lib_claim_memory(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages = 1, .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST
        },
        "make a host claim before the non-NULL zero-count release");
    if ( rc )
        return rc;

    rc = lib_check_claim(ctx, baseline, 1, "check the host claim exists");
    if ( rc )
        return rc;

    rc = lib_claim_memory(
        ctx, ctx->dom_1, 0, &claim,
        "accept nr_claims == 0 and ignore the claims pointer");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, baseline, 0,
                           "check non-NULL zero-count release");
}

/* I1-10: Entries that claim zero pages are rejected, alone or in a set. */
static int test_reject_zero_page_entry(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t host = {
        .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST,
    };
    xen_domctl_memory_claim_t node = {
        .target = ctx->env->primary_node,
    };
    xen_domctl_memory_claim_t mixed[2] = {
        { .pages = 1, .target = ctx->env->primary_node },
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST },
    };

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "pages=0 node=%u", node.target);

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &host, EINVAL,
        "reject a host-wide entry with zero pages");
    if ( rc )
        return rc;

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1, &node, EINVAL,
        "reject a node entry with zero pages");
    if ( rc )
        return rc;

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 2, mixed, EINVAL,
        "reject a claim set with one zero-page entry");
}

/*
 * I1-9: Check both xc_domain_claim_pages() and xc_domain_set_memory_claims()
 * with pages > free pages fail with ENOMEM.
 */
static int test_claim_pages_gt_free_enomem(struct test_ctx *ctx)
{
    xc_domaininfo_t info;
    uint64_t free_pages;

    /* Get the total free memory for sizing the claim */
    if ( lib_get_total_free_pages(ctx, &free_pages) )
        return -1;

    if ( xc_domain_getinfo_single(ctx->env->xch, ctx->dom_1, &info) )
        return lib_fail(ctx, "xc_domain_getinfo failed");

    if ( free_pages >= UINT32_MAX - (uint64_t)info.tot_pages )
        return lib_skip_test(ctx, "Free pages exceed the domain page limit");

    ctx->alloc_pages = free_pages + 1;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim=%" PRIu64 " free=%" PRIu64, ctx->alloc_pages, free_pages);

    rc = lib_claim_pages_legacy_failure(
        ctx, ctx->dom_1, ctx->alloc_pages + info.tot_pages, ENOMEM,
        "reject xc_domain_claim_pages() with pages > total free pages");
    if ( rc )
        return rc;

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages = ctx->alloc_pages,
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST
        },
        ENOMEM, "reject set_memory_claims() with pages > total free pages");
    if ( rc )
        return rc;

    /* Get the free pages on the primary node and fail to claim more than it */
    if ( lib_get_node_free_pages(ctx, ctx->env->primary_node,
                                 &free_pages, NULL) )
        return -1;

    rc = lib_expect_claim_memory_failure(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages  = free_pages + 1,
            .target = ctx->env->primary_node
        },
        ENOMEM, "reject set_memory_claims() with pages > node free pages");
    return rc;
}

#endif /* MEM_CLAIM_INPUT_PHASE1_H */
