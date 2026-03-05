/* SPDX-License-Identifier: GPL-2.0-only */
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <xenctrl.h>
#include <xenforeignmemory.h>
#include <xengnttab.h>
#include <xen-tools/common-macros.h>

static unsigned int nr_failures;
#define fail(fmt, ...)                          \
({                                              \
    nr_failures++;                              \
    (void)printf(fmt, ##__VA_ARGS__);           \
})

#define log_check(fmt, ...)                     \
    do {                                        \
        printf("L%u: " fmt "\n",              \
               __LINE__, ##__VA_ARGS__);        \
    } while ( 0 )

#define MB_PAGES(x) (MB(x) / XC_PAGE_SIZE)

#define CLAIM_TEST_ORDER 9 /* 2M */

static xc_interface *xch;
static uint32_t domid = DOMID_INVALID;

static xc_physinfo_t physinfo;
static unsigned int claim_test_node;

static struct xen_domctl_createdomain create = {
    .flags = XEN_DOMCTL_CDF_hvm | XEN_DOMCTL_CDF_hap,
    .max_vcpus = 1,
    .max_grant_frames = 1,
    .grant_opts = XEN_DOMCTL_GRANT_version(1),

    .arch = {
#if defined(__x86_64__) || defined(__i386__)
        .emulation_flags = XEN_X86_EMU_LAPIC,
#endif
    },
};

typedef int (*claim_fn_t)(xc_interface *xch, uint32_t domid,
                          unsigned long pages);

static int get_node_free_pages(unsigned int node, unsigned long *free_pages,
                               unsigned long *total_pages);


static void _print_claim_memory_state(xc_interface *xch, int node,
                                      unsigned int line)
{
    xc_physinfo_t p;
    unsigned long node_free, node_total;
    int rc;

    rc = xc_physinfo(xch, &p);
    if ( rc )
    {
        printf("L%u:     Memory state unavailable: %d - %s\n",
               line, errno, strerror(errno));
        return;
    }

    if ( node >= 0 )
    {
        rc = get_node_free_pages(node, &node_free, &node_total);
        if ( rc )
            return fail("  Failed to query free pages on node %u: %d - %s\n",
                        claim_test_node, errno, strerror(errno));

        printf("L%u:     Memory: free=%"PRIu64", claimed=%"PRIu64""
               ", node %d free=%lu / %lu\n",
               line, p.free_pages, p.outstanding_pages,
               node, node_free, node_total);
    }
    else
        printf("L%u:     Memory: free=%"PRIu64", claimed=%"PRIu64"\n",
               line, p.free_pages, p.outstanding_pages);
}

#define print_claim_memory_state(xch, node) \
    _print_claim_memory_state(xch, node, __LINE__)

/* Wrapper function to test claiming memory using xc_domain_claim_pages. */
static int wrap_claim_pages(xc_interface *xch,
                            uint32_t domid,
                            unsigned long pages)
{
    int rc;

    print_claim_memory_state(xch, 0);
    rc = xc_domain_claim_pages(xch, domid, pages);
    log_check("xc_domain_claim_pages(%u, %lu) = %d", domid, pages, rc);
    print_claim_memory_state(xch, 0);
    return rc;
}

/* Wrapper function to test claiming memory using xc_domain_claim_memory. */
static int wrap_claim_memory(xc_interface *xch,
                             uint32_t domid,
                             unsigned long pages)
{
    memory_claim_t claim[] = {
        { .pages = pages, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL }
    };

    log_check("Check xc_domain_claim_memory() with nr_claims == 0 to fail");
    int rc = xc_domain_claim_memory(xch, domid, 0, NULL);
    if ( rc != -1 || errno != EINVAL )
    {
        fail("Expected nr_claims == 0 to fail with EINVAL\n");
        return rc;
    }

    log_check("Check xc_domain_claim_memory() accepts a valid global claim");
    print_claim_memory_state(xch, -1);
    return xc_domain_claim_memory(xch, domid, 1, claim);
}

/* Wrapper to test xc_domain_claim_memory() with a NUMA node */
static int wrap_claim_memory_node(xc_interface *xch,
                                  uint32_t domid,
                                  unsigned long pages)
{
    int rc;
    memory_claim_t claims[UINT8_MAX + 1] = {}; /* + 1 to test overflow check */

    log_check("Check xc_domain_claim_memory() rejects non-present node");
    claims[0] = (memory_claim_t) { .pages = pages, .node = physinfo.nr_nodes };
    rc = xc_domain_claim_memory(xch, domid, 1, claims);
    if ( rc != -1 || errno != ENOENT )
    {
        fail("Expected claim failure on invalid node to fail with ENOENT\n");
        return rc;
    }

    log_check("Check xc_domain_claim_memory() rejects nr_claims > UINT8_MAX");
    rc = xc_domain_claim_memory(xch, domid,
                                XEN_DOMCTL_CLAIM_MEMORY_MAX_CLAIMS + 1, claims);
    if ( rc != -1 || errno != EOPNOTSUPP )
    {
        fail("Expected nr_claims = UINT8_MAX + 1 to fail with EOPNOTSUPP\n");
        return rc;
    }

    claims[0].node = UINT8_MAX + 1;
    log_check("Check xc_domain_claim_memory() rejects node > UINT8_MAX");
    rc = xc_domain_claim_memory(xch, domid, 1, claims);
    if ( rc != -1 || errno != ENOENT )
    {
        fail("Expected node == UINT8_MAX + 1 to fail with ENOENT\n");
        return rc;
    }

    log_check("Check xc_domain_claim_memory() rejects pages > INT32_MAX");
    claims[0] = (memory_claim_t) { .pages = INT32_MAX + 1UL, .node = 0 };
    rc = xc_domain_claim_memory(xch, domid, 1, claims);
    if ( rc != -1 || errno != ENOMEM )
    {
        fail("Expected ENOMEM with pages > INT32_MAX\n");
        return rc;
    }

    log_check("Test xc_domain_claim_memory() to reject pad != 0 with EINVAL");
    claims[0] = (memory_claim_t) { .pages = pages, .node = claim_test_node };
    claims[0].pad = 1;
    rc = xc_domain_claim_memory(xch, domid, 1, claims);
    if ( rc != -1 || errno != EINVAL )
    {
        fail("Expected EINVAL with pad not set to zero\n");
        return rc;
    }

    /* Pass a valid claim for the selected node and continue the test */
    claims[0] = (memory_claim_t) { .pages = pages, .node = claim_test_node };
    log_check("Check xc_domain_claim_memory(%lu,%u) accepts valid node claim",
              pages, claim_test_node);
    print_claim_memory_state(xch, (int)claim_test_node);
    return xc_domain_claim_memory(xch, domid, 1, claims);
}

static int get_node_free_pages(unsigned int node, unsigned long *free_pages,
                               unsigned long *total_pages)
{
    int rc;
    unsigned int num_nodes = 0;
    xc_meminfo_t *meminfo;

    rc = xc_numainfo(xch, &num_nodes, NULL, NULL);
    if ( rc )
        return rc;

    if ( node >= num_nodes )
    {
        errno = EINVAL;
        return -1;
    }

    meminfo = calloc(num_nodes, sizeof(*meminfo));
    if ( !meminfo )
        return -1;

    rc = xc_numainfo(xch, &num_nodes, meminfo, NULL);
    if ( rc )
        goto out;

    *free_pages = meminfo[node].memfree / XC_PAGE_SIZE;
    if ( total_pages )
        *total_pages = meminfo[node].memsize / XC_PAGE_SIZE;

 out:
    free(meminfo);
    return rc;
}

static void run_test(claim_fn_t claim_call_wrapper, const char *claim_name,
                     bool node_claim)
{
    int rc;
    uint64_t free_heap_bytes;
    unsigned long free_pages, claim_pages, total_pages;
    const unsigned long request_pages = 1UL << CLAIM_TEST_ORDER;

    printf("  Testing %s\n", claim_name);
    /*
     * Check that the system is quiescent.  Outstanding claims is a global
     * field.
     */
    log_check("Check xc_physinfo() obtains initial free and outstanding pages");
    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        return fail("Failed to obtain physinfo: %d - %s\n",
                    errno, strerror(errno));

    printf("Free pages: %"PRIu64", Outstanding claims: %"PRIu64"\n",
           physinfo.free_pages, physinfo.outstanding_pages);

    if ( physinfo.outstanding_pages )
        return fail("  Test needs running on a quiescent system\n");

    /*
     * We want any arbitrary domain.  Start with HVM/HAP, falling back to
     * HVM/Shadow and then to PV.  The dom0 running this test case is one of
     * these modes.
     */
#if defined(__x86_64__) || defined(__i386__)
    if ( !(physinfo.capabilities & XEN_SYSCTL_PHYSCAP_hap) )
        create.flags &= ~XEN_DOMCTL_CDF_hap;

    if ( !(physinfo.capabilities & (XEN_SYSCTL_PHYSCAP_hap|
                                    XEN_SYSCTL_PHYSCAP_shadow)) ||
         !(physinfo.capabilities & XEN_SYSCTL_PHYSCAP_hvm) )
    {
        create.flags &= ~XEN_DOMCTL_CDF_hvm;
        create.arch.emulation_flags = 0;
    }
#endif

    log_check("Check xc_domain_create() creates the test domain");
    rc = xc_domain_create(xch, &domid, &create);
    if ( rc )
        return fail("  Domain create failure: %d - %s\n",
                    errno, strerror(errno));

    log_check("Check xc_domain_setmaxmem() sets maxmem to unlimited");
    rc = xc_domain_setmaxmem(xch, domid, -1);
    if ( rc )
        return fail("  Failed to set maxmem: %d - %s\n",
                    errno, strerror(errno));

    printf("  Created d%u\n", domid);

    /*
     * Creating a domain shouldn't change the claim.  Check it's still 0.
     */
    log_check("Check xc_physinfo() confirms no outstanding claim after create");
    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        return fail("  Failed to obtain physinfo: %d - %s\n",
                    errno, strerror(errno));

    if ( physinfo.outstanding_pages )
        return fail("  Unexpected outstanding claim of %"PRIu64" pages\n",
                    physinfo.outstanding_pages);

    log_check("Check xc_availheap() reports free heap pages for claim sizing");
    rc = xc_availheap(xch, 0, 0, node_claim ? (int)claim_test_node : -1,
                      &free_heap_bytes);
    if ( rc )
        return fail("  Failed to query available heap: %d - %s\n",
                    errno, strerror(errno));

    free_pages = free_heap_bytes / XC_PAGE_SIZE;
    if ( node_claim )
    {
        rc = get_node_free_pages(claim_test_node, &free_pages, &total_pages);
        if ( rc )
            return fail("  Failed to query free pages on node %u: %d - %s\n",
                        claim_test_node, errno, strerror(errno));
        log_check("Node %u has %lu free pages out of %lu total pages\n",
                  claim_test_node, free_pages, total_pages);
    }

    if ( free_pages <= request_pages + 1 )
        return fail("  Not enough free pages (%lu) to test %s claims\n",
                    free_pages, node_claim ? "node" : "global");

    claim_pages = free_pages - request_pages + 1;

    rc = claim_call_wrapper(xch, domid, claim_pages);
    if ( rc )
        return fail("       Failed to claim %lu: %d - %s\n\n",
                    claim_pages, errno, strerror(errno));

    log_check("Check xc_physinfo() reflects requested outstanding claim");
    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        return fail("  Failed to obtain physinfo: %d - %s\n",
                    errno, strerror(errno));

    if ( physinfo.outstanding_pages != claim_pages )
        return fail("  Expected claim to be %lu pages, got %"PRIu64" pages\n",
                    claim_pages, physinfo.outstanding_pages);
    {
        uint32_t other_domid = DOMID_INVALID;
        xen_pfn_t other_ram[] = { 0 };
        unsigned int memflags = node_claim ?
                                XENMEMF_exact_node(claim_test_node) : 0;

        log_check("Check xc_domain_create() creates a second test domain");
        rc = xc_domain_create(xch, &other_domid, &create);
        if ( rc )
            return fail("  Second domain create failure: %d - %s\n",
                        errno, strerror(errno));

        log_check("Check xc_domain_setmaxmem() configures second test domain");
        rc = xc_domain_setmaxmem(xch, other_domid, -1);
        if ( rc )
        {
            fail("  Failed to set maxmem for second domain: %d - %s\n",
                 errno, strerror(errno));
            goto destroy_other;
        }

        log_check("Check xc_domain_populate_physmap_exact() blocked by claim");
        rc = xc_domain_populate_physmap_exact(
            xch, other_domid, ARRAY_SIZE(other_ram), CLAIM_TEST_ORDER,
            memflags, other_ram);
        if ( rc == 0 )
            fail("  Expected %s claim to block second-domain allocation\n",
                 node_claim ? "node" : "global");

 destroy_other:
        log_check("Check xc_domain_destroy() destroys second test domain");
        rc = xc_domain_destroy(xch, other_domid);
        if ( rc )
            return fail("  Failed to destroy second domain: %d - %s\n",
                        errno, strerror(errno));
    }

    /*
     * Allocate one CLAIM_TEST_ORDER chunk to the domain. This should reduce
     * the outstanding claim by request_pages. For node claims, request memory
     * from the claimed node.
     */
    xen_pfn_t ram[] = { 0 };
    log_check("Check xc_domain_populate_physmap_exact() consumes from claim");
    rc = xc_domain_populate_physmap_exact(
        xch, domid, ARRAY_SIZE(ram), CLAIM_TEST_ORDER,
        node_claim ? XENMEMF_node(claim_test_node) : 0, ram);
    if ( rc )
        return fail("  Failed to populate physmap domain: %d - %s\n",
                    errno, strerror(errno));

    log_check("Check xc_physinfo() shows reduced claim after allocation");
    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        return fail("  Failed to obtain physinfo: %d - %s\n",
                    errno, strerror(errno));

    if ( physinfo.outstanding_pages != claim_pages - request_pages )
        return fail("  Expected claim to be %lu pages, got %"PRIu64" pages\n",
                    claim_pages - request_pages, physinfo.outstanding_pages);

    /*
     * Destroying the domain should release the outstanding 2M claim.
     */
    log_check("Check xc_domain_destroy() releases outstanding claim");
    rc = xc_domain_destroy(xch, domid);

    /* Cancel the cleanup path, even in the case of an error. */
    domid = DOMID_INVALID;

    if ( rc )
        return fail("  Failed to destroy domain: %d - %s\n",
                    errno, strerror(errno));

    log_check("Check xc_physinfo() confirms claims returned to zero");
    rc = xc_physinfo(xch, &physinfo);
    if ( rc )
        return fail("  Failed to obtain physinfo: %d - %s\n",
                    errno, strerror(errno));

    if ( physinfo.outstanding_pages )
        return fail("  Expected no outstanding claim, got %"PRIu64" pages\n",
                    physinfo.outstanding_pages);
}

int main(int argc, char **argv)
{
    int rc;
    unsigned int num_nodes = 0;
    xc_meminfo_t *meminfo = NULL;

    printf("Memory claims tests\n");

    log_check("Check xc_interface_open() initializes Xen control interface");
    xch = xc_interface_open(NULL, NULL, 0);

    if ( !xch )
        err(1, "xc_interface_open");

    log_check("Check xc_numainfo() obtains NUMA node count");
    rc = xc_numainfo(xch, &num_nodes, NULL, NULL);
    if ( rc || !num_nodes )
        err(1, "xc_numainfo");

    meminfo = calloc(num_nodes, sizeof(*meminfo));
    if ( !meminfo )
        err(1, "calloc");

    log_check("Check xc_numainfo() fills per-node memory information");
    rc = xc_numainfo(xch, &num_nodes, meminfo, NULL);
    if ( rc )
        err(1, "xc_numainfo");

    claim_test_node = 0;
    for ( unsigned int i = 1; i < num_nodes; i++ )
    {
        if ( meminfo[i].memfree > meminfo[claim_test_node].memfree )
            claim_test_node = i;
    }

    free(meminfo);

    struct {
        claim_fn_t fn;
        const char *name;
        bool node_claim;
    } tests[] = {
        {
            .fn = wrap_claim_pages,
            .name = "xc_domain_claim_pages",
            .node_claim = false,
        },
        {
            .fn = wrap_claim_memory,
            .name = "xc_domain_claim_memory",
            .node_claim = false,
        },
        {
            .fn = wrap_claim_memory_node,
            .name = "xc_domain_claim_memory_node",
            .node_claim = true,
        },
    };
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);
    for ( size_t i = 0; i < num_tests; i++ )
    {
        run_test(tests[i].fn, tests[i].name, tests[i].node_claim);
        if ( domid != DOMID_INVALID )
        {
            log_check("Check xc_domain_destroy() cleans up test domain");
            rc = xc_domain_destroy(xch, domid);
            if ( rc )
                fail("  Failed to destroy domain: %d - %s\n",
                     errno, strerror(errno));
            domid = DOMID_INVALID;
        }
    }
    log_check("All tests completed, nr_failures=%d\n", nr_failures);
    return !!nr_failures;
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
