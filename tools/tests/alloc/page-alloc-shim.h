/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Page allocator shim for xen/common/page_alloc.c.
 *
 * This shim provides the minimal Xen definitions that page_alloc.c needs
 * in the unit-test environment.  It models a minimal subset of the Xen
 * environment, such as domain and NUMA node management, that page_alloc.c
 * interacts with, so that the allocator logic so the allocator behaves
 * as it does in the hypervisor while giving direct visibility into the
 * allocator state for verification in the tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef _TEST_ALLOC_PAGE_ALLOC_SHIM_
#define _TEST_ALLOC_PAGE_ALLOC_SHIM_

/*.
 * Guard against language servers and linters picking up this header in
 * the wrong context.
 *
 * This header is only intended to be used in the test program for unit
 * testing functions in xen/common/page_alloc.c, and test programs define
 * TEST_USES_PAGE_ALLOC_SHIM to enable the definitions in this header.
 */
#ifndef TEST_USES_PAGE_ALLOC_SHIM
#warning "This header is only for use in page_alloc.c unit tests."
#else
/*
 * Inside the intended test context, provide mocks and stub definitions.
 */

/* Configure the included headers for the test context */
#ifndef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS 64
#endif

#if defined(CONFIG_NUMA) && !defined(CONFIG_NR_NUMA_NODES)
#define CONFIG_NR_NUMA_NODES 64
#endif

#define CONFIG_SCRUB_DEBUG

/* Provide struct page_info and related Xen definitions */
#include "mock-page-list.h"

/* Include the common check_asserts library for test assertions */
#include "check-asserts.h"

/*
 * We add the Xen headers to the include path so page_alloc.c can
 * resolve its #include directives without having to replicate all
 * headers as actual files in the test tree:
 *
 * We define the header guards of those files to prevent unwanted
 * definitions from those headers that conflict with the test harness.
 */
#define XEN_SOFTIRQ_H
#define XEN__XVMALLOC_H
#define _LINUX_INIT_H
#define _XEN_PARAM_H
#define __LIB_H__ /* C runtime library, only for the hypervisor */
#define __LINUX_NODEMASK_H
#define __FLUSHTLB_H__
#define __SCHED_H__
#define __SPINLOCK_H__
#define __TYPES_H__ /* Conflicts with the compiler-provided types */
#define __VM_EVENT_H__
#define __X86_PAGE_H__
#define __XEN_CPUMASK_H
#define __XEN_EVENT_H__
#define __XEN_FRAME_NUM_H__
#define __XEN_IRQ_H__
#define __XEN_MM_H__
#define __XEN_PDX_H__

#include <xen/keyhandler.h>
#include <xen/page-size.h>
#include <public/xen.h>

/* Include xen/numa.h with stubs and unused parameter warnings disabled */
#define cpumask_clear_cpu(cpu, mask) ((void)(cpu), (void)(mask))
#define mfn_to_pdx(mfn)              ((unsigned long)(mfn))
#pragma GCC diagnostic push
#ifndef CONFIG_NUMA
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <xen/numa.h>
#pragma GCC diagnostic pop

/* Flexible definition to support 32- and 64-bit architectures */
#undef PADDR_BITS
#define PADDR_BITS              (BITS_PER_LONG - PAGE_SHIFT)
#define pfn_to_paddr(pfn)       ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)        ((unsigned long)((pa) >> PAGE_SHIFT))
#define INVALID_MFN_INITIALIZER (~0UL)

typedef unsigned long nodemask_t;

/* struct domain for testing domain-specific page allocation and claims */
struct domain {
    spinlock_t     page_alloc_lock;
    nodemask_t     node_affinity;
    nodeid_t       last_alloc_node;
    domid_t        domain_id;
    unsigned int   tot_pages;
    unsigned int   max_pages;
    unsigned int   extra_pages;
    unsigned int   global_claims;
    unsigned int   node_claims;
    unsigned int   claims[MAX_NUMNODES];
    unsigned int   xenheap_pages;
    bool           is_dying;
    struct domain *next_in_list;
};
extern struct domain *domain_list;

struct vcpu {
    struct domain *domain;
};

/*
 * Provide two domains for the test context, so that test helpers can call
 * allocator functions that require domain context and verify behavior that
 * depends on domain state, such as claims accounting and page allocation
 * for specific domains.
 */
