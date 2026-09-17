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

static struct vcpu __used *current;  /* zero-init; assigned via constructor below */
enum system_state system_state = SYS_STATE_active;

struct timer {};
#ifndef __riscv
struct arch_domain {};
struct arch_vcpu {};
struct arch_vcpu_io {};
#endif
struct lock_profile {};
struct lock_profile_qhead {};
struct tasklet {};
#define parse_bool(s, e) (-1)
#define perfc_incr(x) ((void)0)
#define this_cpu(x) (shim_per_cpu__##x)

/*
 * RCU annotation and helper stubs.  rcupdate.h is blocked; provide the
 * minimal definitions that radix-tree.h and sched.h need during compilation.
 */
#define __rcu                    /* RCU ownership annotation (empty) */
#define rcu_assign_pointer(p, v) ((p) = (v))
struct rcu_head {
    struct rcu_head *next;
    void             (*func)(struct rcu_head *);
};
struct _rcu_read_lock {};
typedef struct _rcu_read_lock rcu_read_lock_t;
#define rcu_dereference(p)    (p)
#define rcu_read_lock(lock)   ((void)(lock))
#define rcu_read_unlock(lock) ((void)(lock))

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#else
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#endif
#include <xen/sched.h>

/* Dummy domains for allocations and page ownership in the test context */
static struct domain test_dummy_domain1;
static struct domain test_dummy_domain2;
static struct domain __used *dom1 = &test_dummy_domain1;
static struct domain __used *dom2 = &test_dummy_domain2;
#undef put_domain
#define put_domain(d)                 ((void)(d))
#define rcu_lock_domain(id)           (&test_dummy_domain1)
#define rcu_lock_domain_by_any_id(id) (&test_dummy_domain1)
#define rcu_unlock_domain(d)          ((void)(d))
#define dom_io                        (&test_dummy_domain1)
#define dom_xen                       (&test_dummy_domain2)

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
#endif

nodemask_t node_online_map;

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

#define NOW()               0LL
#define smp_processor_id()  0U
/* smp_wmb and cpumask_weight defined before sched.h include; identical
 * redefinitions here are benign but kept for clarity. */
#define cpumask_clear(mask)      ((void)(mask))
#define cpumask_and(dst, a, b)   ((void)(dst), (void)(a), (void)(b))
#define cpumask_or(dst, a, b)    ((void)(dst), (void)(a), (void)(b))
#define cpumask_first(mask)      0U

/* cpumask_weight defined before sched.h; identical redefinition is benign */
#define page_get_owner_and_reference(pg)  page_get_owner(pg)
#define page_is_offlinable(mfn)           true
#define softirq_pending(cpu)              false
#define process_pending_softirqs()        ((void)0)
#define on_selected_cpus(msk, f, data, w) ((void)0)

/* Testing hypercall preemption is not supported yet. */
#undef hypercall_preempt_check
#define hypercall_preempt_check()  0

#undef arch_free_heap_page
#define arch_free_heap_page(d, pg) ((void)(d), (void)(pg))
#define ASSERT_ALLOC_CONTEXT()     ((void)0)

#define put_page(pg) ((void)(pg))
bool get_page(struct page_info *page, const struct domain *domain)
{
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

/* LLC (Last Level Cache) coloring support stubs */
unsigned int get_max_nr_llc_colors(void)
{
    return 1U;
}

unsigned int page_to_llc_color(const struct page_info *pg)
{
    return 0U;
}

void send_global_virq(uint32_t virq)
{
}

unsigned long simple_strtoul(const char *cp, const char **endp,
                             unsigned int base)
{
    return strtoul(cp, (char **)endp, base);
}

void __domain_crash(struct domain *d)
{
}

#endif
#endif
