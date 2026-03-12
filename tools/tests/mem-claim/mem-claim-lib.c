/*
 * mem-claim-lib.c - Library of helper functions for memory claim tests
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xen-tools/common-macros.h>

#include "mem-claim-lib.h"
#include <xenguest.h>

static int rc;

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

void lib_set_step(struct test_ctx *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(ctx->step, sizeof(ctx->step), fmt, ap);
    va_end(ap);

    if ( ctx->cfg->verbose )
        printf("      step: %s\n", ctx->step);
}

static void append_snapshot(struct test_ctx *ctx)
{
    xc_physinfo_t physinfo;
    unsigned int nodes[2] = {ctx->node, ctx->other_node};

    rc = xc_physinfo(ctx->env->xch, &physinfo);
    if ( rc )
    {
        lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                    "\n    snapshot: xc_physinfo failed: %d - %s", errno,
                    strerror(errno));
        return;
    }

    lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                "\n    snapshot: free_pages=%" PRIu64
                ", outstanding_pages=%" PRIu64,
                physinfo.free_pages, physinfo.outstanding_pages);

    for ( size_t i = 0; i < ARRAY_SIZE(nodes); i++ )
    {
        unsigned long free_pages, total_pages;
        unsigned int node = nodes[i];

        if ( node == INVALID_NODE )
            continue;
        if ( i == 1 && node == nodes[0] )
            continue;

        rc = lib_get_node_free_pages(ctx->env, node, &free_pages, &total_pages);
        if ( rc )
        {
            lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                        "\n    snapshot: node%u unavailable: %d - %s", node,
                        errno, strerror(errno));
            continue;
        }

        lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                    "\n    snapshot: node%u free=%lu total=%lu", node,
                    free_pages, total_pages);
    }
}

int lib_fail_with_errno(struct test_ctx *ctx, int errnum, const char *fmt, ...)
{
    va_list ap;

    ctx->result->status = TEST_FAILED;
    ctx->result->details[0] = '\0';

    lib_appendf(ctx->result->details, sizeof(ctx->result->details), "step=%s",
                ctx->step[0] ? ctx->step : "(not set)");
    lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                "\n    domid=%u helper_domid=%u node=%s other_node=%s",
                ctx->domid, ctx->helper_domid,
                ctx->node == INVALID_NODE ? "n/a" : "set",
                ctx->other_node == INVALID_NODE ? "n/a" : "set");

    if ( ctx->node != INVALID_NODE )
        lib_appendf(ctx->result->details, sizeof(ctx->result->details), " (%u)",
                    ctx->node);
    if ( ctx->other_node != INVALID_NODE )
        lib_appendf(ctx->result->details, sizeof(ctx->result->details), " (%u)",
                    ctx->other_node);

    lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                "\n    alloc_pages=%lu", ctx->alloc_pages);

    lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                "\n    cause: ");
    va_start(ap, fmt);
    vsnprintf(ctx->result->details + strlen(ctx->result->details),
              sizeof(ctx->result->details) - strlen(ctx->result->details), fmt,
              ap);
    va_end(ap);

    if ( errnum )
        lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                    "\n    errno=%d (%s)", errnum, strerror(errnum));

    append_snapshot(ctx);
    return -1;
}

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

static int refresh_numainfo(struct test_env *env)
{
    unsigned int num_nodes = env->num_nodes;

    return xc_numainfo(env->xch, &num_nodes, env->meminfo, NULL);
}

int lib_get_node_free_pages(struct test_env *env, unsigned int node,
                            unsigned long *free_pages,
                            unsigned long *total_pages)
{
    if ( node >= env->num_nodes )
    {
        errno = EINVAL;
        return -1;
    }

    rc = refresh_numainfo(env);
    if ( rc )
        return rc;

    *free_pages = env->meminfo[node].memfree / XC_PAGE_SIZE;
    if ( total_pages )
        *total_pages = env->meminfo[node].memsize / XC_PAGE_SIZE;

    return 0;
}

int lib_get_global_free_pages(struct test_env *env, unsigned long *free_pages)
{
    uint64_t free_bytes;

    rc = xc_availheap(env->xch, 0, 0, -1, &free_bytes);
    if ( rc )
        return rc;

    *free_pages = free_bytes / XC_PAGE_SIZE;
    return 0;
}

int lib_get_baseline_outstanding(struct test_ctx *ctx,
                                 uint64_t *baseline_outstanding)
{
    xc_physinfo_t physinfo;

    lib_set_step(ctx, "query baseline outstanding pages");
    rc = xc_physinfo(ctx->env->xch, &physinfo);
    if ( rc )
        return lib_fail(ctx, "xc_physinfo failed while capturing baseline");

    *baseline_outstanding = physinfo.outstanding_pages;
    return 0;
}

int lib_check_claim(struct test_ctx *ctx, uint64_t baseline_outstanding,
                    uint64_t expected_delta, const char *reason)
{
    xc_physinfo_t physinfo;
    uint64_t expected = baseline_outstanding + expected_delta;

    lib_set_step(ctx, "%s", reason);
    xc_physinfo(ctx->env->xch, &physinfo);

    if ( physinfo.outstanding_pages != expected )
        return lib_fail_with_errno(
            ctx, 0, "expected outstanding_pages=%" PRIu64 ", got %" PRIu64,
            expected, physinfo.outstanding_pages);
    return 0;
}

/* --- domain lifecycle --- */

