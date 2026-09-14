/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * claim-memory-allocations.h - Test claiming memory and claims protection
 * with NUMA-aware claim sets.
 *
 * Check claiming memory and allocation against claims with NUMA-aware
 * claim sets, including:
 *
 * - Claim nearly all free memory on a node or host-wide,
 * - Verify the claim is reflected in physinfo
 * - Assert that memory can be allocated by redeeming the claim.
 * - Exercise xc_domain_claim_memory() with different valid claim sets.
 * - Exercise xc_domain_claim_pages() and verify that claims are reflected
 *   in the outstanding pages in Xen.
 */
#include "libtestclaims.h"

/*
 * A1-1: basic single-node claim is tracked in outstanding pages and released
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
    uint64_t pre_existing_claims, free_pages;
    xen_domctl_memory_claim_t claim;

    /* Get the free memory on the test node */
    ctx->target1 = ctx->env->primary_node;
    lib_get_node_free_pages(ctx, ctx->target1, &free_pages, NULL);

    if ( free_pages < 2 )
        return lib_fail(
            ctx, "need >= 2 free pages on node %" PRIu64 ", got %" PRIu64,
            ctx->target1, free_pages);

    /*
     * Leave one page of slack between the claim and the free pages
     * so claim does not fail due to a small concurrent allocation.
     */
    ctx->alloc_pages = free_pages - 1;
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%" PRIu64 " claim_pages=%" PRIu64 " free_pages=%" PRIu64,
             ctx->target1, ctx->alloc_pages, free_pages);

    /*
     * Capture the baseline after domain creation (domain creation doesn't
     * change outstanding pages, so this equals the fixture's baseline).
     */
    rc = lib_get_total_claims(ctx, &pre_existing_claims);
    if ( rc )
        return rc;

    claim = (xen_domctl_memory_claim_t){
        .pages = ctx->alloc_pages,
        .target = ctx->target1
    };
    rc = lib_claim_memory(ctx, ctx->dom_1, 1, &claim, "set basic node claim");
    if ( rc )
        return rc;

    return lib_check_claim(
        ctx, pre_existing_claims, ctx->alloc_pages,
        "check node claim is reflected in outstanding pages");

    /* Domain teardown releases claim; fixture verifies baseline is restored. */
}

/*
 * A1-2: host-wide claim is replaced atomically after an allocation.
 *
 * Sets an initial host-wide claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int test_update_host_after_alloc(struct test_ctx *ctx)
{
    unsigned long free_pages;
    unsigned long initial_pages;

    /* Get the total free memory for sizing the initial claim */
    lib_get_total_free_pages(ctx, &free_pages);

    if ( free_pages < 2 )
        return lib_fail(ctx, "need >= 2 free pages total, got %" PRIu64,
                        free_pages);

    /*
     * This test needs two valid claim targets: an initial larger claim
     * and a smaller replacement target after consuming one claimed page.
     */
    initial_pages    = free_pages;
    ctx->alloc_pages = free_pages / 2;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "alloc_pages=%" PRIu64 " initial=%" PRIu64 " "
             "replacement=%" PRIu64 " total free=%" PRIu64,
             ctx->alloc_pages, initial_pages, ctx->alloc_pages, free_pages);

    rc = lib_claim_memory(ctx, ctx->dom_1, 1, /* one claim record */
                          &(xen_domctl_memory_claim_t){
                              .pages  = initial_pages,
                              .target = XEN_DOMCTL_CLAIM_MEMORY_HOST,
                          },
                          "install initial host-wide claim");
    if ( rc )
        return rc;

    lib_set_step(ctx, "Allocate one extent to consume part of claim");
    rc = lib_populate_success(ctx, (lib_populate_args_t){
                                  .domid      = ctx->dom_1,
                                  .nr_extents = 1,
                              });
    if ( rc )
        return rc;

    rc = lib_claim_memory(ctx, ctx->dom_1, 1, /* one claim record */
                          &(xen_domctl_memory_claim_t){
                              .pages  = ctx->alloc_pages,
                              .target = XEN_DOMCTL_CLAIM_MEMORY_HOST,
                          },
                          "reduce host-wide claim after allocation");
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->dom_1);
}

/*
 * A1-3: node-specific claim is replaced atomically after an allocation.
 *
 * Same as A1-2 but scoped to the primary NUMA node:
 * Sets an initial node claim, allocates one extent (consuming part of it),
 * then sets a smaller replacement claim and verifies the outstanding count
 * reflects the new absolute target.
 */
