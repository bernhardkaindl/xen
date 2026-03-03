/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * test-node-locks.c - NUMA node-parallel memory claim/alloc/free benchmark.
 *
 * For each online NUMA node, spawns a thread that repeatedly:
 *   1. Creates a domain
 *   2. Claims 2 GB of memory
 *   3. Populates 16x1GB + 500x2MB + 500x4KB pages from that node
 *   4. Resets the claim
 *   5. Destroys the domain
 *
 * Reports per-node mean iteration time and standard deviation.
 */

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xenctrl.h>
#include <xen-tools/common-macros.h>

/* --------------- defaults --------------- */

#define DEFAULT_ITERATIONS      20
#define DEFAULT_THREADS          1
#define DEFAULT_NR_1G            1
#define DEFAULT_NR_2M          500
#define DEFAULT_NR_4K          500

/* Page orders (in units of 4 KB base pages) */
#define ORDER_4K            0          /* 1 page   =   4 KB */
#define ORDER_2M            9          /* 512 pages =   2 MB */
#define ORDER_1G           18          /* 262144 pages = 1 GB */

/* Extra 2M pages to replace each 1G page when 1G is unavailable */
#define PAGES_2M_PER_1G    512

/* --------------- per-phase timing --------------- */

enum phase {
    PHASE_CREATE,
    PHASE_CLAIM,
    PHASE_POPULATE_1G,
    PHASE_POPULATE_2M,
    PHASE_POPULATE_4K,
    PHASE_DESTROY,
    NR_PHASES
};

static const char *phase_names[NR_PHASES] = {
    [PHASE_CREATE]      = "create",
    [PHASE_CLAIM]       = "claim",
    [PHASE_POPULATE_1G] = "populate_1g",
    [PHASE_POPULATE_2M] = "populate_2m",
    [PHASE_POPULATE_4K] = "populate_4k",
    [PHASE_DESTROY]     = "destroy",
};

struct phase_stats {
    double total_ms[NR_PHASES];
    unsigned long count[NR_PHASES];      /* calls for non-populate, extents for populate */
};

/* --------------- runtime configuration (set from CLI) --------------- */

static unsigned int num_iterations   = DEFAULT_ITERATIONS;
static unsigned int threads_per_node = DEFAULT_THREADS;
static unsigned long nr_1g_pages     = DEFAULT_NR_1G;
static unsigned long nr_2m_pages     = DEFAULT_NR_2M;
static unsigned long nr_4k_pages     = DEFAULT_NR_4K;

/*
 * Claim size in 4 KB pages.  Computed at startup to cover:
 *   nr_1g * 1GB + nr_2m * 2MB + nr_4k * 4KB + 20% headroom.
 */
static unsigned long claim_pages;

/* Domain creation template – same logic as test-mem-claim.c */
static struct xen_domctl_createdomain create_template = {
    .flags       = XEN_DOMCTL_CDF_hvm | XEN_DOMCTL_CDF_hap,
    .max_vcpus   = 1,
    .max_grant_frames = 1,
    .grant_opts  = XEN_DOMCTL_GRANT_version(1),
    .arch = {
#if defined(__x86_64__) || defined(__i386__)
        .emulation_flags = XEN_X86_EMU_LAPIC,
#endif
    },
};

/* --------------- shared state --------------- */

static atomic_bool error_flag = false;   /* any thread sets on failure */

static void sig_handler(int sig)
{
    (void)sig;
    atomic_store(&error_flag, true);
}

/* Per-thread context */
struct thread_ctx {
    unsigned int  node;
    unsigned int  thread_id;             /* thread index within node */
    unsigned int  num_iterations;
    double       *times_ms;              /* array[num_iterations] */
    double        mean_ms;
    double        stddev_ms;
    int           rc;                    /* 0 on success */
    char          errmsg[512];
    struct phase_stats stats;            /* per-phase timing */
};

/* --------------- helpers --------------- */

