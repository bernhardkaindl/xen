/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * input-phase2.h - Test argument validation for memory claims
 *
 * This file contains test cases to validate argument handling when dealing
 * with per-node NUMA memory claims.
 */
#ifndef TOOLS_TESTS_MEM_CLAIM_INPUT_PHASE2_H
#define TOOLS_TESTS_MEM_CLAIM_INPUT_PHASE2_H

#include "libtestclaims.h"

/* Helper to expect ENOMEM for host-wide claims */
static int claim_expect_host_wide_enomem(struct test_ctx *ctx, uint64_t claims)
{
    xc_domaininfo_t info;
    uint64_t free_pages;

    if ( lib_get_total_free_pages(ctx, &free_pages) )
        return -1;
    if ( free_pages < claims )
        return lib_skip_test(ctx,
                             "Host cannot fit the request without claims");

    if ( xc_domain_getinfo_single(ctx->env->xch, ctx->dom_2, &info) )
        return lib_fail(ctx, "xc_domain_getinfo failed");

    rc = lib_claim_pages_legacy_failure(
        ctx, ctx->dom_2, claims + info.tot_pages, ENOMEM,
        "expect ENOMEM for xc_domain_claim_pages() with claims > spare page");
    if ( rc )
        return rc;

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_2, 1,
        /* Request more than the spare to ensure failure */
        &(xen_domctl_memory_claim_t){
            .pages  = claims,
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST
        },
        ENOMEM,
        "expect ENOMEM for set_memory_claims() with claims > spare pages");
}

/* Helper to expect ENOMEM for per-node claims */
static int claim_expect_enomem_on_node(struct test_ctx *ctx, uint64_t claims)
{
    uint64_t free_pages;

    if ( lib_get_node_free_pages(ctx, ctx->target1, &free_pages, NULL) )
        return -1;
    if ( free_pages < claims )
        return lib_skip_test(ctx,
                             "Node cannot fit the request without claims");

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_2, 1,
        /* Request more than the spare to ensure failure */
        &(xen_domctl_memory_claim_t){
            .pages = claims, .target = ctx->target1
        },
        ENOMEM,
        "expect ENOMEM for set_memory_claims() with claims > spare pages");
}

/*
 * I2-1
 *
 * Create a legacy claim for dom_1 using claim_pages and assert claim calls
 * for dom_2 exceeding the unclaimed memory fail with ENOMEM.
 */
static int test_claim_pages_causes_enomem(struct test_ctx *ctx)
{
    xc_domaininfo_t info;
    uint64_t free_pages;

    /* Get the total free memory for sizing the claim */
    if ( lib_get_total_free_pages(ctx, &free_pages) )
        return -1;
    if ( xc_domain_getinfo_single(ctx->env->xch, ctx->dom_1, &info) )
        return lib_fail(ctx, "xc_domain_getinfo failed");

    if ( free_pages <= SPARE_PAGES )
        return lib_skip_test(ctx, "Not enough free pages for a host claim");

    ctx->alloc_pages = free_pages - SPARE_PAGES;
    if ( ctx->alloc_pages > UINT32_MAX - (uint64_t)info.tot_pages )
        return lib_skip_test(ctx, "Host claim exceeds the domain page limit");

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim=%" PRIu64 " free=%" PRIu64, ctx->alloc_pages, free_pages);

    rc = lib_claim_pages_legacy(
        ctx, ctx->dom_1, ctx->alloc_pages + info.tot_pages,
        "dom_1: claim nearly all host memory with claim_pages");
    if ( rc )
        return rc;

    rc = claim_expect_host_wide_enomem(ctx, SPARE_PAGES * 2);
    if ( !rc )
        rc = claim_expect_enomem_on_node(ctx, SPARE_PAGES * 2);
    return rc;
}

/*
 * I2-2
 *
 * Create a host-wide claim for dom 1 using claim_memory and assert that
 * claim calls for dom 2 that exceed the unclaimed memory fail with ENOMEM.
 */
static int test_host_overcommit_enomem(struct test_ctx *ctx)
{
    rc = lib_claim_all_on_host(ctx, ctx->dom_1, SPARE_PAGES);
    if ( rc )
        return rc;

    rc = claim_expect_host_wide_enomem(ctx, SPARE_PAGES * 2);
    if ( !rc )
        rc = claim_expect_enomem_on_node(ctx, SPARE_PAGES * 2);
    return rc;
}

/*
 * I2-3
 *
 * Create a primary-node claim for dom 1 using claim_memory and assert that
 * claim calls for dom 2 that exceed the unclaimed memory fail with ENOMEM.
 */
static int test_node_overcommit_enomem(struct test_ctx *ctx)
{
    rc = lib_claim_all_on_node(ctx, ctx->dom_1, ctx->target1, SPARE_PAGES);
    if ( rc )
        return rc;

    return claim_expect_enomem_on_node(ctx, SPARE_PAGES * 2);
}

#endif /* TOOLS_TESTS_MEM_CLAIM_INPUT_PHASE2_H */
