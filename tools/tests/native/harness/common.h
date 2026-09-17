/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_COMMON_H
#define TOOLS_TESTS_NATIVE_HARNESS_COMMON_H

/* Assertion helpers shared by the tests. */
#include "testcase-asserts.h"
#include "config.h"

/* Heap allocator stubs */
#define __XMALLOC_H__
#define xmalloc(type)             calloc(1, sizeof(type))
#define xmalloc_array(type, nr)   calloc((nr), sizeof(type))
#define xzalloc_array(type, nr)   calloc((nr) ? (nr) : 1, sizeof(type))
#define xvzalloc_array(type, nr)  calloc((nr), sizeof(type))
#define xzalloc(type)             calloc(1, sizeof(type))
#define _xmalloc(size, align)     ((void)align, xmalloc(size))
#define _xzalloc(size, align)     ((void)align, xmalloc(size))
#define xfree(p)                  free(p)
#define xvfree(p)                 free(p)

/* Page directory index helpers */
#define mfn_to_pdx(mfn)   mfn_x(mfn)
#define pdx_to_mfn(pdx)   _mfn(pdx)
#define page_to_pdx(pg)   ((unsigned long)((pg) - frame_table))
#define pdx_to_page(pdx)  (frame_table + (pdx))

/* Atomic bit operations for the host, based on GCC built-in atomics */
#define arch_test_bit(nr, addr) \
    ((__atomic_load_n((const volatile bitop_uint_t *)(addr) + \
                      BITOP_WORD(nr), __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define test_and_set_bit arch__test_and_set_bit
#define arch__test_and_set_bit(nr, addr) \
    ((__atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                        BITOP_WORD(nr), BITOP_MASK(nr), \
                        __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define test_and_clear_bit arch__test_and_clear_bit
#define arch__test_and_clear_bit(nr, addr) \
    ((__atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                        BITOP_WORD(nr), ~BITOP_MASK(nr), \
                        __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define __set_bit set_bit
#define set_bit(nr, addr) \
    __atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                      BITOP_WORD(nr), BITOP_MASK(nr), __ATOMIC_SEQ_CST)
#define __clear_bit clear_bit
#define clear_bit(nr, addr) \
    __atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                       BITOP_WORD(nr), ~BITOP_MASK(nr), __ATOMIC_SEQ_CST)

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define __XEN_BUG_H__
#define __XEN_ERRNO_H__
#define __XEN_PDX_H__
#define _X86_BITOPS_H
#define _ARM_BITOPS_H
#define ASM__RISCV__BITOPS_H
#include <xen/cpumask.h>
#pragma GCC diagnostic pop

/* Blocking P2M headers needs fewer shims than including them */
#define _XEN_P2M_H
#define _XEN_ASM_X86_P2M_H
#define ASM__RISCV__P2M_H
#define map_mmio_regions(d, gfn, nr, mfn)   0
#define unmap_mmio_regions(d, gfn, nr, mfn) 0
struct p2m_domain { int dummy; }; /* riscv */

/* Common Xen types used by the test environment. */
typedef long long s_time_t;
typedef bool spinlock_t;
typedef spinlock_t rwlock_t;
typedef spinlock_t rspinlock_t;
typedef spinlock_t percpu_rwlock_t;

/*
 * If testcase_assert_verbose_assertions is enabled, the spinlock
 * functions print the spinlock being acquired or released along with
 * the file and line number of the assertion that triggered it.
 * This can be helpful for debugging test failures and understanding
 * the sequence of events leading up to the failure.
 */
#define spin_lock(l)              ((void)(l))
#define spin_unlock(l)            ((void)(l))
#define spin_lock_cb(l, cb, data) spin_lock(l)
#define spin_lock_kick()          ((void)0)
#define nrspin_lock(l)            spin_lock(l)
#define nrspin_unlock(l)          spin_unlock(l)
#define rspin_lock(l)             spin_lock(l)
#define rspin_unlock(l)           spin_unlock(l)
#define read_lock(rwl)            spin_lock(rwl)
#define read_unlock(rwl)          spin_unlock(rwl)
#define write_lock(rwl)           spin_lock(rwl)
#define write_unlock(rwl)         spin_unlock(rwl)
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
#define DEFINE_SPINLOCK(l)        spinlock_t l

/* The real implementation is hidden with the other Xen-only allocator APIs. */
#define cmpxchgptr(ptr, old, new) ({             \
    __typeof__(*(ptr)) cmpxchg_old_ = *(ptr);    \
    if ( cmpxchg_old_ == (old) )                \
        *(ptr) = (new);                          \
    cmpxchg_old_;                               \
})

/* xvmalloc_array supports both 2-arg (type, n) used by page_alloc.c and
 * 3-arg (type, rows, cols) used by domctl.c via a variadic helper.
 * The helper appends a sentinel '1' so the 2-arg form multiplies by 1. */
#define xvmalloc_array(type, ...)    _xvmalloc_impl(type, __VA_ARGS__, 1)
#define _xvmalloc_impl(t, a, b, ...) calloc((size_t)(a) * (size_t)(b), \
                                            sizeof(t))

/*
 * Use the real guest_access functions and provide stubs for
 * xen/asm-x86/guest_access.h's __raw_copy_{to,from}_guest() functions
 * which are used by the high-level copy_{to,from}_guest*() functions.
 */
#define __X86_UACCESS_H__
#define __ASM_X86_GUEST_ACCESS_H__
unsigned long raw_copy_to_guest(void *to, const void *from, unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}
#define __raw_copy_to_guest raw_copy_to_guest
unsigned long raw_copy_to_guest_flush_dcache(void *to, const void *from,
                                             unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}
unsigned long raw_copy_from_guest(void *to, const void *from, unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}
#define __raw_copy_from_guest raw_copy_from_guest
unsigned long raw_clear_guest(void *to, unsigned int len)
{
    return 0;
}

/* nodemask support for the test environment. */
#define DECLARE_PER_CPU(type, name) static __used type shim_per_cpu__##name
cpumask_t cpu_online_map;
cpumask_t cpu_present_map;
unsigned int nr_cpu_ids = NR_CPUS;

/*
 * PDX compression not yet actively used for the synthetic heap of the tests.
 * Use a small identity PDX bridge because pulling in full xen/pdx.h would
 * bring compressed-PDX runtime tables that the lib does not initialise yet.
 *
 * That is a deliberate shim boundary to allow testing without needing to
 * support PDX runtime in the test harness.  The test cases can use the
 * mfn_to_pdx and pdx_to_mfn helpers to work with PDX values, and the test
 * harness will track the valid range of PDX values based on the frame_table
 * size and the MFNs of the test pages that are initialised during the tests,
 * but the PDX values are not actually compressed in the test harness.
 */
#define __mfn_valid(mfn) true

/* tlbflush.h */
#define per_cpu(a, b) (0)
bool tlb_clk_enabled;
u32 tlbflush_clock;
#ifdef __x86_64__
#define cpu_has_cx16 true
#define cpu_relax() ((void)0)
#else
void flush_page_to_ram(unsigned long mfn, bool sync_icache) {}
#endif

/*
 * Some functions need to use types defined in specific headers,
 * so we include them and define header guards to prevent unwanted
 * definitions from those headers that conflict with the test harness
 * or bring in Xen-internal structures that are already provided by
 * the natural C compiler defines, libc defines and stubs in this shim.
 */
#define __ASM_I386_CPUFEATURE_H
#define __ARM_CURRENT_H__
#define __X86_CURRENT_H__
#define __XEN_IOCAP_H__
#define __XEN_PAGING_H__
#define __XEN_RCUPDATE_H
#define __XSM_H__
#define __LIB_H__ /* C runtime library, only for the hypervisor */
#define XEN_SELF_TESTS_H
#define XEN_SOFTIRQ_H
#define XEN__XVMALLOC_H
#define _LINUX_INIT_H
#define __RWLOCK_H__
#define _TIMER_H_
#define __XEN_PERCPU_H__
#define __XEN_TASKLET_H__
#define __SPINLOCK_H__
#define __VM_EVENT_H__
#define __XEN_EVENT_H__
#define __XEN_IRQ_H__
#define __ASM_DOMAIN_H__
#endif
