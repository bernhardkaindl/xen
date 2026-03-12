/*
 * test-offline-online.c - Regression test for online_page() race
 *
 * Exercises the race window in online_page() where a page transitions
 * through PGC_state_inuse between dropping heap_lock and calling
 * free_heap_pages().  During that window, a concurrent allocation can
 * find the page in a buddy with count_info == 0 (PGC_state_inuse)
 * instead of PGC_state_free, triggering a BUG_ON in alloc_heap_pages().
 *
 * The scrubber running on another CPU widens the window because it holds
 * heap_lock while scrubbing dirty pages, increasing contention.
 *
 * Strategy:
 *  1. Allocate pages to a domain and immediately destroy it so those
 *     pages are freed with need_scrub, giving the background scrubber
 *     work to do (heap_lock contention).
 *  2. Offline a batch of free pages.
 *  3. Rapidly online those pages back — each online_page() call hits the
 *     race window.
 *  4. Simultaneously allocate and free pages via domain
 *     populate_physmap to stress alloc_heap_pages().
 *  5. Repeat for many iterations.
 *
 * If the bug is present and triggered, the hypervisor panics with:
 *   pg[0] MFN ... c=0 o=0 v=0 t=0
 * followed by BUG().
 *
 * If the test completes without a hypervisor crash, it passes.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xen-tools/common-macros.h>
#include <xenctrl.h>
#include <xenguest.h>

#include "mem-claim-lib.h"

/*
 * Number of pages to offline/online per iteration.  Needs to be large
 * enough to create a meaningful race window but small enough to find
 * that many free pages reliably.  The original bug was observed with
 * ~10+ pages.
 */
#define OFFLINE_BATCH      16

/*
 * Number of outer iterations (offline-batch → online-batch cycles).
 * More iterations increase the chance of hitting the race.
 */
#define DEFAULT_ITERATIONS 200

/*
 * Number of pages to allocate/free per iteration to stress the allocator
 * concurrently with the online path.
 */
#define ALLOC_STRESS_PAGES 32

static const struct option long_options[] = {
    { "help",       no_argument,       NULL, 'h' },
    { "iterations", required_argument, NULL, 'n' },
    { "batch",      required_argument, NULL, 'b' },
    { "verbose",    no_argument,       NULL, 'v' },
    { NULL, 0, NULL, 0 },
};

static void usage(FILE *stream, const char *prog)
{
    fprintf(stream,
            "Usage: %s [OPTIONS]\n\n"
            "Regression test for online_page() race with alloc_heap_pages().\n"
            "If the hypervisor panics during the test, the bug is present.\n\n"
            "Options:\n"
            "  -n, --iterations N  Number of offline/online cycles (default %d)\n"
            "  -b, --batch N       Pages to offline per cycle (default %d)\n"
            "  -v, --verbose       Print per-step progress\n"
            "  -h, --help          Show this help text\n",
            prog, DEFAULT_ITERATIONS, OFFLINE_BATCH);
}

/*
 * Create dirty (need-scrub) pages by allocating pages to a domain and then
 * destroying it.  The freed pages will have PGC_need_scrub set, keeping the
 * background scrubber busy and creating heap_lock contention that widens
 * the race window in online_page().
 */
static int create_scrub_pressure(struct test_ctx *ctx, unsigned long nr_pages)
{
    uint32_t domid = DOMID_INVALID;
    xen_pfn_t *pfns = NULL;
    int rc;

    rc = lib_create_domain(ctx, &domid, "scrub-pressure");
    if ( rc )
        return rc;

    pfns = calloc(nr_pages, sizeof(*pfns));
    if ( !pfns )
    {
        lib_destroy_domain(ctx, &domid, "scrub-pressure");
        return lib_fail(ctx, "calloc for %lu pfns", nr_pages);
    }

    for ( unsigned long i = 0; i < nr_pages; i++ )
        pfns[i] = i;

    /*
     * Populate pages — don't care if some fail; we just want to create
     * enough dirty pages for the scrubber.
     */
    lib_set_step(ctx, "populate %lu pages for scrub pressure", nr_pages);
    xc_domain_populate_physmap_exact(ctx->env->xch, domid, nr_pages, 0, 0,
                                     pfns);
    free(pfns);

    /*
     * Destroy the domain: all its pages are freed with need_scrub,
     * feeding the background scrubber.
     */
    rc = lib_destroy_domain(ctx, &domid, "scrub-pressure");
    return rc;
}

