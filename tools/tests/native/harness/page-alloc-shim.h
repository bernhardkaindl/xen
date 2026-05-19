/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include xen/common/page_alloc.c in host-side tests.
 *
 * This shim provides the minimal Xen definitions that page_alloc.c
 * needs to run in a host-side test environment.  It replaces a
 * minimal subset of the Xen environment that xen/common/page_alloc.c
 * interacts with with stubs so it can run in the test environment,
 * allowing test scenarios to verify the behavior of page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_PAGE_ALLOC_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_PAGE_ALLOC_SHIM_H

/*
 * Guard against language servers and linters picking up this header.
 *
 * This shim is intended to be used in test programs for testing the
 * code of xen/common/page_alloc.c in a host-side test environment,
 * and test programs need to define TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
 * to enable the definitions in this header.
 */
#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#define CONFIG_SCRUB_DEBUG
#define TEST_WRAP_XEN_INCLUDE_XEN_MM_H

/* Provide struct page_info and related Xen definitions */
#include "common.h"
#include "mm-wrapper.h"

#include <xen/nospec.h>     /* xen/pci.h needs to include xen/nospec.h */
#include <xen/kernel.h>
#include <xen/typesafe.h>
#include <public/xen.h>

/* Include xen/numa.h with stubs and unused parameter warnings disabled */
#define cpumask_clear_cpu(cpu, mask) ((void)(cpu), (void)(mask))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <xen/numa.h>
#pragma GCC diagnostic pop

/* Flexible definition to support 32- and 64-bit architectures */
typedef unsigned long nodemask_t;

/*
 * Include xen/sched.h for struct domain and struct vcpu definitions.
 * The guards need to prevent inclusion of the conflicting headers
 * that would cause problems in the test context, but sched.h itself
 * is needed to provide the full definitions of struct domain and
 * struct vcpu that page_alloc.c interacts with.
 */

/*
 * Provide stubs for types from blocked headers that xen/sched.h's struct
 * domain and struct vcpu reference by value.  These are minimal empty
 * definitions sufficient to make the structs compile in the test context.
 */
typedef cpumask_t *cpumask_var_t;
struct timer {};
#ifndef __riscv
struct arch_domain {};
struct arch_vcpu {};
struct arch_vcpu_io {};
#endif
struct lock_profile {};
struct lock_profile_qhead {};
struct tasklet {};
struct _rcu_read_lock {};
typedef struct _rcu_read_lock rcu_read_lock_t;

/* rcu_dereference is used by for_each_domain in sched.h (rcupdate.h blocked) */
#define rcu_dereference(p) (p)

/* perfc_incr is used in SCHED_STAT_CRANK (perfc.h blocked) */
#define perfc_incr(x)      ((void)0)

/*
 * Macros and stubs needed by sched.h's inline function bodies during header
 * compilation.  These come from headers that are blocked above.  Some are
 * also redefined in the stubs section below.
 */

/*
 * RCU annotation and helper stubs.  rcupdate.h is blocked; provide the
 * minimal definitions that radix-tree.h and sched.h need during compilation.
 */
#define __rcu                           /* RCU ownership annotation (empty) */
#define rcu_assign_pointer(p, v)        ((p) = (v))
struct rcu_head {
    struct rcu_head *next;
    void             (*func)(struct rcu_head *);
};

static struct vcpu __used *current;  /* zero-init; assigned via constructor below */
enum system_state system_state = SYS_STATE_active;

/* Macros used by sched.h inline functions (blocked headers provide these) */
#define cpumask_weight(mask)  1U
#define rcu_read_lock(lock)   ((void)(lock))
#define rcu_read_unlock(lock) ((void)(lock))
/*
 * Minimal per-CPU stubs: sched.h uses DECLARE_PER_CPU and this_cpu() in
 * inline functions.  These come from the blocked percpu.h.  Map onto simple
 * file-scope static variables to satisfy compilation.
 */
#define this_cpu(x)                 (shim_per_cpu__##x)

/*
 * page_to_list is a macro in mock-page-list.h (included earlier) AND a
 * static inline in sched.h.  Undefine the macro before the include so that
 * sched.h can define the function without a conflicting macro expansion.
 * The mock macro is restored after sched.h is included.
 */
#undef page_to_list
#undef is_xen_heap_page
#undef is_xen_fixed_mfn
#undef is_xen_heap_mfn
#define is_xen_heap_page(pg)  false
#define is_xen_fixed_mfn(mfn) false
#define is_xen_heap_mfn(mfn)  false

/*
 * domain_crash has a 1-arg macro in hypervisor-macros.h and a variadic
 * macro in sched.h.  Undefine before the include to avoid -Wmacro-redefined;
 * sched.h's variadic version plus the __domain_crash() stub provided below
 * give us the correct behaviour in the test context.
 */
#undef domain_crash

/* ioreq.h provides ioreq_t used by struct vcpu_io inside sched.h */
#include <public/hvm/ioreq.h>

/*
 * Include the real xen/sched.h to get struct domain, struct vcpu, and
 * all related declarations.  The guards above prevent conflicting
 * subsidiary headers from being pulled in.  Suppress warnings about
 * unused parameters in the Xen header inline functions.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/*
 * Suppress builtin declaration mismatch warnings for Xen's ffsl and flsl
 * definitions, Xen's ffs macros are unsigned while gcc expects signed.
 */
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#else
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#endif
#include <xen/sched.h>
#pragma GCC diagnostic pop

/* Dummy domains for allocations and page ownership in the test context */
static struct domain test_dummy_domain1;
static struct domain test_dummy_domain2;
static struct domain __used *dom1 = &test_dummy_domain1;
static struct domain __used *dom2 = &test_dummy_domain2;