static double timespec_diff_ms(const struct timespec *a,
                               const struct timespec *b)
{
    double sec  = (double)(b->tv_sec  - a->tv_sec);
    double nsec = (double)(b->tv_nsec - a->tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

static void compute_stats(const double *samples, unsigned int n,
                          double *mean_out, double *stddev_out)
{
    double sum = 0.0, sum_sq = 0.0;

    for ( unsigned int i = 0; i < n; i++ )
        sum += samples[i];

    *mean_out = sum / n;

    for ( unsigned int i = 0; i < n; i++ )
    {
        double d = samples[i] - *mean_out;
        sum_sq += d * d;
    }

    *stddev_out = (n > 1) ? sqrt(sum_sq / (n - 1)) : 0.0;
}

/* --------------- domain helpers --------------- */

/*
 * Adjust the creation template once based on hardware capabilities.
 * Must be called before any thread starts.
 */
static void fixup_create_flags(const xc_physinfo_t *physinfo)
{
#if defined(__x86_64__) || defined(__i386__)
    if ( !(physinfo->capabilities & XEN_SYSCTL_PHYSCAP_hap) )
        create_template.flags &= ~XEN_DOMCTL_CDF_hap;

    if ( !(physinfo->capabilities &
           (XEN_SYSCTL_PHYSCAP_hap | XEN_SYSCTL_PHYSCAP_shadow)) ||
         !(physinfo->capabilities & XEN_SYSCTL_PHYSCAP_hvm) )
    {
        create_template.flags &= ~XEN_DOMCTL_CDF_hvm;
        create_template.arch.emulation_flags = 0;
    }
#endif
}

static int domain_create(xc_interface *xch, uint32_t *domid,
                        char *errmsg, size_t errlen)
{
    struct xen_domctl_createdomain create = create_template;
    int rc;

    *domid = DOMID_INVALID;
    rc = xc_domain_create(xch, domid, &create);
    if ( rc )
    {
        snprintf(errmsg, errlen, "xc_domain_create failed: %d - %s",
                 errno, strerror(errno));
        return rc;
    }

    rc = xc_domain_setmaxmem(xch, *domid, -1);
    if ( rc )
    {
        snprintf(errmsg, errlen, "xc_domain_setmaxmem(d%u) failed: %d - %s",
                 *domid, errno, strerror(errno));
        xc_domain_destroy(xch, *domid);
        *domid = DOMID_INVALID;
    }

    return rc;
}

static int domain_destroy(xc_interface *xch, uint32_t *domid)
{
    int rc;

    if ( *domid == DOMID_INVALID )
        return 0;

    rc = xc_domain_destroy(xch, *domid);
    *domid = DOMID_INVALID;
    return rc;
}

/* --------------- claim helpers --------------- */

static int claim_memory(xc_interface *xch, uint32_t domid,
                        unsigned long nr_pages)
{
    return xc_domain_claim_pages(xch, domid, nr_pages);
}

static int reset_claim(xc_interface *xch, uint32_t domid)
{
    return xc_domain_claim_pages(xch, domid, 0);
}

/* --------------- populate / free helpers --------------- */

/*
 * Populate @nr_extents extents of @order from NUMA @node into @domid.
 * @pfns must point to an array of @nr_extents entries; on entry each
 * element is the guest PFN base, on return it holds the MFN.
 */
static int populate_pages(xc_interface *xch, uint32_t domid,
                          xen_pfn_t *pfns, unsigned long nr_extents,
                          unsigned int order, unsigned int node)
{
    unsigned int memflags = XENMEMF_exact_node(node);

    return xc_domain_populate_physmap_exact(xch, domid, nr_extents,
                                            order, memflags, pfns);
}

/* --------------- per-iteration work --------------- */

/*
 * Initialise the pfn array with sequential guest PFN bases for extents
 * of size (1 << order) starting at @base_pfn.  Returns the next free PFN.
 */
static xen_pfn_t init_pfns(xen_pfn_t *pfns, unsigned long count,
                            unsigned int order, xen_pfn_t base_pfn)
{
    unsigned long pages_per_extent = 1UL << order;

    for ( unsigned long i = 0; i < count; i++ )
        pfns[i] = base_pfn + i * pages_per_extent;

    return base_pfn + count * pages_per_extent;
}

static int run_iteration(xc_interface *xch, unsigned int node,
                         xen_pfn_t *pfns_1g, xen_pfn_t *pfns_2m,
                         xen_pfn_t *pfns_4k,
                         struct phase_stats *stats,
                         char *errmsg, size_t errlen)
{
    uint32_t domid;
    int rc;
    xen_pfn_t next_pfn = 0;
    bool has_1g = false;
    unsigned long nr_2m_actual = nr_2m_pages;
    struct timespec ts, te;

    /* 1. Create domain */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rc = domain_create(xch, &domid, errmsg, errlen);
    clock_gettime(CLOCK_MONOTONIC, &te);
    if ( rc )
        return rc;
    stats->total_ms[PHASE_CREATE] += timespec_diff_ms(&ts, &te);
    stats->count[PHASE_CREATE]++;

    /* 2. Claim memory */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rc = claim_memory(xch, domid, claim_pages);
    clock_gettime(CLOCK_MONOTONIC, &te);
    if ( rc )
    {
        snprintf(errmsg, errlen,
                 "claim_memory(d%u, %lu pages) failed: %d - %s",
                 domid, claim_pages, errno, strerror(errno));
        goto out;
    }
    stats->total_ms[PHASE_CLAIM] += timespec_diff_ms(&ts, &te);
    stats->count[PHASE_CLAIM]++;

    /* 3. Populate pages from this node */

    /* Try 1G pages first; fall back to extra 2M pages on failure */
    if ( nr_1g_pages )
    {
        next_pfn = init_pfns(pfns_1g, nr_1g_pages, ORDER_1G, next_pfn);
        clock_gettime(CLOCK_MONOTONIC, &ts);
        rc = populate_pages(xch, domid, pfns_1g, nr_1g_pages, ORDER_1G, node);
        clock_gettime(CLOCK_MONOTONIC, &te);
        if ( rc )
        {
            if ( errno == EBUSY )
            {
                /*
                 * 1G pages not available or busy due to contention —
                 * replace with 512 x 2M pages per 1G page.
                 * Reset next_pfn since we didn't allocate the 1G extent.
                 */
                nr_2m_actual = nr_2m_pages + nr_1g_pages * PAGES_2M_PER_1G;
                next_pfn = 0;
            }
            else
            {
                snprintf(errmsg, errlen,
                         "populate 1G (d%u, node %u, order %u) failed: %d - %s",
                         domid, node, ORDER_1G, errno, strerror(errno));
                goto out;
            }
        }
        else
        {
            has_1g = true;
            stats->total_ms[PHASE_POPULATE_1G] += timespec_diff_ms(&ts, &te);
            stats->count[PHASE_POPULATE_1G] += nr_1g_pages;
        }
    }

    next_pfn = init_pfns(pfns_2m, nr_2m_actual, ORDER_2M, next_pfn);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rc = populate_pages(xch, domid, pfns_2m, nr_2m_actual, ORDER_2M, node);
    clock_gettime(CLOCK_MONOTONIC, &te);
    if ( rc )
    {
        snprintf(errmsg, errlen,
                 "populate 2M (d%u, node %u, %lu extents, order %u) "
                 "failed: %d - %s",
                 domid, node, nr_2m_actual, ORDER_2M,
                 errno, strerror(errno));
        goto out;
    }
    stats->total_ms[PHASE_POPULATE_2M] += timespec_diff_ms(&ts, &te);
    stats->count[PHASE_POPULATE_2M] += nr_2m_actual;

    next_pfn = init_pfns(pfns_4k, nr_4k_pages, ORDER_4K, next_pfn);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rc = populate_pages(xch, domid, pfns_4k, nr_4k_pages, ORDER_4K, node);
    clock_gettime(CLOCK_MONOTONIC, &te);
    if ( rc )
    {
        snprintf(errmsg, errlen,
                 "populate 4K (d%u, node %u, %lu extents, order %u) "
                 "failed: %d - %s",
                 domid, node, nr_4k_pages, ORDER_4K,
                 errno, strerror(errno));
        goto out;
    }
    stats->total_ms[PHASE_POPULATE_4K] += timespec_diff_ms(&ts, &te);
    stats->count[PHASE_POPULATE_4K] += nr_4k_pages;

    /* 4. Reset the claim */
    rc = reset_claim(xch, domid);
    if ( rc )
    {
        snprintf(errmsg, errlen,
                 "reset_claim(d%u) failed: %d - %s",
                 domid, errno, strerror(errno));
        goto out;
    }
 out:
    /* 5. Destroy domain */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    domain_destroy(xch, &domid);
    clock_gettime(CLOCK_MONOTONIC, &te);
    stats->total_ms[PHASE_DESTROY] += timespec_diff_ms(&ts, &te);
    stats->count[PHASE_DESTROY]++;
    return rc;
}

/* --------------- thread entry point --------------- */

static void *thread_func(void *arg)
{
    struct thread_ctx *ctx = arg;
    xc_interface *xch;

    xch = xc_interface_open(NULL, NULL, 0);
    if ( !xch )
    {
        snprintf(ctx->errmsg, sizeof(ctx->errmsg),
                 "Node %u: xc_interface_open failed", ctx->node);
        ctx->rc = -1;
        atomic_store(&error_flag, true);
        return NULL;
    }

    /*
     * Heap-allocate PFN arrays once per thread (too large for the stack
     * with 16 x 1G extents fallback to 2M).
     */
    size_t max_2m = nr_2m_pages + nr_1g_pages * PAGES_2M_PER_1G;
    xen_pfn_t *pfns_1g = malloc(nr_1g_pages ? nr_1g_pages * sizeof(*pfns_1g)
                                             : sizeof(*pfns_1g));
    xen_pfn_t *pfns_2m = malloc(max_2m * sizeof(*pfns_2m));
    xen_pfn_t *pfns_4k = malloc(nr_4k_pages ? nr_4k_pages * sizeof(*pfns_4k)
                                             : sizeof(*pfns_4k));

    if ( !pfns_1g || !pfns_2m || !pfns_4k )
    {
        snprintf(ctx->errmsg, sizeof(ctx->errmsg),
                 "Node %u: malloc for PFN arrays failed", ctx->node);
        ctx->rc = -1;
        atomic_store(&error_flag, true);
        goto out_close;
    }

    /* Warmup iteration (not counted in stats) */
    {
        char itermsg[256] = {};

        if ( !atomic_load(&error_flag) )
        {
            struct phase_stats warmup_stats = {};

            ctx->rc = run_iteration(xch, ctx->node, pfns_1g, pfns_2m,
                                    pfns_4k, &warmup_stats,
                                    itermsg, sizeof(itermsg));
            if ( ctx->rc )
            {
                snprintf(ctx->errmsg, sizeof(ctx->errmsg),
                         "Node %u, warmup: %s", ctx->node, itermsg);
                atomic_store(&error_flag, true);
                goto out_close;
            }
            printf("  Node %u T%u: warmup done\n",
                   ctx->node, ctx->thread_id);
        }
    }

    for ( unsigned int i = 0; i < ctx->num_iterations; i++ )
    {
        struct timespec t_start, t_end;
        char itermsg[256] = {};

        if ( atomic_load(&error_flag) )
        {
            /* Record how many iterations actually completed */
            ctx->num_iterations = i;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_start);
        ctx->rc = run_iteration(xch, ctx->node, pfns_1g, pfns_2m, pfns_4k,
                                &ctx->stats, itermsg, sizeof(itermsg));
        clock_gettime(CLOCK_MONOTONIC, &t_end);

        if ( ctx->rc )
        {
            snprintf(ctx->errmsg, sizeof(ctx->errmsg),
                     "Node %u, iter %u: %s",
                     ctx->node, i, itermsg);
            ctx->num_iterations = i;
            atomic_store(&error_flag, true);
            break;
        }

        ctx->times_ms[i] = timespec_diff_ms(&t_start, &t_end);

        {
            xc_physinfo_t info;
            uint64_t scrub = 0;

            if ( !xc_physinfo(xch, &info) )
                scrub = info.scrub_pages;

            printf("  Node %u T%u: iter %u/%u  %.1f ms  "
                   "(scrub: %"PRIu64" pages, %.1f MB)\n",
                   ctx->node, ctx->thread_id, i + 1,
                   ctx->num_iterations, ctx->times_ms[i],
                   scrub, (double)scrub * XC_PAGE_SIZE / MB(1));
        }
    }

    if ( !ctx->rc && ctx->num_iterations > 0 )
        compute_stats(ctx->times_ms, ctx->num_iterations,
                      &ctx->mean_ms, &ctx->stddev_ms);

 out_close:
    free(pfns_1g);
    free(pfns_2m);
    free(pfns_4k);
    xc_interface_close(xch);
    return NULL;
}

/* --------------- usage / option parsing --------------- */

static const struct option long_opts[] = {
    { "help",       no_argument,       NULL, 'h' },
    { "iterations", required_argument, NULL, 'i' },
    { "threads",    required_argument, NULL, 't' },
    { "1g-pages",   required_argument, NULL, 'g' },
    { "2m-pages",   required_argument, NULL, 'm' },
    { "4k-pages",   required_argument, NULL, 'k' },
    { NULL, 0, NULL, 0 }
};

static void usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n"
           "\n"
           "NUMA node-parallel memory claim/alloc/free benchmark.\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help and exit\n"
           "  -i, --iterations N    Iterations per thread   (default: %u)\n"
           "  -t, --threads N       Threads per NUMA node   (default: %u)\n"
           "  -g, --1g-pages N      1 GB pages per iteration (default: %lu)\n"
           "  -m, --2m-pages N      2 MB pages per iteration (default: %lu)\n"
           "  -k, --4k-pages N      4 KB pages per iteration (default: %lu)\n"
           "\n"
           "The claim size is computed automatically from the page counts\n"
           "with 20%% headroom.  If 1 GB pages are unavailable they are\n"
           "transparently replaced by 2 MB pages.\n",
           prog,
           DEFAULT_ITERATIONS, DEFAULT_THREADS,
           (unsigned long)DEFAULT_NR_1G,
           (unsigned long)DEFAULT_NR_2M,
           (unsigned long)DEFAULT_NR_4K);
}

