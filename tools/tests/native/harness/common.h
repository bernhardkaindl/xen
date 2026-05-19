/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_HARNESS_H
#define TOOLS_TESTS_ALLOC_HARNESS_H

/* Assertion helpers shared by the tests. */
#include "testcase-asserts.h"

/* Configure the Xen code for the test context */
#define CONFIG_DEBUG
#define COMPILE_OFFSETS
#ifdef __x86_64__
#define __ASM_I386_CPUFEATURE_H
#define __XEN_X86_CPUFEATURESET_H__
#define _X86_BITOPS_H
#define X86_FEATURE_XEN_SMAP 0
#endif
#define CONFIG_MMU
#ifdef __arm__
#define CONFIG_ARM_32
#define CONFIG_PADDR_BITS 40
#endif
#ifdef __aarch64__
#define CONFIG_ARM_64
#define CONFIG_PADDR_BITS 48
#endif
#ifdef __riscv
#define CONFIG_RISCV_64
#define CONFIG_QEMU_PLATFORM
#endif

#define CONFIG_NR_CPUS 2048
#if defined(CONFIG_NUMA) && !defined(CONFIG_NR_NUMA_NODES)
/* Xen supports at least 64 nodes. New HW can have a lot more memory nodes. */
#define CONFIG_NR_NUMA_NODES 254 /* 255(0xFF) is reserved for NUMA_NO_NODE. */
#endif
#ifdef CONFIG_NR_NUMA_NODES
#define MAX_NUMNODES CONFIG_NR_NUMA_NODES
#else
#define MAX_NUMNODES 1
#endif

/* Header guards we always need to block (configuration-independent)*/
#define __ARM_CURRENT_H__
#define __ASM_X86_X86_EMULATE_H__
#define __X86_CURRENT_H__
#define __XEN_IOCAP_H__
#define __XEN_PAGING_H__
#define __XEN_RCUPDATE_H
#define __XSM_H__

/* Blocking P2M headers needs fewer shims than including them */
#define _XEN_P2M_H
#define _XEN_ASM_X86_P2M_H
#define ASM__RISCV__P2M_H
#define map_mmio_regions(d, gfn, nr, mfn)   0
#define unmap_mmio_regions(d, gfn, nr, mfn) 0
struct p2m_domain { int dummy; }; /* riscv */

/* Common Xen types used by the test environment. */
/* Short integer types (xen/types.h blocked by __TYPES_H__) */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
typedef unsigned long cpumask_t;
typedef long long s_time_t;
typedef bool spinlock_t;
typedef spinlock_t rwlock_t;
typedef spinlock_t rspinlock_t;
typedef spinlock_t percpu_rwlock_t;

#define __XEN_BITMAP_H
#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(bits) (((bits) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#endif
#ifndef DECLARE_BITMAP
#define DECLARE_BITMAP(name, bits) unsigned long name[BITS_TO_LONGS(bits)]
#endif

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
#define read_trylock(rwl)         (spin_lock(rwl), true)
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
#define rw_is_locked(rwl)         true
#define rw_is_write_locked(rwl)   true
#define spin_is_locked(l)         true
#define rspin_is_locked(l)        true
#define DEFINE_SPINLOCK(l)        spinlock_t l

/* Heap allocator stubs */
#define __XMALLOC_H__
#define xmalloc(type)             calloc(1, sizeof(type))
#define xmalloc_array(type, nr)   calloc((nr), sizeof(type))
#define xvzalloc_array(type, nr)  calloc((nr), sizeof(type))
#define xzalloc(type)             calloc(1, sizeof(type))
#define xfree(p)                  free(p)
#define xvfree(p)                 free(p)

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
    memset(to, 0, len);
    return 0;
}

