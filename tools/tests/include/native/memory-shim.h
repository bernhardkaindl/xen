/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include xen/common/domctl.c in host-side tests.
 *
 * This shim provides the minimal Xen definitions that domctl.c
 * needs to run in a host-side test environment.  It replaces a
 * minimal subset of the Xen environment that xen/common/domctl.c
 * interacts with with stubs so it can run in the test environment,
 * allowing test scenarios to verify the behavior of domctl.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_MEMORY_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_MEMORY_SHIM_H

#ifdef TEST_WRAP_XEN_COMMON_MEMORY_C
#define _XEN_MEM_ACCESS_H       /* mem_access_memop stub provided below */
#define __ASM_HARDIRQ_H         /* in_irq() stub provided below */
#define __ASM_GENERIC_HARDIRQ_H /* in_irq() stub provided below */
#define xsm_memory_exchange(xsm, d)              0
#define xsm_add_to_physmap(xsm, d1, d2)          0
#define xsm_domain_resource_map(xsm, d)          0
#define xsm_memory_adjust_reservation(xsm, a, b) 0
#define xsm_memory_stat_reservation(xsm, a, b)   0
#define xsm_remove_from_physmap(xsm, a, b)       0
#define xsm_get_vnumainfo(xsm, d)                0

/*
 * Guest-handle range check: asm/guest_access.h is blocked; in the test
 * context the handle always covers the requested range.
 */
#ifndef guest_handle_subrange_okay
#define guest_handle_subrange_okay(hnd, first, last) 1
#endif

/*
 * P2M populate-on-demand helpers used in decrease_reservation.
 * These paths are not exercised by claim tests.
 */
#define p2m_pod_decrease_reservation(d, gfn, order) 0UL

/*
 * Physmap and p2m operations: stubs for the non-x86 code paths in memory.c.
 * On x86 the real implementations live in asm/p2m.h (already blocked by
 * domctl-shim.h); the code paths below the #ifdefs still reference these.
 */
#define gfn_to_mfn(d, gfn)                                    _mfn(~0UL)
#define guest_physmap_add_page(d, gfn, mfn, order)            (-EOPNOTSUPP)
#define guest_physmap_remove_page(d, gfn, mfn, order)         (-EOPNOTSUPP)
#define guest_physmap_mark_populate_on_demand(d, gpfn, order) (-EOPNOTSUPP)

/* Page reference count helpers used in guest_remove_page and helpers */
#define get_page_type(pg, type) false
#define put_page_and_type(pg)   ((void)(pg))

/* Arch page operations used in clear_domain_page / copy_domain_page */
#ifndef clear_page
#define clear_page(ptr) memset((ptr), 0, PAGE_SIZE)
#endif
#define copy_page_cold(dst, src) memcpy((dst), (src), PAGE_SIZE)

/*
 * Memory extent hypercall shift: defined in xen/hypercall.h (blocked).
 * do_memory_op() right-shifts cmd by this amount to extract the start extent.
 */
#define MEMOP_EXTENT_SHIFT 6
#define MEMOP_CMD_MASK     ((1 << MEMOP_EXTENT_SHIFT) - 1)

/*
 * RCU domain locking for remote domains: sched.h declares
 * rcu_lock_remote_domain_by_id() as a non-static extern.  Provide a
 * matching non-static definition here.
 */
int rcu_lock_remote_domain_by_id(domid_t dom, struct domain **dp)
{
    struct domain *d;

    for_each_domain ( d )
    {
        if ( d->domain_id == dom )
        {
            *dp = d;
            return 0;
        }
    }
    return -ESRCH;
}

/*
 * p2m_type_t and related constants: architecture p2m.h is blocked by the
 * wrapper because its helpers need too much real arch_domain state for this
 * harness.  Provide the minimal subset used by memory.c.
 */
typedef unsigned int p2m_type_t;
typedef unsigned int p2m_query_t;
#define P2M_ALLOC   (1u << 0)
#define P2M_UNSHARE (1u << 1)
#define get_page_from_gfn(d, gfn, t, q) \
    ({ (void)(d); (void)(gfn); (void)(t); (void)(q); \
       (struct page_info *)NULL; })

/*
 * Maximum GPFN: architecture-specific; in the test environment there is no
 * real guest memory so return 0.
 */
#define domain_get_maximum_gpfn(d) 0UL

/*
 * Physmap stubs for xenmem_add_to_physmap_one() and set_foreign_p2m_entry().
 * These arch-level operations are not currently supported in the test env.
 */
#define set_foreign_p2m_entry(d, fd, gfn, mfn) (-EOPNOTSUPP)

/* arch_memory_op: architecture-specific memory operations not tested here. */
#define arch_memory_op(cmd, arg) (-EOPNOTSUPP)

/* mem_access.h: memory access operations are not tested here */
#define mem_access_memop(cmd, arg) (-EOPNOTSUPP)
#define arch_acquire_resource_check(d) false

#endif
#endif