static struct domain test_dummy_domain1;
static struct domain test_dummy_domain2;
static struct domain __used *dom1 = &test_dummy_domain1;
static struct domain __used *dom2 = &test_dummy_domain2;

/* To provide a current vcpu/domain pair for code paths that inspect it. */
static unsigned char test_dummy_storage[PAGE_SIZE];
static struct vcpu test_current_vcpu;
static struct vcpu *current = &test_current_vcpu;
static cpumask_t cpu_online_map = ~0UL;

#define for_each_domain(_d) \
        for ( (_d) = domain_list; (_d) != NULL; (_d) = (_d)->next_in_list )
#define for_each_online_node(i) for ( (i) = 0; (i) < MAX_NUMNODES; ++(i) )
#define for_each_cpu(i, mask)   for ( (i) = 0; (i) < 1; ++(i) )

/* dom_cow is a domain pointer used by the memory sharing code */
#ifdef CONFIG_MEM_SHARING
static struct domain *dom_cow;
#else
#define dom_cow NULL
#endif

/*
 * Logging spinlock for the test context
 */
static spinlock_t *heap_lock_ptr;

/* Helper function to track spinlock actions for additional context */
static void print_spinlock(const char *action, spinlock_t *lock,
                           const char *file, int line, const char *func)
{
    const char *relpath = file;

    if ( !testcase_assert_verbose_assertions )
        return;

    while ( (file = strstr(relpath, "../")) )
        relpath += 3;

    for ( int i = 0; i < testcase_assert_verbose_indent_level; i++ )
        printf("  ");

    /* Print the path first:*/
    if ( testcase_assert_current_func == NULL ||
         strcmp(testcase_assert_current_func, func) != 0 )
        printf("%s:%d: %s(): ", relpath, line, func);
    else
        printf("%s:%d: ", relpath, line);

    if ( lock == heap_lock_ptr )
        printf("heap_lock %s\n", action);
    else if ( domain_list && lock == &test_dummy_domain1.page_alloc_lock )
        printf("dom1->page_alloc_lock %s\n", action);
    else if ( domain_list && lock == &test_dummy_domain2.page_alloc_lock )
        printf("dom2->page_alloc_lock %s\n", action);
    else
        printf("unknown lock %p %s\n", (void *)lock, action);
}

/*
 * If testcase_assert_verbose_assertions is enabled, the spinlock
 * functions print the spinlock being acquired or released along with
 * the file and line number of the assertion that triggered it.
 * This can be helpful for debugging test failures and understanding
 * the sequence of events leading up to the failure.
 */
#define spin_lock(l) \
        (print_spinlock("acquired", l, __FILE__, __LINE__, __func__), (void)(l))
#define spin_unlock(l) \
        (print_spinlock("released", l, __FILE__, __LINE__, __func__), (void)(l))
#define spin_lock_cb(l, cb, data) spin_lock(l)
#define spin_lock_kick()          ((void)0)
#define nrspin_lock(l)            spin_lock(l)
#define nrspin_unlock(l)          spin_unlock(l)
#define rspin_lock(l)             spin_lock(l)
#define rspin_unlock(l)           spin_unlock(l)
#define DEFINE_SPINLOCK(l)        spinlock_t l
/*
 * For the test context, we assume all locks are always held to avoid having
 * to manage lock state in the test helpers.  This allows the test helpers
 * to call allocator functions that require locks to be held without needing
 * to acquire those locks, which simplifies the test code and focuses on
 * exercising the allocator logic under test.
 *
 * Invariants that would normally be protected by locks must still be upheld
 * by the test helpers, but the test helpers can assume they have exclusive
 * access to the allocator state and do not need to worry about concurrency.
 */
#define spin_is_locked(l)         true
#define rspin_is_locked(l)        true