static void parse_opts(int argc, char **argv)
{
    int c;

    while ( (c = getopt_long(argc, argv, "hi:t:g:m:k:",
                             long_opts, NULL)) != -1 )
    {
        switch ( c )
        {
        case 'h':
            usage(argv[0]);
            exit(0);
        case 'i':
            num_iterations = (unsigned int)atoi(optarg);
            if ( !num_iterations )
                errx(1, "Invalid iteration count: %s", optarg);
            break;
        case 't':
            threads_per_node = (unsigned int)atoi(optarg);
            if ( !threads_per_node )
                errx(1, "Invalid thread count: %s", optarg);
            break;
        case 'g':
            nr_1g_pages = strtoul(optarg, NULL, 0);
            break;
        case 'm':
            nr_2m_pages = strtoul(optarg, NULL, 0);
            break;
        case 'k':
            nr_4k_pages = strtoul(optarg, NULL, 0);
            break;
        default:
            usage(argv[0]);
            exit(1);
        }
    }

    /*
     * Compute claim: total 4K-equivalent pages with 20% headroom.
     * Each 1G extent = 2^18 pages, each 2M extent = 2^9 pages.
     */
    {
        unsigned long base = nr_1g_pages * (1UL << ORDER_1G)
                           + nr_2m_pages * (1UL << ORDER_2M)
                           + nr_4k_pages;
        claim_pages = base + base / 5;   /* +20% */
        if ( !claim_pages )
            errx(1, "Nothing to allocate — set at least one page count");
    }
}