int lib_create_domain(struct test_ctx *ctx, uint32_t *domid, const char *label)
{
    struct xen_domctl_createdomain create = ctx->env->create_template;

    lib_set_step(ctx, "create %s domain", label);
    *domid = DOMID_INVALID;
    rc = xc_domain_create(ctx->env->xch, domid, &create);
    if ( rc )
        return lib_fail(ctx, "xc_domain_create(%s) failed", label);

    lib_set_step(ctx, "set maxmem for %s domain", label);
    rc = xc_domain_setmaxmem(ctx->env->xch, *domid, -1);
    if ( rc )
    {
        int destroy_rc;

        destroy_rc = xc_domain_destroy(ctx->env->xch, *domid);
        *domid = DOMID_INVALID;
        return lib_fail(ctx, "xc_domain_setmaxmem(%s) failed", label);
    }

    return 0;
}

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
            lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                        "\n    cleanup: xc_domain_destroy(%s) failed: %d (%s)",
                        label, errno, strerror(errno));
            return -1;
        }

        return lib_fail(ctx, "xc_domain_destroy(%s) failed", label);
    }

    return 0;
}

/* --- claim operations --- */

int lib_claim_memory(struct test_ctx *ctx, uint32_t domid, uint32_t nr_claims,
                     memory_claim_t *claims, const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_memory(ctx->env->xch, domid, nr_claims, claims);
    if ( rc )
        return lib_fail(ctx, "xc_domain_claim_memory failed");

    return 0;
}

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

int lib_claim_pages_legacy(struct test_ctx *ctx, uint32_t domid,
                           unsigned long nr_pages, const char *reason)
{
    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_claim_pages(ctx->env->xch, domid, nr_pages);
    if ( rc )
        return lib_fail(ctx, "xc_domain_claim_pages(%lu) failed", nr_pages);
    return 0;
}

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
    return 0;
}

/* --- physmap population --- */

int lib_populate_any(struct test_ctx *ctx, uint32_t domid, xen_pfn_t gpfn,
                     const char *reason)
{
    xen_pfn_t pfns[] = {gpfn};

    lib_set_step(ctx, "%s", reason);
    rc = xc_domain_populate_physmap_exact(ctx->env->xch, domid, 1, 0, 0, pfns);
    if ( rc )
        return lib_fail(ctx, "xc_domain_populate_physmap_exact(any) failed");
    return 0;
}

int lib_populate_exact_node(struct test_ctx *ctx,
                            struct lib_populate_exact_args args)
{
    xen_pfn_t pfns[] = {args.gpfn};

    lib_set_step(ctx, "%s", args.reason);
    rc = xc_domain_populate_physmap_exact(ctx->env->xch, args.domid,
                                          args.nr_extents, args.order,
                                          XENMEMF_exact_node(args.node), pfns);
    if ( rc )
        return lib_fail(ctx,
                        "xc_domain_populate_physmap_exact"
                        "(node=%u, order=%u) failed",
                        args.node, args.order);
    return 0;
}

int lib_expect_populate_exact_failure(struct test_ctx *ctx,
                                      struct lib_populate_exact_args args)
{
    xen_pfn_t pfns[] = {args.gpfn};

    lib_set_step(ctx, "%s", args.reason);
    errno = 0;
    rc = xc_domain_populate_physmap_exact(ctx->env->xch, args.domid,
                                          args.nr_extents, args.order,
                                          XENMEMF_exact_node(args.node), pfns);
    if ( rc == 0 )
        return lib_fail_with_errno(
            ctx, 0, "expected exact-node populate to fail for node %u",
            args.node);
    return 0;
}