/* nodemask support for the test environment. */
#define nodes_intersects(a, b)    ((a) & (b))
#define nodes_and(dst, a, b)      ((dst) = (a) & (b))
#define nodes_andnot(dst, a, b)   ((dst) = (a) & ~(b))
#define nodes_clear(dst)          ((dst) = 0)
#define nodemask_test(node, mask) ((*(mask) >> (node)) & 1UL)
#define node_set(node, mask)      ((mask) |= (1UL << (node)))
#define node_clear(node, mask)    ((mask) &= ~(1UL << (node)))
#define nodemask_bits(maskp)      ((unsigned long *)(maskp))
#define node_test_and_set(node, mask)                   \
        ({                                              \
             bool was_set = nodemask_test(node, &mask); \
             node_set(node, mask);                      \
             was_set;                                   \
         })
#define for_each_online_node(i)   for ( (i) = 0; (i) < MAX_NUMNODES; ++(i) )
#define for_each_cpu(i, mask)     for ( (i) = 0; (i) < 1; ++(i) )
#define alternative(a, b, c)      ((void)(a), (void)(b))
#define DECLARE_PER_CPU(type, name) static __used type shim_per_cpu__##name
static cpumask_t cpu_online_map = ~0UL;

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
#define mfn_to_pdx(mfn) mfn_x(mfn)
#define pdx_to_mfn(pdx) _mfn(pdx)
#define page_to_pdx(pg) ((unsigned long)((pg) - frame_table))
#define pdx_to_page(pdx) (frame_table + (pdx))
#define __mfn_valid(mfn) true

/* tlbflush.h */
#define per_cpu(a, b) (0)
bool tlb_clk_enabled;
u32 tlbflush_clock;
#define tlbflush_time 0U
#define __cpumask_clear_cpu(cpu, mask) ((void)(cpu), (void)(mask))

#ifdef __x86_64__
#define cpu_relax()                         ((void)0)
#define test_and_clear_bit(nr, addr)                         \
        ({                                                   \
             bool was_set = test_bit((nr), (addr));          \
             *(addr) &= ~(1UL << (nr));                      \
             was_set;                                        \
         })
#define cmpxchg(ptr, old, new)                               \
        ({                                                   \
             __typeof__(*(ptr)) old__ = (old);               \
             __typeof__(*(ptr)) new__ = (new);               \
             __sync_val_compare_and_swap((ptr), old__, new__); \
         })
unsigned int attr_const generic_flsl(unsigned long x)
{
    return ((x) ? (sizeof(long) * 8) - __builtin_clzl(x) : 0);
}
unsigned int attr_const generic_ffsl(unsigned long x)
{
    return __builtin_ffsl((long)(x));
}
#else
void flush_page_to_ram(unsigned long mfn, bool sync_icache)
{
    (void)mfn;
    (void)sync_icache;
}
#endif

/*
 * Some functions need to use types defined in specific headers,
 * so we include them and define header guards to prevent unwanted
 * definitions from those headers that conflict with the test harness
 * or bring in Xen-internal structures that are already provided by
 * the natural C compiler defines, libc defines and stubs in this shim.
 */
#define __TYPES_H__
#define __LIB_H__ /* C runtime library, only for the hypervisor */
#define XEN_SOFTIRQ_H
#define XEN__XVMALLOC_H
#define _LINUX_INIT_H
#define __LINUX_NODEMASK_H
#define __RWLOCK_H__
#define __XEN_SMP_H__
#define __XEN_BUG_H__
#define __XEN_ERRNO_H__
#define _TIMER_H_
#define __XEN_PERCPU_H__
#define __XEN_STRING_H__
#define __XEN_TIME_H__
#define __XEN_RCUPDATE_H
#define __XEN_PREEMPT_H__
#define __XEN_TASKLET_H__
#define __XEN_PERFC_H__
#define __SPINLOCK_H__
#define __VM_EVENT_H__
#define __XEN_EVENT_H__
#define __XEN_CPUMASK_H
#define __XEN_IRQ_H__
#define __XEN_PDX_H__
#define __ASM_DOMAIN_H__
#define __ARCH_X86_ATOMIC__
#define __X86_ALTERNATIVE_H__
#endif