/* memflags: */
#define _MEMF_no_refcount         0
#define MEMF_no_refcount          (1U << _MEMF_no_refcount)
#define _MEMF_populate_on_demand  1
#define MEMF_populate_on_demand   (1U << _MEMF_populate_on_demand)
#define _MEMF_keep_scrub          2
#define MEMF_keep_scrub           (1U << _MEMF_keep_scrub)
#define _MEMF_no_dma              3
#define MEMF_no_dma               (1U << _MEMF_no_dma)
#define _MEMF_exact_node          4
#define MEMF_exact_node           (1U << _MEMF_exact_node)
#define _MEMF_no_owner            5
#define MEMF_no_owner             (1U << _MEMF_no_owner)
#define _MEMF_no_tlbflush         6
#define MEMF_no_tlbflush          (1U << _MEMF_no_tlbflush)
#define _MEMF_no_icache_flush     7
#define MEMF_no_icache_flush      (1U << _MEMF_no_icache_flush)
#define _MEMF_no_scrub            8
#define MEMF_no_scrub             (1U << _MEMF_no_scrub)
#define _MEMF_node                16
#define MEMF_node_mask            ((1U << (8 * sizeof(nodeid_t))) - 1)
#define MEMF_node(n)              ((((n) + 1)&MEMF_node_mask) << _MEMF_node)
#define MEMF_get_node(f)          ((((f) >> _MEMF_node) - 1)&MEMF_node_mask)
#define _MEMF_bits                24
#define MEMF_bits(n)              ((n) << _MEMF_bits)

#define string_param(name, var)
#define custom_param(name, fn)
#define size_param(name, var)
#define boolean_param(name, func)
#define integer_param(name, var)
#define ACCESS_ONCE(x) (x)
#define cmpxchg(ptr, oldv, newv) \
        ({                       \
             *(ptr) = (newv);    \
             (oldv);             \
         })

#define is_xen_heap_page(pg)          false
#define page_to_virt(pg)              ((void *)(pg))
#define virt_to_page(v)               ((struct page_info *)(v))
#define mfn_to_virt(mfn)              ((void *)&test_dummy_storage)
#define __mfn_to_virt(mfn)            mfn_to_virt(mfn)
#define _mfn(x)                       ((mfn_t)(x))
#define mfn_x(x)                      ((unsigned long)(x))
#define mfn_add(mfn, nr)              ((mfn) + (nr))
#define mfn_min(a, b)                 ((a) < (b) ? (a) : (b))

/*
 * NUMA stubs for unit testing NUMA-aware page allocator logic.
 *
 * nodemask_test() and node_set() implement real bit operations so that
 * domain_install_claim_set() can correctly detect duplicate node entries
 * in a claim set. mfn_to_pdx() is defined before xen/numa.h is included.
 */

static nodemask_t node_online_map = ~0UL;
#define num_online_nodes()            MAX_NUMNODES
#define node_online(node)             ((node) < MAX_NUMNODES)
#define nodes_intersects(a, b)        ((a) & (b))
#define nodes_and(dst, a, b)          ((dst) = (a) & (b))
#define nodes_andnot(dst, a, b)       ((dst) = (a) & ~(b))
#define nodes_clear(dst)              ((dst) = 0)
#define nodemask_test(node, mask)     ((*(mask) >> (node)) & 1UL)
#define node_set(node, mask)          ((mask) |= (1UL << (node)))
#define node_clear(node, mask)        ((void)(mask))
#define node_test_and_set(node, mask) false
#define first_node(mask)              0U
#define next_node(node, mask)         MAX_NUMNODES
#define cycle_node(node, mask)        0U

#ifdef CONFIG_NUMA
#define __node_distance(a, b) 0
nodeid_t cpu_to_node[NR_CPUS];
cpumask_t node_to_cpumask[MAX_NUMNODES];
struct node_data node_data[MAX_NUMNODES];
unsigned int memnode_shift;

static typeof(*memnodemap) _memnodemap[64];
nodeid_t *memnodemap = _memnodemap;
unsigned long memnodemapsize = sizeof(_memnodemap);
#endif /* CONFIG_NUMA */

/*
 * Stub definitions for Xen functions and macros used by page_alloc.c,
 * sufficient to support the test scenarios in tools/tests/alloc.
 *
 * These are not intended to be complete or accurate for general use
 * in other test contexts or as a general-purpose shim for page_alloc.c.
 */