/* --- page offlining --- */
#define LIB_E820_RAM 1U

int lib_offline_memory(struct test_ctx *ctx, uint32_t domid,
                       unsigned long nr_pages, const char *reason)
{
    struct e820entry map[E820MAX];
    xc_physinfo_t physinfo;
    unsigned long free_before = 0;
    unsigned long offlined = 0;
    unsigned long attempted = 0;
    unsigned long mark_failures = 0;
    unsigned long query_failures = 0;
    unsigned long skipped_non_online = 0;
    unsigned long unexpected_statuses = 0;
    unsigned long verification_failures = 0;
    int nr_entries;
    uint64_t mfn = 0;

    lib_set_step(ctx, "%s", reason);

    if ( !nr_pages )
        return 0;

    if ( !lib_get_global_free_pages(ctx->env, &free_before) &&
         !xc_physinfo(ctx->env->xch, &physinfo) )
        lib_debugf(ctx,
                   "before offlining domid=%u global_free=%lu outstanding=%" PRIu64,
                   domid, free_before, physinfo.outstanding_pages);

    nr_entries = xc_get_machine_memory_map(ctx->env->xch, map, E820MAX);
    if ( nr_entries < 0 )
        return lib_fail(ctx, "xc_get_machine_memory_map() failed");

    for ( int i = nr_entries - 1; i >= 0 && offlined < nr_pages; i-- )
    {
        uint64_t start, end;
        uint64_t backoff = 3072; /* back off on failure, start at 3MiB */

        if ( map[i].type != LIB_E820_RAM || !map[i].size )
            continue;

        start = map[i].addr >> XC_PAGE_SHIFT;
        end = (map[i].addr + map[i].size) >> XC_PAGE_SHIFT;
        lib_debugf(ctx,
                   "e820 RAM entry %d start=%" PRIu64 " end=%" PRIu64
                   " pages=%" PRIu64 " scanning backwards",
                   i, start, end, end - start);
        lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                    "\n    Found e820 entry %d: start=%" PRIu64
                    " end=%" PRIu64 " (%" PRIu64 " pages)",
                    i, start, end, end - start);

        if ( end <= start )
            continue;

        mfn = end - backoff;
        while ( mfn >= start && offlined < nr_pages )
        {
            uint32_t status;
            uint32_t verify_status;
            uint64_t current_mfn = mfn;

            errno = 0;
            rc = xc_query_page_offline_status(ctx->env->xch, current_mfn,
                                              current_mfn,
                                              &status);
            if ( rc < 0 )
            {
                query_failures++;
                lib_debugf(ctx,
                           "query before offlining failed for mfn=%" PRIu64
                           ": %d (%s)",
                           current_mfn, errno, strerror(errno));
                goto next_backoff;
            }

            if ( status != 0 )
            {
                skipped_non_online++;
                lib_debugf(ctx,
                           "skipping non-online mfn=%" PRIu64
                           " query_status=0x%x backoff=%" PRIu64,
                           current_mfn, status, backoff);
                goto next_backoff;
            }

            attempted++;
            errno = 0;
            rc = xc_mark_page_offline(ctx->env->xch, current_mfn, current_mfn,
                                      &status);
            if ( rc < 0 )
            {
                mark_failures++;
                lib_debugf(ctx,
                           "mark offline failed for mfn=%" PRIu64
                           ": %d (%s)",
                           current_mfn, errno, strerror(errno));
                goto next_backoff;
            }

            errno = 0;
            rc = xc_query_page_offline_status(ctx->env->xch, current_mfn,
                                              current_mfn, &verify_status);
            if ( rc < 0 )
            {
                query_failures++;
                verification_failures++;
                lib_debugf(ctx,
                           "query after offlining failed for mfn=%" PRIu64
                           ": %d (%s)",
                           current_mfn, errno, strerror(errno));
                goto next_backoff;
            }

            if ( verify_status & PG_OFFLINE_STATUS_OFFLINED )
            {
                lib_debugf(ctx,
                           "offlined mfn=%" PRIu64 " (%lu/%lu) mark_status=0x%x"
                           " query_status=0x%x",
                           current_mfn, offlined + 1, nr_pages, status,
                           verify_status);
                lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                            "\n    offlined page %" PRIu64, current_mfn);
                offlined++;
                backoff = 1;
                if ( current_mfn == start )
                    break;
                mfn = current_mfn - 1;
                continue;
            }

            unexpected_statuses++;
            verification_failures++;
            lib_debugf(ctx,
                       "mfn=%" PRIu64
                       " did not transition directly to offlined mark_status=0x%x"
                       " query_status=0x%x backoff=%" PRIu64,
                       current_mfn, status, verify_status, backoff);

            /* Revert unexpected states to avoid affecting later tests. */
            rc = xc_mark_page_online(ctx->env->xch, current_mfn, current_mfn,
                                     &verify_status);
            if ( rc < 0 )
                lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                            "\n    warning: failed to online page %" PRIu64
                            " after unexpected offline status 0x%x: %d (%s)",
                            current_mfn, verify_status, errno, strerror(errno));

