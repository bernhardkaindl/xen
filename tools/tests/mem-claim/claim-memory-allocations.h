/*
 * Check claining and allocation with xc_domain_claim_memory()
 * and xc_domain_claim_pages() APIs, and that claims are reflected
 * in the outstanding pages in Xen.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mem-claim-lib.h"

/*
 * C-0: basic single-node claim is tracked in outstanding pages and released
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
static int test_basic_node_claim(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    unsigned long free_pages, total_pages;
    unsigned long claim_pages;
    memory_claim_t claim;

    /* Get the free memory on the test node */
    ctx->node = ctx->env->primary_node;
    lib_get_node_free_pages(ctx->env, ctx->node, &free_pages, &total_pages);

    ctx->alloc_pages = free_pages;
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
 * C-1: global claim is replaced atomically after an allocation.
 *
 * Sets an initial global claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int test_global_replace_after_alloc(struct test_ctx *ctx)
{
    unsigned long free_pages;
    unsigned long initial_pages;

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
 * C-2: node-specific claim is replaced atomically after an allocation.
 *
 * Same as C-1 but scoped to the primary NUMA node:
 * Sets an initial node claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int test_node_replace_after_alloc(struct test_ctx *ctx)
{
    unsigned long free_pages, total_pages, initial_pages;

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
 * C-3: legacy xc_domain_claim_pages() global claim is tracked in outstanding
 * pages and released when the domain is destroyed.
 */
static int test_legacy_global_claim(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;
    unsigned long free_pages, claim_pages;

    /* Get the global free memory for sizing the claim */
    lib_get_global_free_pages(ctx->env, &free_pages);

    ctx->alloc_pages = free_pages;
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
 * C-4: Test blocking allocation with claims and claim movement between nodes.
 *
 * This test performs a sequence of claims and allocations to verify that claims
 * block allocations on the claimed node, that moving a claim to another node
 * allows allocation on the original node, and that the new node is now blocked
 * by the claim until it is released.
 *
 * To achieve this, the test creates a helper domain used for allocation
 * attempts, then:
 *
 * Claims most free pages on the primary node, verifies allocation is blocked,
 * then moves the claim to the secondary node, verifies the original allocation
 * can now succeed on the primary node.
 *
 * It then verifies allocation is now blocked on the secondary node,
 * releases the claim, and verifies the allocation can now succeed on the
 * secondary node as well.
 *
 * Requires at least two online NUMA nodes.
 */
static int test_move_claim_between_nodes(struct test_ctx *ctx)
{
    unsigned long spare_pages = 10;

    if ( !ctx->env->have_secondary_node )
        return lib_skip_test(ctx, "requires at least two online NUMA nodes");

    ctx->node = ctx->env->primary_node;
    ctx->other_node = ctx->env->secondary_node;

    rc = lib_create_domain(ctx, &ctx->helper_domid, "helper");
    if ( rc )
        return rc;

    lib_set_step(ctx, "claim most memory on source node");
    rc = lib_claim_all_on_node(ctx, ctx->domid, ctx->node, spare_pages);
    if ( rc )
        return rc;

    rc = lib_expect_populate_exact_failure(
        ctx, (struct lib_populate_exact_args){
                 .domid = ctx->helper_domid,
                 .gpfn = 0,
                 .nr_extents = spare_pages * 2,
                 .order = 0,
                 .node = ctx->node,
                 .reason = "source node allocation is blocked by the claim",
             });
    if ( rc )
        return rc;

    lib_set_step(ctx, "Move the claim to most memory on the destination node");
    rc = lib_claim_all_on_node(ctx, ctx->domid, ctx->other_node, spare_pages);
    if ( rc )
        return rc;

    rc = lib_populate_exact_node(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->helper_domid,
            .gpfn = spare_pages * 2,
            .nr_extents = spare_pages * 2,
            .order = 0,
            .node = ctx->node,
            .reason = "source alloc of 2 * spare succeeds after claim moved",
        });
    if ( rc )
        return rc;

    rc = lib_expect_populate_exact_failure(
        ctx, (struct lib_populate_exact_args){
                 .domid = ctx->helper_domid,
                 .gpfn = 0,
                 .nr_extents = spare_pages * 2,
                 .order = 0,
                 .node = ctx->other_node,
                 .reason = "destination alloc of 2 * spare is now blocked",
             });
    if ( rc )
        return rc;

    rc = lib_release_all_claims(ctx, ctx->domid);
    if ( rc )
        return rc;

    rc = lib_populate_exact_node(
        ctx,
        (struct lib_populate_exact_args){
            .domid = ctx->helper_domid,
            .gpfn = spare_pages * 2,
            .nr_extents = spare_pages * 2,
            .order = 0,
            .node = ctx->other_node,
            .reason = "destnode alloc of 2 * spare succeeds after claim cancel",
        });
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->domid);
}

/*
 * C-14: Check that a calling xc_domain_claim_pages(claim_pages = 0)
 * resets the claims to the baseline.
 */
static int test_zero_claim_resets_claim(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;

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
 * C-15: Check that a calling xc_domain_claim_memory(claim_pages = 0)
 * resets the claims to the baseline.
 */
static int test_zero_claim_memory_resets(struct test_ctx *ctx)
{
    uint64_t pre_claim_outstanding;

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