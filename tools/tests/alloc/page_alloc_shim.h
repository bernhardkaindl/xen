/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shim definitions for Xen types and functions used by page_alloc.c.
 *
 * This shim is intended to be included directly in the test program
 * or header-only library for testing a specific scenario, included
 * by the test program, for unit testing functions in common/page_alloc.c.
 *
 * It provides stubs (minimal definitions for Xen types and functions)
 * used by page_alloc.c, sufficient to support the test scenarios in
 * tools/tests/alloc.
 *
 * It is not intended to be complete or accurate for general use in
 * other test contexts or as a general-purpose shim for page_alloc.c.
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
#define __TYPES_H__ /* Prevent collisions with the compiler-provided types */
#define __VM_EVENT_H__
#define __X86_PAGE_H__
#define __XEN_CONST_H__
#define __XEN_CPUMASK_H
#define __XEN_EVENT_H__
#define __XEN_FRAME_NUM_H__
#define __XEN_IRQ_H__
#define __XEN_KCONFIG_H
#define __XEN_MM_H__
#define __XEN_PDX_H__

/* Define common mocks and stubs needed for the test context */
#include "mock-page_list.h"
#include "harness.h"

/* Include common Xen definitions for the test context */
#include <xen/config.h>
#include <xen/keyhandler.h>
#include <xen/page-size.h>

/* Include xen/numa.h with stubs and unused parameter warnings disabled */
#define cpumask_clear_cpu(cpu, mask) ((void)(cpu), (void)(mask))
#define mfn_to_pdx(mfn)              ((void)mfn, 0)
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

struct domain {
    spinlock_t     page_alloc_lock;
    nodemask_t     node_affinity;
    nodeid_t       last_alloc_node;
    domid_t        domain_id;
    unsigned int   tot_pages;
    unsigned int   max_pages;
    unsigned int   extra_pages;
    unsigned int   outstanding_pages;
    unsigned int   xenheap_pages;
    bool           is_dying;
    struct domain *next_in_list;
};
extern struct domain *domain_list;

struct vcpu {
    struct domain *domain;
};

/*
 * Dummy spinlock definitions for the test context
 */
static spinlock_t *heap_lock_ptr;