static int test_node_replace_after_alloc(struct test_ctx *ctx)
{
    uint64_t free_pages, initial_pages;

    /* Node used for claim sizing, claiming and allocation */
    ctx->target1 = ctx->env->primary_node;

    /* Get the free memory on the test node for sizing the initial claim */
    lib_get_node_free_pages(ctx, ctx->target1, &free_pages, NULL);
    if ( free_pages < 2 )
        return lib_skip_test(
            ctx, "need >= 2 pages on node %" PRIu64 ", got %" PRIu64,
            ctx->target1, free_pages);
    /*
     * This test needs two valid claim targets: an initial larger claim
     * and a smaller replacement target after consuming one claimed page.
     *
     * Leave one page of slack between the claim and the free pages
     * so claim does not fail due to a small concurrent allocation.
     */
    initial_pages    = free_pages - 1;
    ctx->alloc_pages = free_pages / 2;

    /* Logging of test parameters */
    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "node=%" PRIu64 " init=%" PRIu64 " replacement=%" PRIu64
             " free=%" PRIu64,
             ctx->target1, initial_pages, ctx->alloc_pages, free_pages);

    /* Create the claim with initial_pages */
    rc = lib_claim_memory(ctx, ctx->dom_1, 1, /* one claim */
                          &(xen_domctl_memory_claim_t){
                              .pages  = initial_pages,
                              .target = ctx->target1,
                          },
                          "set initial node-specific replacement claim");
    if ( rc )
        return rc;

    lib_set_step(ctx, "Allocate one extent to consume part of claim");
    rc =
        lib_populate_success(ctx, (lib_populate_args_t){
                                 .domid      = ctx->dom_1,
                                 .nr_extents = 1,
                                 .flags = XENMEMF_exact_node(ctx->target1),
                             });
    if ( rc )
        return rc;

    /* Update the claim with ctx->alloc_pages */
    rc = lib_claim_memory(ctx, ctx->dom_1, 1, /* one claim */
                          &(xen_domctl_memory_claim_t){
                              .pages  = ctx->alloc_pages,
                              .target = ctx->target1,
                          },
                          "replace node claim with a new absolute target");
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->dom_1);
}

/*
 * A1-4: legacy xc_domain_claim_pages() host claim is tracked in outstanding
 * pages, reduced by an allocation, and released when the domain is destroyed.
 */
static int test_legacy_host_wide_claim(struct test_ctx *ctx)
{
    uint64_t pre_existing_claims, free_pages;

    /* Get the total free memory for sizing the claim */
    lib_get_total_free_pages(ctx, &free_pages);

    ctx->alloc_pages = free_pages / 2;
    snprintf(ctx->result->params, sizeof(ctx->result->params), "claim=%" PRIu64,
             ctx->alloc_pages);

    rc = lib_get_total_claims(ctx, &pre_existing_claims);
    if ( rc )
        return rc;

    rc = lib_claim_pages_legacy(ctx, ctx->dom_1, ctx->alloc_pages,
                                "set legacy host-wide claim");
    if ( rc )
        return rc;

    rc = lib_check_claim(ctx, pre_existing_claims, ctx->alloc_pages,
                         "claim is added the outstanding pages");
    if ( rc )
        return rc;

    lib_set_step(ctx, "allocate extents to redeem a part of claim");
    rc = lib_populate_success(ctx, (lib_populate_args_t){
                                  .domid      = ctx->dom_1,
                                  .nr_extents = 10,
                              });
    if ( rc )
        return rc;

    return lib_check_claim(
        ctx, pre_existing_claims, ctx->alloc_pages - 10,
        "allocated against claim, outstanding pages reduced");
    /* Teardown releases the claim; fixture verifies baseline is restored. */
}

/*
 * A1-5: Test blocking allocation with claims and claim movement between nodes.
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
    uint64_t free_src, free_dst, spare_pages = 10;

    if ( !ctx->env->have_secondary_node )
        return lib_skip_test(ctx, "Requires at least two online NUMA nodes.");

    ctx->target1 = ctx->env->primary_node;
    ctx->target2 = ctx->env->secondary_node;

    lib_get_node_free_pages(ctx, ctx->target1, &free_src, NULL);
    lib_get_node_free_pages(ctx, ctx->target2, &free_dst, NULL);

    if ( free_src < spare_pages + 1 || free_dst < spare_pages + 1 )
        return lib_fail(ctx, "Need more pages, got %" PRIu64 "/%" PRIu64 ".",
                        free_src, free_dst);

    lib_set_step(ctx, "Claim most memory on source node.");
    rc = lib_claim_all_on_node(ctx, ctx->dom_1, ctx->target1, spare_pages);
    if ( rc )
        return rc;

    lib_set_step(ctx, "The claim blocks the allocation on the source node.");
    rc =
        lib_populate_failure(ctx, (lib_populate_args_t){
                                 .domid      = ctx->dom_2,
                                 .nr_extents = spare_pages * 2,
                                 .flags = XENMEMF_exact_node(ctx->target1),
                             });
    if ( rc )
        return rc;

    lib_set_step(ctx, "Move the claim to most memory on the destination node.");
    rc = lib_claim_all_on_node(ctx, ctx->dom_1, ctx->target2, spare_pages);
    if ( rc )
        return rc;

    lib_set_step(ctx, "Moved claim no longer blocks allocs on source node.");
    rc =
        lib_populate_success(ctx, (lib_populate_args_t){
                                 .domid      = ctx->dom_2,
                                 .start      = spare_pages * 2,
                                 .nr_extents = spare_pages * 2,
                                 .flags = XENMEMF_exact_node(ctx->target1),
                             });
    if ( rc )
        return rc;

    lib_set_step(ctx, "Moved claim now blocks allocs on destination node.");
    rc =
        lib_populate_failure(ctx, (lib_populate_args_t){
                                 .domid      = ctx->dom_2,
                                 .nr_extents = spare_pages * 2,
                                 .flags = XENMEMF_exact_node(ctx->target2),
                             });
    if ( rc )
        return rc;

    rc = lib_release_all_claims(ctx, ctx->dom_1);
    if ( rc )
        return rc;

    lib_set_step(ctx, "Claim released, allocs on destination node succeed.");
    rc =
        lib_populate_success(ctx, (lib_populate_args_t){
                                 .domid      = ctx->dom_2,
                                 .start      = spare_pages * 2,
                                 .nr_extents = spare_pages * 2,
                                 .flags = XENMEMF_exact_node(ctx->target2),
                             });
    if ( rc )
        return rc;

    return lib_release_all_claims(ctx, ctx->dom_1);
}

/*
 * A1-6: Check that a calling xc_domain_claim_pages(claim_pages = 0)
 * resets the claims to the baseline.
 */