/*
 * Restore test-friendly overrides for sched.h macros that reference
 * real Xen runtime infrastructure (domain_destroy, etc.).
 */
#undef put_domain
#define put_domain(d)       ((void)(d))

/* RCU domain locking (additional stubs beyond rcu_lock_domain already there) */
#define rcu_unlock_domain(d) ((void)(d))
#define dom_io               (&test_dummy_domain1)
#define dom_xen              (&test_dummy_domain2)

/* To provide a current vcpu/domain pair for code paths that inspect it. */

static struct vcpu test_current_vcpu;
/* 'current' was forward-declared before sched.h.  Set the pointer at
 * program startup via a constructor so that test helpers see the right vcpu
 * from the very first call into the allocator. */
static void __attribute__((constructor)) _page_alloc_shim_init_current(void)
{
    current = &test_current_vcpu;
}
/* dom_cow is a domain pointer used by the memory sharing code */
#ifdef CONFIG_MEM_SHARING
static struct domain *dom_cow;
#else
#define dom_cow NULL
#endif

/* NUMA stubs for unit testing NUMA-aware page allocator logic. */
#define first_node(mask)                    \
        ({                                  \
             unsigned long __mask = (mask); \
             __builtin_ffs(__mask) - 1;     \
         })
#define next_node(node, mask)                                              \
        ({                                                                 \
             unsigned long __mask = (mask) & ~((1UL << ((node) + 1)) - 1); \
             __builtin_ffs(__mask) - 1;                                    \
         })
#define cycle_node(node, mask)                                             \
        ({                                                                 \
             unsigned long __mask = (mask) & ~((1UL << ((node) + 1)) - 1); \
             int __next = __builtin_ffs(__mask) - 1;                       \
             __next >= 0 ? __next : first_node(mask);                      \
         })
#define num_online_nodes()        MAX_NUMNODES
#define node_online(node)         ((node) < MAX_NUMNODES)
static nodemask_t node_online_map = ~0UL;

#ifdef CONFIG_NUMA
/* Replacements for common/numa.c */
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
 * sufficient to support the test scenarios.
 *
 * These are not intended to be complete or accurate for general use
 * in other test contexts or as a general-purpose shim for page_alloc.c.
 */
#define rcu_lock_domain(id)               (&test_dummy_domain1)
#define rcu_lock_domain_by_any_id(id)     (&test_dummy_domain1)
#define NOW()                             0LL
#define cpu_online(cpu)                   ((cpu) == 0)
#define smp_processor_id()                0U
/* smp_wmb and cpumask_weight defined before sched.h include; identical
 * redefinitions here are benign but kept for clarity. */
#define cpumask_empty(mask)               true
#define cpumask_clear(mask)               ((void)(mask))
#define cpumask_and(dst, a, b)            ((void)(dst), (void)(a), (void)(b))
#define cpumask_or(dst, a, b)             ((void)(dst), (void)(a), (void)(b))
#define cpumask_copy(dst, src)            ((void)(dst), (void)(src))
#define cpumask_first(mask)               0U
#define cpumask_intersects(a, b)          false

#define parse_bool(s, e) (-1) /* Use the default register_keyhandler value */

/* cpumask_weight defined before sched.h; identical redefinition is benign */
#define __cpumask_set_cpu(cpu, mask)      ((void)(cpu), (void)(mask))
#define page_get_owner_and_reference(pg)  page_get_owner(pg)
#define page_is_offlinable(mfn)           true
#define softirq_pending(cpu)              false
#define process_pending_softirqs()        ((void)0)
#define on_selected_cpus(msk, f, data, w) ((void)0)
#define get_order_from_pages(nr)          0U
#define get_order_from_bytes(bytes)       0U

/* Testing hypercall preemption is not supported yet. */
#undef hypercall_preempt_check
#define hypercall_preempt_check()  0

#undef arch_free_heap_page
#define arch_free_heap_page(d, pg) ((void)(d), (void)(pg))
#define unmap_domain_page(ptr)     ((void)(ptr))
#define ASSERT_ALLOC_CONTEXT()     ((void)0)
#define get_knownalive_domain(d)   ((void)(d))
#define mem_paging_enabled(d)      false

#define put_page(pg) ((void)(pg))
bool get_page(struct page_info *page, const struct domain *domain)
{
    (void)page;
    (void)domain;
    return false;
}

#ifdef __x86_64__
#define clear_page_hot(ptr)          memset((ptr), 0, PAGE_SIZE)
#define clear_page_cold(ptr)         memset((ptr), 0, PAGE_SIZE)
#define scrub_page_hot(ptr)          clear_page_hot(ptr)
#define scrub_page_cold(ptr)         clear_page_cold(ptr)
#define set_gpfn_from_mfn(mfn, gpfn) ((void)0)

unsigned int arch_get_dma_bitsize(void)
{
    return 32U;
}
#endif

#if defined(__arm__) || defined(__aarch64__)
#define test_and_clear_bit(nr, addr) generic__test_and_clear_bit(nr, addr)
#endif

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

void send_global_virq(uint32_t virq)
{
    (void)virq;
}

unsigned long simple_strtoul(const char *cp, const char **endp,
                             unsigned int base)
{
    return strtoul(cp, (char **)endp, base);
}

/*
 * __domain_crash is declared by sched.h (via domain_crash macro) but has no
 * definition for the test environment.  Provide a no-op stub so the test
 * binary links correctly if domain_crash is ever expanded.
 */
void __domain_crash(struct domain *d)
{
    (void)d;
}

#endif
#endif