/* Helper function to track spinlock actions for traceability and validation */
static void print_spinlock(const char *action, spinlock_t *lock,
                           const char *file, int line, const char *func)
{
    const char *relpath = file;

    if ( !testcase_assert_verbose_assertions )
        return;

    while ( (file = strstr(relpath, "../")) )
        relpath += 3;

    /* Print the path first:*/
    fprintf(stdout, "%s:%d: %s(): ", relpath, line, func);

    if ( lock == heap_lock_ptr )
        fprintf(stdout, "heap_lock %s\n", action);
    else if ( domain_list && lock == &domain_list->page_alloc_lock )
        fprintf(stdout, "domain_list->page_alloc_lock %s\n", action);
    else
        fprintf(stdout, "unknown lock %p %s\n", (void *)lock, action);
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

/* Architecture-specific page state defines */
#define PG_shift(idx)             (BITS_PER_LONG - (idx))
#define PG_mask(x, idx)           (x##UL << PG_shift(idx))
#define PGT_count_width           PG_shift(2)
#define PGT_count_mask            ((1UL << PGT_count_width) - 1)
#define PGC_allocated             PG_mask(1, 1)
#define PGC_xen_heap              PG_mask(1, 2)
#define _PGC_need_scrub           PG_shift(4)
#define PGC_need_scrub            PG_mask(1, 4)
#define _PGC_broken               PG_shift(7)
#define PGC_broken                PG_mask(1, 7)
#define PGC_state                 PG_mask(3, 9)
#define PGC_state_inuse           PG_mask(0, 9)
#define PGC_state_offlining       PG_mask(1, 9)
#define PGC_state_offlined        PG_mask(2, 9)
#define PGC_state_free            PG_mask(3, 9)
#define page_state_is(pg, st)     (((pg)->count_info & PGC_state) == PGC_state_##st)
#define PGC_count_width           PG_shift(9)
#define PGC_count_mask            ((1UL << PGC_count_width) - 1)
#define _PGC_extra                PG_shift(10)
#define PGC_extra                 PG_mask(1, 10)

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
#define MEMF_node(n)              ((((n) + 1) & MEMF_node_mask) << _MEMF_node)
#define MEMF_get_node(f)          ((((f) >> _MEMF_node) - 1) & MEMF_node_mask)
#define _MEMF_bits                24
#define MEMF_bits(n)              ((n) << _MEMF_bits)

#define string_param(name, var)
#define custom_param(name, fn)
#define size_param(name, var)
#define boolean_param(name, func)
#define integer_param(name, var)
#define ACCESS_ONCE(x) (x)
#define likely(x)      (!!(x))
#define unlikely(x)    (!!(x))
#define cmpxchg(ptr, oldv, newv) \
    ({                           \
        *(ptr) = (newv);         \
        (oldv);                  \
    })

#define for_each_domain(_d) \
    for ( (_d) = domain_list; (_d) != NULL; (_d) = (_d)->next_in_list )
#define for_each_online_node(i) for ( (i) = 0; (i) < MAX_NUMNODES; ++(i) )
#define for_each_cpu(i, mask)   for ( (i) = 0; (i) < 1; ++(i) )
#define is_xen_heap_page(pg)    false
#define PFN_ORDER(pg)           ((pg)->v.free.order)
#define page_to_mfn(pg)         ((pg)->mfn)
#define page_to_virt(pg)        ((void *)(pg))
#define virt_to_page(v)         ((struct page_info *)(v))
#define mfn_to_page(mfn)        (&test_dummy_page)
#define maddr_to_page(pa)       (&test_dummy_page)
#define mfn_to_virt(mfn)        ((void *)&test_dummy_storage)
#define __mfn_to_virt(mfn)      mfn_to_virt(mfn)
#define mfn_valid(mfn)          (mfn >= first_valid_mfn && mfn < max_page)
#define _mfn(x)                 ((mfn_t)(x))
#define mfn_x(x)                ((unsigned long)(x))
#define mfn_add(mfn, nr)        ((mfn) + (nr))
#define mfn_min(a, b)           ((a) < (b) ? (a) : (b))

static nodemask_t        node_online_map = ~0UL;
static cpumask_t         cpu_online_map  = ~0UL;
static struct page_info  test_dummy_page;
static unsigned char     test_dummy_storage[PAGE_SIZE];
static struct domain     test_dummy_domain1;
static struct domain     test_dummy_domain2;
static struct domain    *dom_cow;
static struct vcpu       test_current_vcpu;
static struct vcpu      *current     = &test_current_vcpu;
static struct page_info *frame_table = &test_dummy_page;

#ifdef CONFIG_NUMA
nodeid_t                   cpu_to_node[NR_CPUS];
cpumask_t                  node_to_cpumask[MAX_NUMNODES];
struct node_data           node_data[MAX_NUMNODES];
unsigned int               memnode_shift;
static typeof(*memnodemap) _memnodemap[64];
nodeid_t                  *memnodemap     = _memnodemap;
unsigned long              memnodemapsize = sizeof(_memnodemap);
#define __node_distance(a, b) 0
#endif /* CONFIG_NUMA */

/* STATIC_IF(x) magic */
#define ___config_enabled(__ignored, val, ...) val

#define STATIC_IF(option)        static_if(option)
#define static_if(value)         _static_if(__ARG_PLACEHOLDER_##value)
#define _static_if(arg1_or_junk) ___config_enabled(arg1_or_junk static, )
#define __ARG_PLACEHOLDER_1      0,
#define config_enabled(cfg)      _config_enabled(cfg)
#define _config_enabled(value)   __config_enabled(__ARG_PLACEHOLDER_##value)

#define __config_enabled(arg1_or_junk)    ___config_enabled(arg1_or_junk 1, 0)

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
#define num_online_nodes()                MAX_NUMNODES
#define cpu_online(cpu)                   ((cpu) == 0)
#define smp_processor_id()                0U
#define smp_wmb()                         ((void)0)
#define node_online(node)                 ((node) < MAX_NUMNODES)
#define nodes_intersects(a, b)            true
#define nodes_and(dst, a, b)              ((dst) = (a) & (b))
#define nodes_andnot(dst, a, b)           ((dst) = (a) & ~(b))
#define nodes_clear(dst)                  ((dst) = 0)
#define nodemask_test(node, mask)         true
#define node_set(node, mask)              ((void)(mask))
#define node_clear(node, mask)            ((void)(mask))
#define node_test_and_set(node, mask)     false
#define first_node(mask)                  0U
#define next_node(node, mask)             MAX_NUMNODES
#define cycle_node(node, mask)            0U
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
    ASSERT(d->extra_pages <= d->tot_pages);
    return d->tot_pages - d->extra_pages;
}

/* LLC (Last Level Cache) coloring support stubs */
#define llc_coloring_enabled                false
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
#endif /* _TEST_ALLOC_PAGE_ALLOC_SHIM_ */
