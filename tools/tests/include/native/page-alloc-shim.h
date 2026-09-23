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
#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
/* Provide struct page_info and related Xen definitions */
#define TEST_WRAP_XEN_INCLUDE_XEN_MM_H
#include "mm-wrapper.h"

enum system_state system_state = SYS_STATE_active;
struct timer {};
#ifndef __riscv
struct arch_domain {};
struct arch_vcpu {};
struct arch_vcpu_io {};
#endif
struct lock_profile_qhead {};
struct tasklet {};
#define parse_bool(s, e) (-1)
#define this_cpu(x)      (shim_per_cpu__##x)

#define __rcu
#define rcu_assign_pointer(p, v) ((p) = (v))
struct rcu_head {};
struct _rcu_read_lock {};
typedef struct _rcu_read_lock rcu_read_lock_t;
#define rcu_dereference(p) (p)
#define rcu_read_lock(l)   ((void)(l))
#define rcu_read_unlock(l) ((void)(l))

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
static struct vcpu __used *current;
#include <xen/sched.h>
static struct vcpu test_current_vcpu;
static struct vcpu *current = &test_current_vcpu;
#pragma GCC diagnostic pop

/* Dummy domains for allocations and page ownership in the test context */
static struct domain test_dummy_domain1;
static struct domain test_dummy_domain2;
static struct domain __used *dom1 = &test_dummy_domain1;
static struct domain __used *dom2 = &test_dummy_domain2;
#undef put_domain
#define put_domain(d)                 ((void)0)
#define rcu_lock_domain_by_any_id(id) (&test_dummy_domain1)
#define dom_io                        (&test_dummy_domain1)
#define dom_xen                       (&test_dummy_domain2)
nodemask_t node_online_map;

#ifdef CONFIG_NUMA
#define __node_distance(a, b) 0
#define arch_get_ram_range(i, start, end) ((void)i, *start = 0, *end = 0, 0)

#endif /* CONFIG_NUMA */

s_time_t get_s_time(void)    { return 0; }
#define smp_processor_id()   0U
#define cpumask_clear(mask)  ((void)0)
#define cpumask_and(d, a, b) ((void)0)
#define cpumask_or(d, a, b)  ((void)0)
#define cpumask_first(mask)  0U

#define page_get_owner_and_reference(pg)  page_get_owner(pg)
#define page_is_offlinable(mfn)           true
#define softirq_pending(cpu)              false
#define process_pending_softirqs()        ((void)0)
#define on_selected_cpus(msk, f, data, w) ((void)0)

/* Testing hypercall preemption is not supported yet. */
#undef hypercall_preempt_check
#define hypercall_preempt_check() 0
#undef arch_free_heap_page
#define arch_free_heap_page(d, p) ((void)0)
#define ASSERT_ALLOC_CONTEXT()    ((void)0)
#define send_global_virq(virq)    ((void)0)
#define __domain_crash(d)         ((void)0)

#include <asm/page.h>
#define get_page(p,d)   false
#define put_page(p)     ((void)0)
#ifndef clear_page
#define clear_page(ptr) ((void)0)
#endif

#ifdef __x86_64__
#define clear_page_hot(ptr)     ((void)0)
#define clear_page_cold(ptr)    ((void)0)

#define scrub_page_cold(ptr)    ((void)0)
#define set_gpfn_from_mfn(m, g) ((void)0)
#define arch_get_dma_bitsize() 32U
#endif

unsigned int get_max_nr_llc_colors(void) { return 1U; }
unsigned int page_to_llc_color(const struct page_info *pg) { return 0U; }
unsigned long simple_strtoul(const char *cp, const char **endp,
                             unsigned int base)
{
    return strtoul(cp, (char **)endp, base);
}
#endif
#endif