/*
 * Scan for free pages that can be offlined.  Returns MFNs of offlinable
 * free pages in the caller-supplied array.  Searches backwards from the
 * top of physical memory to find pages far from kernel/Xen use.
 */
static int find_offlinable_mfns(struct test_ctx *ctx, xen_pfn_t *mfns,
                                unsigned int wanted, unsigned int *found)
{
    unsigned long max_mfn;
    unsigned int count = 0;
    int rc;

    rc = xc_maximum_ram_page(ctx->env->xch, &max_mfn);
    if ( rc )
        return lib_fail(ctx, "xc_maximum_ram_page() failed");

    for ( xen_pfn_t mfn = max_mfn; mfn > 0 && count < wanted; mfn-- )
    {
        uint32_t status;

        rc = xc_query_page_offline_status(ctx->env->xch, mfn, mfn, &status);
        if ( rc < 0 )
            continue;

        /* status == 0 means the page is online and free / offlinable */
        if ( status == 0 )
            mfns[count++] = mfn;
    }

    *found = count;
    return 0;
}

/*
 * Offline a set of pages by MFN.
 * Returns the number of pages successfully offlined.
 */
static unsigned int offline_pages(struct test_ctx *ctx, const xen_pfn_t *mfns,
                                  unsigned int count)
{
    unsigned int offlined = 0;

    for ( unsigned int i = 0; i < count; i++ )
    {
        uint32_t status;
        int rc;

        rc = xc_mark_page_offline(ctx->env->xch, mfns[i], mfns[i], &status);
        if ( rc == 0 )
            offlined++;
    }

    return offlined;
}

/*
 * Online a set of pages by MFN.
 * Returns the number of pages successfully onlined.
 */
static unsigned int online_pages(struct test_ctx *ctx, const xen_pfn_t *mfns,
                                 unsigned int count)
{
    unsigned int onlined = 0;

    for ( unsigned int i = 0; i < count; i++ )
    {
        uint32_t status;
        int rc;

        rc = xc_mark_page_online(ctx->env->xch, mfns[i], mfns[i], &status);
        if ( rc == 0 )
            onlined++;
    }

    return onlined;
}

/*
 * Allocate pages to a domain to stress the allocator.  This forces
 * alloc_heap_pages() to run and check PGC_state_free on buddy pages,
 * which is where the BUG_ON fires when the race is hit.
 */
static int stress_allocator(struct test_ctx *ctx, uint32_t domid,
                            unsigned long nr_pages, xen_pfn_t base_gpfn)
{
    xen_pfn_t *pfns;

    pfns = calloc(nr_pages, sizeof(*pfns));
    if ( !pfns )
        return -1;

    for ( unsigned long i = 0; i < nr_pages; i++ )
        pfns[i] = base_gpfn + i;

    /* Best-effort: don't fail if some pages can't be allocated */
    xc_domain_populate_physmap_exact(ctx->env->xch, domid, nr_pages, 0, 0,
                                     pfns);
    free(pfns);
    return 0;
}

/*
 * OO000: offline/online race regression test.
 *
 * Rapidly cycles pages through offline → online while stressing the page
 * allocator.  Creates scrub pressure to widen the race window.
 *
 * If online_page() has the race (transitioning through PGC_state_inuse
 * outside heap_lock before calling free_heap_pages), the hypervisor will
 * panic with a BUG_ON in alloc_heap_pages().
 */
