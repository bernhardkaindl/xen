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

/*
 * Guard against language servers and linters picking up this header.
 *
 * This shim is intended to be used in test programs for testing
 * the code of xen/common/domctl.c in a host-side test environment,
 * and test programs need to define TEST_WRAP_XEN_COMMON_MEMORY_C
 * to enable the definitions in this header.
 */
#ifndef TEST_WRAP_XEN_COMMON_MEMORY_C
#warning "Include this header only in integration tests using memory.c"
#else

/*
 * Inside the intended test context, provide stub definitions.
 *
 * Some functions need to use types defined in specific headers,
 * so we include them and define header guards to prevent unwanted
 * definitions from those headers that conflict with the test harness
 * or bring in Xen-internal structures that are already provided by
 * the natural C compiler defines, libc defines and stubs in this shim.
 */

/*
 * Block headers whose content conflicts with the test harness or that pull in
 * types and functions not available in the host environment.  Stubs for the
 * symbols that memory.c actually uses from those headers are provided below.
 */
#define __XEN_IOREQ_H__         /* ioreq types unavailable; code under CONFIG_IOREQ_SERVER */
#define __XEN_GRANT_TABLE_H__   /* gnttab_* stubs provided below */
#define __XEN_LLC_COLORING_H__  /* llc_coloring_enabled stub provided below */
#define _XEN_MEM_ACCESS_H       /* mem_access_memop stub provided below */
#define __ASM_HARDIRQ_H         /* in_irq() stub provided below */
#define ARM_GENERIC_HARDIRQ_H   /* in_irq() stub provided below */
#define __ASM_GENERIC_HARDIRQ_H /* in_irq() stub provided below */

/*
 * Stubs for symbols from the blocked headers that memory.c actually uses.
 */

/*
 * XSM hooks used by memory.c beyond those already stubbed in domctl-shim.h.
 * The test environment permits all operations.
 */
#ifndef XSM_TARGET
#define XSM_TARGET      0
#endif
#ifndef XSM_DM_PRIV
#define XSM_DM_PRIV     0
#endif
#define xsm_memory_exchange(xsm, d)              0
#define xsm_add_to_physmap(xsm, d1, d2)          0
#define xsm_domain_resource_map(xsm, d)          0
#define xsm_memory_adjust_reservation(xsm, a, b) 0
#define xsm_memory_stat_reservation(xsm, a, b)   0
#define xsm_remove_from_physmap(xsm, a, b)       0
#define xsm_get_vnumainfo(xsm, d)                0

/*
 * MFN equality and invalid sentinel: mm-frame.h is blocked by
 * __XEN_FRAME_NUM_H__, so provide these here.
 */
#define mfn_eq(x, y) (mfn_x(x) == mfn_x(y))

/*
 * Guest-handle range check: asm/guest_access.h is blocked; in the test
 * context the handle always covers the requested range.
 */
#ifndef guest_handle_subrange_okay
#define guest_handle_subrange_okay(hnd, first, last) 1
#endif

/*
 * Static memory: not used in the test environment.
 * acquire_reserved_page is declared in xen/mm.h (blocked) and used only
 * when is_domain_using_staticmem() is true — which is never in the tests.
 */
#define acquire_reserved_page(d, flags) INVALID_MFN

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
#define get_page(pg, d)         true
#define put_page_alloc_ref(pg)  ((void)(pg))
#define get_page_type(pg, type) false
#define put_page_and_type(pg)   ((void)(pg))

/* Arch page operations used in clear_domain_page / copy_domain_page */
#ifndef clear_page
#define clear_page(ptr)         memset((ptr), 0, PAGE_SIZE)
#endif
#define copy_page_cold(dst, src) memcpy((dst), (src), PAGE_SIZE)
#define copy_page_hot(dst, src)  copy_page_cold(dst, src)

/*
 * Memory extent hypercall shift: defined in xen/hypercall.h (blocked).
 * do_memory_op() right-shifts cmd by this amount to extract the start extent.
 */
#define MEMOP_EXTENT_SHIFT      6
#define MEMOP_CMD_MASK          ((1 << MEMOP_EXTENT_SHIFT) - 1)

/*
 * GFN equality and invalid sentinel: mm-frame.h is blocked by
 * __XEN_FRAME_NUM_H__; gfn_t may be a type-safe wrapper in debug builds.
 */
#define gfn_eq(x, y)           (gfn_x(x) == gfn_x(y))
#define invalidate_icache()    ((void)0)

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
        ((void)(t), (void)(q), (struct page_info *)NULL)

/* grant_table.h: resource frame query/acquire stubs */
#define gnttab_resource_max_frames(d, id)       0
#define gnttab_acquire_resource(d, id, f, n, l) (-EOPNOTSUPP)

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
#define arch_memory_op(cmd, arg)   (-EOPNOTSUPP)

/* Stealing is initially not supported in the test environment */
#define steal_page(d, page, flags) (-EOPNOTSUPP)

/* mem_access.h: memory access operations are not tested here */
#define mem_access_memop(cmd, arg) (-EOPNOTSUPP)
#define arch_acquire_resource_check(d) false

#ifndef in_irq
#define in_irq()             false /* Tests aren't in interrupt context */
#endif
#define is_control_domain(d) false /* All test domains are unprivileged */

#endif
#endif
