/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * input-phase2.h - Test argument validation for memory claims
 *
 * This file contains test cases to validate argument handling when dealing
 * with NUMA-aware claim sets.
 */
#include "libtestclaims.h"

static int d2_claim_expect_enomem_global(struct test_ctx *ctx, uint64_t claims)
{
    rc = lib_claim_pages_legacy_failure(
        ctx, ctx->dom_2, claims, ENOMEM,
        "expect ENOMEM for xc_domain_claim_pages() with claims > spare page");
    if ( rc )
        return rc;

    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_2, 1,
        /* Request more than the spare to ensure failure */
        &(memory_claim_t){.pages  = claims,
                          .target = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
        ENOMEM, "expect ENOMEM for claim_memory() with claims > spare pages");
}

static int d2_claim_expect_enomem_node(struct test_ctx *ctx, uint64_t claims)
{
    return lib_expect_claim_memory_failure(
        ctx, ctx->dom_2, 1,
        /* Request more than the spare to ensure failure */
        &(memory_claim_t){.pages = claims, .target = ctx->target1}, ENOMEM,
        "expect ENOMEM for claim_memory() with claims > spare pages");
}

/*
 * I2-1
 *
 * Create a legacy global claim for d1 using claim_pages and assert that
 * claim calls for d2 that exceed the unclaimed memory fail with ENOMEM.
 */
static int test_claim_pages_causes_enomem(struct test_ctx *ctx)
{
    uint64_t free_pages;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx, &free_pages);
    ctx->alloc_pages = free_pages - SPARE_PAGES;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "claim=%" PRIu64 " global=%" PRIu64, ctx->alloc_pages, free_pages);

    rc = lib_claim_pages_legacy(
        ctx, ctx->dom_1, ctx->alloc_pages,
        "dom_1: claim nearly all global memory with claim_pages");
    if ( rc )
        return rc;

    rc = d2_claim_expect_enomem_global(ctx, SPARE_PAGES * 2);
    if ( !rc )
        rc = d2_claim_expect_enomem_node(ctx, SPARE_PAGES * 2);
    return rc;
}

/*
 * I2-2
 *
 * Create a global claim for d1 using claim_memory and assert that
 * claim calls for d2 that exceed the unclaimed memory fail with ENOMEM.
 */
static int test_claim_memory_causes_enomem(struct test_ctx *ctx)
{
    if ( lib_claim_all_on_host(ctx, ctx->dom_1, SPARE_PAGES) )
        return -1;
    rc = d2_claim_expect_enomem_global(ctx, SPARE_PAGES * 2);
    if ( !rc )
        rc = d2_claim_expect_enomem_node(ctx, SPARE_PAGES * 2);
    return rc;
}

/*
 * I2-3
 *
 * Create a primary-node claim for d1 using claim_memory and assert that
 * claim calls for d2 that exceed the unclaimed memory fail with ENOMEM.
 */
static int test_claim_prima_causes_enomem(struct test_ctx *ctx)
{
    if ( lib_claim_all_on_node(ctx, ctx->dom_1, ctx->target1, SPARE_PAGES) )
        return -1;
    return d2_claim_expect_enomem_node(ctx, SPARE_PAGES * 2);
}