static int test_zero_claim_resets_claim(struct test_ctx *ctx)
{
    uint64_t pre_existing_claims;

    rc = lib_get_total_claims(ctx, &pre_existing_claims);
    if ( rc )
        return rc;

    /* Make a claim first to move outstanding away from the baseline. */
    rc = lib_claim_pages_legacy(ctx, ctx->dom_1, 8,
                                "zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    /* Now set a zero claim to reset outstanding back to the baseline. */
    rc = lib_claim_pages_legacy(ctx, ctx->dom_1, 0,
                                "zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, pre_existing_claims, 0,
                           "check zero claim resets outstanding to baseline");
}

/*
 * A1-7: Check that a calling xc_domain_claim_memory(claim_pages = 0)
 * resets the claims to the baseline.
 */
static int test_zero_claim_memory_resets(struct test_ctx *ctx)
{
    uint64_t pre_existing_claims;

    rc = lib_get_total_claims(ctx, &pre_existing_claims);
    if ( rc )
        return rc;

    /* Make a claim first to move outstanding away from the baseline. */
    rc = lib_claim_memory(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages = 8, .target = ctx->env->primary_node
        },
        "make a claim to move outstanding away from baseline");
    if ( rc )
        return rc;

    /* Now set a zero claim to reset outstanding back to the baseline. */
    rc = lib_claim_memory(
        ctx, ctx->dom_1, 1,
        &(xen_domctl_memory_claim_t){
            .pages = 0, .target = XEN_DOMCTL_CLAIM_MEMORY_HOST
        },
        "set a zero claim to reset outstanding to baseline");
    if ( rc )
        return rc;

    return lib_check_claim(ctx, pre_existing_claims, 0,
                           "check zero claim resets outstanding to baseline");
}

/*
 * A1-8: Check that xc_domain_set_memory_claims accepts nr_entries == 0 and
 * claim_set == NULL to report the required claim-set size.
 */
static int test_query_claim_memory_size(struct test_ctx *ctx)
{
    xen_domctl_memory_claim_t claim = {};
    uint32_t nr_entries = 0;

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "mode=get nr_entries=0 claim_set=NULL");

    lib_set_step(ctx,
                 "Query claim-set size with nr_entries=0 and claim_set=NULL");
    rc = xc_domain_set_memory_claims(ctx->env->xch, ctx->dom_1,
                                     nr_entries, NULL);
    if ( rc != -1 || errno != ERANGE )
        return lib_fail_with_errno(
            ctx, errno,
            "expected xc_domain_set_memory_claims(entries=0, claim_set=NULL) "
            "to fail with ERANGE, got rc=%d and nr_entries=%u",
            rc, nr_entries);

    if ( nr_entries != 1 )
        return lib_fail(ctx, "expected required claim-set size 1, got %u",
                        nr_entries);

    lib_set_step(ctx, "Fetch the claim set using the returned entry count");
    rc = xc_domain_set_memory_claims(ctx->env->xch, ctx->dom_1,
                                     nr_entries, &claim);
    if ( rc )
        return lib_fail_with_errno(ctx, errno,
                                   "xc_domain_get_memory_claims failed");

    if ( nr_entries != 1 )
        return lib_fail(ctx, "expected one returned claim record, got %u",
                        nr_entries);

    if ( claim.target != XEN_DOMCTL_CLAIM_MEMORY_HOST || claim.pages != 0 )
        return lib_fail(ctx,
                        "expected a single zero host-wide claim, got target=%#x"
                        " pages=%" PRIu64,
                        claim.target, (uint64_t)claim.pages);

    return 0;
}