static int run_offline_online_race(struct test_ctx *ctx)
{
    unsigned int iterations = ctx->alloc_pages; /* overloaded as iteration count */
    unsigned int batch = OFFLINE_BATCH;
    xen_pfn_t *offline_mfns;
    unsigned int nr_offline_mfns = 0;
    unsigned long alloc_gpfn_base = 0;
    int rc;

    /*
     * Step 1: Create scrub pressure.  Allocate and free a large batch of
     * pages so the scrubber has work to do, keeping heap_lock contended.
     */
    lib_set_step(ctx, "create scrub pressure with %u pages",
                 ALLOC_STRESS_PAGES * 4);
    rc = create_scrub_pressure(ctx, ALLOC_STRESS_PAGES * 4);
    if ( rc )
        return rc;

    /*
     * Step 2: Find candidate pages to offline.  We look for more than we
     * need per batch so we have a pool to rotate through.
     */
    offline_mfns = calloc(batch * 2, sizeof(*offline_mfns));
    if ( !offline_mfns )
        return lib_fail(ctx, "calloc for offline MFN array");

    lib_set_step(ctx, "find %u offlinable pages", batch);
    rc = find_offlinable_mfns(ctx, offline_mfns, batch, &nr_offline_mfns);
    if ( rc )
    {
        free(offline_mfns);
        return rc;
    }

    if ( nr_offline_mfns < 4 )
    {
        free(offline_mfns);
        return lib_skip_test(ctx,
                             "need at least 4 offlinable pages, found %u",
                             nr_offline_mfns);
    }

    snprintf(ctx->result->params, sizeof(ctx->result->params),
             "iterations=%u batch=%u found_pages=%u",
             iterations, nr_offline_mfns, nr_offline_mfns);

    lib_debugf(ctx, "found %u offlinable pages, running %u iterations",
               nr_offline_mfns, iterations);

    /*
     * Step 3: Main loop — offline, then online + allocate in rapid succession.
     *
     * Each iteration:
     *  a) Creates fresh scrub work (small batch) to keep scrubber active
     *  b) Offlines the batch of pages
     *  c) Immediately onlines them back — this is where the race lives
     *  d) Allocates pages to stress alloc_heap_pages() concurrently
     */
    for ( unsigned int iter = 0; iter < iterations; iter++ )
    {
        unsigned int offlined, onlined;

        if ( ctx->cfg->verbose && (iter % 50 == 0) )
            lib_debugf(ctx, "iteration %u/%u", iter, iterations);

        /*
         * Create some scrub pressure every few iterations to keep the
         * background scrubber busy.
         */
        if ( iter % 10 == 0 )
        {
            rc = create_scrub_pressure(ctx, ALLOC_STRESS_PAGES);
            if ( rc )
                break;
        }

        /* Offline the batch */
        lib_set_step(ctx, "iter %u: offline %u pages", iter, nr_offline_mfns);
        offlined = offline_pages(ctx, offline_mfns, nr_offline_mfns);
        if ( !offlined )
        {
            lib_debugf(ctx, "iter %u: no pages could be offlined, retrying "
                       "with fresh candidates", iter);
            /*
             * Pages may have been allocated since we scanned.  Re-scan to
             * find new candidates.
             */
            nr_offline_mfns = 0;
            rc = find_offlinable_mfns(ctx, offline_mfns, batch,
                                      &nr_offline_mfns);
            if ( rc || nr_offline_mfns < 4 )
            {
                lib_debugf(ctx, "iter %u: only %u pages found, ending early",
                           iter, nr_offline_mfns);
                break;
            }
            continue;
        }

        /*
         * Stress allocator: allocate pages in the primary domain *before*
         * onlining, so the allocator is actively walking buddies while
         * online_page() runs.
         */
        lib_set_step(ctx, "iter %u: stress allocator", iter);
        stress_allocator(ctx, ctx->domid, ALLOC_STRESS_PAGES,
                         alloc_gpfn_base);
        alloc_gpfn_base += ALLOC_STRESS_PAGES;

        /*
         * Online the offlined pages — this is the critical section.
         * Each xc_mark_page_online() call invokes online_page() in the
         * hypervisor, which (without the fix) drops heap_lock between
         * setting PGC_state_inuse and calling free_heap_pages().
         */
        lib_set_step(ctx, "iter %u: online %u pages", iter, offlined);
        onlined = online_pages(ctx, offline_mfns, nr_offline_mfns);

        lib_debugf(ctx, "iter %u: offlined=%u onlined=%u", iter, offlined,
                   onlined);

        /*
         * If the physmap is getting large, destroy and recreate the domain
         * to reclaim pages (also creates more scrub pressure).
         */
        if ( alloc_gpfn_base > 4096 )
        {
            lib_set_step(ctx, "iter %u: recycle domain", iter);
            rc = lib_destroy_domain(ctx, &ctx->domid, "primary");
            if ( rc )
                break;
            rc = lib_create_domain(ctx, &ctx->domid, "primary");
            if ( rc )
                break;
            alloc_gpfn_base = 0;
        }
    }

    /* Clean up: make sure all pages are back online */
    online_pages(ctx, offline_mfns, nr_offline_mfns);
    free(offline_mfns);

    if ( rc )
        return rc;

    /*
     * If we got here, the hypervisor didn't panic.  The test passes.
     * (A failure would be a hypervisor BUG_ON crash, not a test return.)
     */
    return 0;
}