#define rcu_lock_domain(id)               (&test_dummy_domain1)
#define rcu_lock_domain_by_any_id(id)     (&test_dummy_domain1)
#define NOW()                             0LL
#define SYS_STATE_active                  1
#define system_state                      0
#define cpu_online(cpu)                   ((cpu) == 0)
#define smp_processor_id()                0U
#define smp_wmb()                         ((void)0)
#define cpumask_empty(mask)               true
#define cpumask_clear(mask)               ((void)(mask))
#define cpumask_and(dst, a, b)            ((void)(dst), (void)(a), (void)(b))
#define cpumask_or(dst, a, b)             ((void)(dst), (void)(a), (void)(b))
#define cpumask_copy(dst, src)            ((void)(dst), (void)(src))
#define cpumask_first(mask)               0U
#define cpumask_intersects(a, b)          false
#define cpumask_weight(mask)              1
#define __cpumask_set_cpu(cpu, mask)      ((void)(cpu), (void)(mask))
#define page_get_owner(pg)                ((pg)->owner)
#define page_set_owner(pg, d)             ((pg)->owner = (d))
#define page_get_owner_and_reference(pg)  ((pg)->owner)
#define page_set_tlbflush_timestamp(pg)   ((pg)->tlbflush_timestamp = 0)
#define set_gpfn_from_mfn(mfn, gpfn)      ((void)0)
#define page_is_offlinable(mfn)           true
#define is_xen_fixed_mfn(mfn)             false
#define filtered_flush_tlb_mask(ts)       ((void)(ts))
#define accumulate_tlbflush(need, pg, ts) ((void)(need), (void)(pg), (void)(ts))
#define flush_page_to_ram(mfn, icache)    ((void)(mfn), (void)(icache))
#define scrub_page_hot(ptr)               clear_page_hot(ptr)
#define scrub_page_cold(ptr)              clear_page_cold(ptr)
#define send_global_virq(virq)            ((void)(virq))
#define softirq_pending(cpu)              false
#define process_pending_softirqs()        ((void)0)
#define on_selected_cpus(msk, f, data, w) ((void)0)
#define cpu_relax()                       ((void)0)
#define xmalloc(type)                     calloc(1, sizeof(type))
#define xmalloc_array(type, nr)           calloc((nr), sizeof(type))
#define xvzalloc_array(type, nr)          calloc((nr), sizeof(type))
#define xvmalloc_array(type, nr)          calloc((nr), sizeof(type))
#define get_order_from_pages(nr)          0U
#define get_order_from_bytes(bytes)       0U
#define arch_mfns_in_directmap(mfn, nr)   true
#define maddr_to_mfn(pa)                  ((mfn_t)paddr_to_pfn(pa))

#define ASSERT_ALLOC_CONTEXT()              ((void)0)
#define arch_free_heap_page(d, pg)          ((void)(d), (void)(pg))
#define get_knownalive_domain(d)            ((void)(d))
#define domain_clamp_alloc_bitsize(d, bits) (bits)
#define mem_paging_enabled(d)               false
#define put_domain(d)                       ((void)(d))
#define clear_page_hot(ptr)                 memset((ptr), 0, PAGE_SIZE)
#define clear_page_cold(ptr)                memset((ptr), 0, PAGE_SIZE)
#define unmap_domain_page(ptr)              ((void)(ptr))
#define put_page(pg)                        ((void)(pg))

void *alloc_xenheap_pages(unsigned int order, unsigned int memflags);
void  init_domheap_pages(paddr_t ps, paddr_t pe);
struct page_info *alloc_domheap_pages(struct domain *d, unsigned int order,
                                      unsigned int memflags);

/* Additional stubs for test support */

unsigned int arch_get_dma_bitsize(void)
{
    return 32U;
}

/* Return number of pages currently posessed by the domain */
static inline unsigned int domain_tot_pages(const struct domain *d)
{
    assert(d->extra_pages <= d->tot_pages);
    return d->tot_pages - d->extra_pages;
}

/* LLC (Last Level Cache) coloring support stubs */
#define llc_coloring_enabled false
unsigned int get_max_nr_llc_colors(void)
{
    return 1U;
}
unsigned int page_to_llc_color(const struct page_info *pg)
{
    (void)pg;
    return 0U;
}

#define parse_bool(s, e) (-1) /* Not parsed, use the default */

void __init register_keyhandler(unsigned char key, keyhandler_fn_t *fn,
                                const char *desc, bool diagnostic)
{
    (void)key;
    (void)fn;
    (void)desc;
    (void)diagnostic;
}

unsigned long simple_strtoul(const char *cp, const char **endp,
                             unsigned int base)
{
    return strtoul(cp, (char **)endp, base);
}

#endif /* TEST_USES_PAGE_ALLOC_SHIM */

#endif /* TOOLS_TESTS_ALLOC_PAGE_ALLOC_SHIM_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