/* --------------- main --------------- */

int main(int argc, char **argv)
{
    xc_interface *xch;
    xc_physinfo_t physinfo;
    unsigned int num_nodes = 0, online_nodes = 0;
    unsigned int total_threads;
    xc_meminfo_t *meminfo = NULL;
    struct thread_ctx *ctxs = NULL;
    pthread_t *threads = NULL;
    int rc, ret = 0;

    parse_opts(argc, argv);

    printf("NUMA node-locks performance test\n"
           "  iterations:  %u\n"
           "  threads/node: %u\n"
           "  1G pages:    %lu\n"
           "  2M pages:    %lu\n"
           "  4K pages:    %lu\n"
           "  claim:       %lu pages (%.1f GB)\n",
           num_iterations, threads_per_node,
           nr_1g_pages, nr_2m_pages, nr_4k_pages,
           claim_pages, (double)claim_pages * XC_PAGE_SIZE / GB(1));

    /* Install signal handlers so Ctrl-C stops threads gracefully */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    xch = xc_interface_open(NULL, NULL, 0);
    if ( !xch )
        err(1, "xc_interface_open");

    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        err(1, "xc_physinfo");

    fixup_create_flags(&physinfo);

    /* Discover NUMA topology */
    rc = xc_numainfo(xch, &num_nodes, NULL, NULL);
    if ( rc || !num_nodes )
        err(1, "xc_numainfo");

    meminfo = calloc(num_nodes, sizeof(*meminfo));
    if ( !meminfo )
        err(1, "calloc meminfo");

    rc = xc_numainfo(xch, &num_nodes, meminfo, NULL);
    if ( rc )
        err(1, "xc_numainfo (2)");

    /* Count online nodes (those with memory) */
    for ( unsigned int i = 0; i < num_nodes; i++ )
        if ( meminfo[i].memsize )
            online_nodes++;

    if ( !online_nodes )
        errx(1, "No online NUMA nodes found");

    printf("NUMA nodes: %u total, %u online\n", num_nodes, online_nodes);

    /* Check that every online node has enough free memory for the claims */
    for ( unsigned int i = 0; i < num_nodes; i++ )
    {
        if ( !meminfo[i].memsize )
            continue;
        unsigned long free_pages = meminfo[i].memfree / XC_PAGE_SIZE;

        printf("  Node %u: %lu MB total, %lu MB free\n",
               i,
               (unsigned long)(meminfo[i].memsize / (1024 * 1024)),
               (unsigned long)(meminfo[i].memfree / (1024 * 1024)));

        if ( free_pages < claim_pages )
        {
            printf("  WARNING: Node %u has only %lu MB free, "
                   "need %lu MB\n",
                   i, (unsigned long)(meminfo[i].memfree / (1024 * 1024)),
                   (unsigned long)(claim_pages * XC_PAGE_SIZE / (1024 * 1024)));
        }
    }

    free(meminfo);

    /* Allocate threads and contexts */
    total_threads = num_nodes * threads_per_node;
    ctxs    = calloc(total_threads, sizeof(*ctxs));
    threads = calloc(total_threads, sizeof(*threads));
    if ( !ctxs || !threads )
        err(1, "calloc threads");

    for ( unsigned int n = 0; n < num_nodes; n++ )
    {
        for ( unsigned int t = 0; t < threads_per_node; t++ )
        {
            unsigned int idx = n * threads_per_node + t;
            ctxs[idx].node = n;
            ctxs[idx].thread_id = t;
            ctxs[idx].num_iterations = num_iterations;
            ctxs[idx].times_ms = calloc(num_iterations, sizeof(double));
            if ( !ctxs[idx].times_ms )
                err(1, "calloc times");
        }
    }

    /* Spawn threads */
    printf("\nStarting %u threads (%u per node, %u nodes) ...\n",
           total_threads, threads_per_node, num_nodes);

    for ( unsigned int i = 0; i < total_threads; i++ )
    {
        rc = pthread_create(&threads[i], NULL, thread_func, &ctxs[i]);
        if ( rc )
        {
            errno = rc;
            err(1, "pthread_create for node %u T%u",
                ctxs[i].node, ctxs[i].thread_id);
        }
    }

    /* Wait for all threads */
    for ( unsigned int i = 0; i < total_threads; i++ )
        pthread_join(threads[i], NULL);

    /* Report results */
    printf("\n%-6s  %-4s  %12s  %12s  %s\n",
           "Node", "T#", "Mean (ms)", "StdDev (ms)", "Status");
    printf("------  ----  ------------  ------------  ------\n");

    for ( unsigned int i = 0; i < total_threads; i++ )
    {
        if ( ctxs[i].rc )
        {
            printf("%-6u  %-4u  %12s  %12s  FAIL: %s\n",
                   ctxs[i].node, ctxs[i].thread_id,
                   "-", "-", ctxs[i].errmsg);
            ret = 1;
        }
        else
        {
            printf("%-6u  %-4u  %12.2f  %12.2f  OK\n",
                   ctxs[i].node, ctxs[i].thread_id,
                   ctxs[i].mean_ms, ctxs[i].stddev_ms);
        }
    }

    /* Per-phase average timing summary */
    {
        struct phase_stats global = {};
        unsigned int total_iters = 0;

        for ( unsigned int i = 0; i < total_threads; i++ )
        {
            if ( ctxs[i].rc )
                continue;
            for ( unsigned int p = 0; p < NR_PHASES; p++ )
            {
                global.total_ms[p] += ctxs[i].stats.total_ms[p];
                global.count[p] += ctxs[i].stats.count[p];
            }
            total_iters += ctxs[i].num_iterations;
        }

        printf("\nPhase timing summary (%u iterations across %u threads):\n",
               total_iters, total_threads);
        printf("  %-14s  %10s  %10s  %10s\n",
               "Phase", "Total (ms)", "Count", "Avg (ms)");
        printf("  %-14s  %10s  %10s  %10s\n",
               "--------------", "----------", "----------", "----------");

        for ( unsigned int p = 0; p < NR_PHASES; p++ )
        {
            if ( global.count[p] )
                printf("  %-14s  %10.1f  %10lu  %10.4f\n",
                       phase_names[p],
                       global.total_ms[p],
                       global.count[p],
                       global.total_ms[p] / global.count[p]);
            else
                printf("  %-14s  %10s  %10s  %10s\n",
                       phase_names[p], "-", "-", "-");
        }
    }

    /* Cleanup */
    for ( unsigned int i = 0; i < total_threads; i++ )
        free(ctxs[i].times_ms);
    free(ctxs);
    free(threads);
    xc_interface_close(xch);

    if ( ret )
        printf("\nFAILED — see errors above.\n");
    else
        printf("\nPASSED\n");

    return ret;
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