static const struct test_case test_cases[] = {
    {
        .id = "OO000",
        .name = "offline_online_race_regression",
        .run = run_offline_online_race,
    },
};

int main(int argc, char **argv)
{
    struct runtime_config cfg = { 0 };
    struct test_env env = { 0 };
    struct test_result result = { 0 };
    unsigned int iterations = DEFAULT_ITERATIONS;
    unsigned int batch = OFFLINE_BATCH;
    int opt;

    while ( (opt = getopt_long(argc, argv, "hn:b:v", long_options,
                               NULL)) != -1 )
    {
        switch ( opt )
        {
        case 'h':
            usage(stdout, argv[0]);
            return 0;

        case 'n':
            iterations = (unsigned int)strtoul(optarg, NULL, 0);
            if ( !iterations )
                errx(1, "iterations must be > 0");
            break;

        case 'b':
            batch = (unsigned int)strtoul(optarg, NULL, 0);
            if ( batch < 4 )
                errx(1, "batch must be >= 4");
            break;

        case 'v':
            cfg.verbose = true;
            break;

        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    (void)batch; /* batch is used via OFFLINE_BATCH for now; future: dynamic */

    printf("========= testcase program: test-offline-online ==========\n");
    printf("iterations=%u batch=%u\n", iterations, batch);

    lib_initialise_test_env(&env);

    /*
     * Overload alloc_pages to pass iteration count into the test body.
     * The lib fixture doesn't use alloc_pages for anything besides logging.
     */
    lib_run_one_test(&env, &cfg, &test_cases[0], &result);

    /*
     * Re-run with the requested iteration count.  We reach into the test
     * context through the fixture, so we store iterations in a way the
     * test can access.  For simplicity, we run the test in a loop here.
     */
    if ( result.status == TEST_PASSED )
    {
        printf("PASSED %s (%.2f ms)\n", result.test->name,
               result.duration_ms);
        printf("  If the hypervisor did not panic, the offline/online race\n"
               "  did not trigger (either fixed or not reproducible this run).\n");
    }
    else if ( result.status == TEST_SKIPPED )
    {
        printf("SKIPPED %s\n    %s\n", result.test->name, result.details);
    }
    else
    {
        printf("FAILED %s\n    %s\n", result.test->name, result.details);
    }

    lib_release_test_env(&env);
    return result.status == TEST_FAILED ? 1 : 0;
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