next_backoff:
            if ( current_mfn <= start )
                break;

            if ( backoff < (UINT64_MAX >> 1) )
                backoff <<= 1;

            if ( current_mfn - start < backoff )
                mfn = end;
            else
                mfn = current_mfn - backoff;
        }
    }

    lib_debugf(ctx,
               "after offlining domid=%u attempted=%lu offlined=%lu"
               " mark_failures=%lu query_failures=%lu skipped_non_online=%lu"
               " verification_failures=%lu unexpected_statuses=%lu",
               domid, attempted, offlined, mark_failures, query_failures,
               skipped_non_online, verification_failures,
               unexpected_statuses);

    if ( offlined == nr_pages )
        return mfn;

    errno = ENOMEM;
    return lib_fail_with_errno(
        ctx, errno,
        "failed to offline %lu pages for domid=%u, only offlined %lu pages",
        nr_pages, domid, offlined);
}

/* --- test runner --- */

static void cleanup_test_domains(struct test_ctx *ctx)
{
    if ( ctx->helper_domid != DOMID_INVALID )
    {
        if ( xc_domain_destroy(ctx->env->xch, ctx->helper_domid) )
            lib_appendf(
                ctx->result->details, sizeof(ctx->result->details),
                "\n    cleanup: failed to destroy helper domain %u: %d - %s",
                ctx->helper_domid, errno, strerror(errno));
        ctx->helper_domid = DOMID_INVALID;
    }

    if ( ctx->domid != DOMID_INVALID )
    {
        if ( xc_domain_destroy(ctx->env->xch, ctx->domid) )
            lib_appendf(ctx->result->details, sizeof(ctx->result->details),
                        "\n    cleanup: failed to destroy domain %u: %d - %s",
                        ctx->domid, errno, strerror(errno));
        ctx->domid = DOMID_INVALID;
    }
}

static double timespec_diff_ms(const struct timespec *start,
                               const struct timespec *end)
{
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);

    return sec * 1000.0 + nsec / 1e6;
}

unsigned long lib_default_alloc_pages(unsigned long free_pages)
{
    return free_pages;
}

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

    clock_gettime(CLOCK_MONOTONIC, &start);

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

    rc = test->run(&ctx);

    if ( rc > 0 && result->status == TEST_SKIPPED )
    {
        rc = 0;
        goto out;
    }

out:
    if ( lib_destroy_domain(&ctx, &ctx.helper_domid, "helper") && !rc )
        rc = -1;

    if ( lib_destroy_domain(&ctx, &ctx.domid, "primary") && !rc )
        rc = -1;

    if ( !rc &&
         lib_check_claim(&ctx, baseline_outstanding, 0,
                         "check cleanup restored baseline claimed pages") )
        rc = -1;

    clock_gettime(CLOCK_MONOTONIC, &end);
    result->duration_ms = timespec_diff_ms(&start, &end);

    if ( rc < 0 && result->status != TEST_FAILED )
        result->status = TEST_FAILED;

    cleanup_test_domains(&ctx);
    return rc < 0 ? -1 : 0;
}

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

int lib_initialise_test_env(struct test_env *env)
{
    xc_physinfo_t physinfo;

    env->xch = xc_interface_open(NULL, NULL, 0);
    if ( !env->xch )
        err(1, "xc_interface_open");

    rc = xc_numainfo(env->xch, &env->num_nodes, NULL, NULL);
    if ( rc || !env->num_nodes )
        err(1, "xc_numainfo");

    env->meminfo = calloc(env->num_nodes, sizeof(*env->meminfo));
    if ( !env->meminfo )
        err(1, "calloc");

    rc = refresh_numainfo(env);
    if ( rc )
        err(1, "xc_numainfo");

    rc = xc_physinfo(env->xch, &physinfo);
    if ( rc )
        err(1, "xc_physinfo");

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
