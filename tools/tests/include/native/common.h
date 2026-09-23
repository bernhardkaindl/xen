/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_COMMON_H
#define TOOLS_TESTS_NATIVE_HARNESS_COMMON_H

#include <native/config.h>
#include <native/bitmap-wrapper.h>
#pragma GCC diagnostic push
#if defined(__clang__ ) && defined(__riscv)
/*
 * For including xen/arch/riscv/include/asm/time.h, ignore non-definition
 * definition warning from __used in __section for clang in xen/compiler.h
 */
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#include <xen/time.h>
#pragma GCC diagnostic pop

#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define ACCESS_ONCE(x)            (x)
#define BUG()                     assert(0)

/* The test environment needs to define these as __used */
#define __initconst   __used
#define __initsetup   __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)

/* Page directory index helpers */
#define __mfn_valid(mfn) true
#define pdx_to_mfn(pdx)  _mfn(pdx)
#define page_to_pdx(pg)  ((unsigned long)((pg) - frame_table))
#define pdx_to_page(pdx) (frame_table + (pdx))

/* Blocking P2M headers needs fewer shims than including them */
#define _XEN_P2M_H
#define _XEN_ASM_X86_P2M_H
#define ASM__RISCV__P2M_H
#define mfn_to_pdx(mfn)  mfn_x(mfn)
#define paddr_to_pdx(pa) ((pa) >> PAGE_SHIFT)
#define map_mmio_regions(d, gfn, nr, mfn)   0
#define unmap_mmio_regions(d, gfn, nr, mfn) 0
struct p2m_domain { int dummy; }; /* riscv */

typedef bool spinlock_t;
typedef spinlock_t rwlock_t;
typedef spinlock_t rspinlock_t;
typedef spinlock_t percpu_rwlock_t;
#define spin_lock_kick()      ((void)0)
#define spin_is_locked(l)     true
#define rspin_is_locked(l)    true
#define DEFINE_SPINLOCK(l)    spinlock_t l
#define spin_lock(l)          ((void)(l))
#define spin_unlock(l)        ((void)(l))
#define spin_lock_cb(l, c, d) ((void)(l))
#define nrspin_lock(l)        ((void)(l))
#define nrspin_unlock(l)      ((void)(l))
#define rspin_lock(l)         ((void)(l))
#define rspin_unlock(l)       ((void)(l))
#define read_lock(l)          ((void)(l))
#define read_unlock(l)        ((void)(l))
#define write_lock(l)         ((void)(l))
#define write_unlock(l)       ((void)(l))

/* The real implementation is hidden with the other Xen-only allocator APIs. */
#define cmpxchgptr(ptr, old, new) ({ \
    __typeof__(*(ptr)) cmpxchg_old_ = *(ptr); \
    if ( cmpxchg_old_ == (old) ) \
        *(ptr) = (new); \
    cmpxchg_old_; \
})

/* nodemask support for the test environment. */
#define DECLARE_PER_CPU(type, name) static __used type shim_per_cpu__##name
cpumask_t cpu_online_map;
cpumask_t cpu_present_map;
unsigned int nr_cpu_ids = NR_CPUS;

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
#define XEN_SOFTIRQ_H
#define XEN__XVMALLOC_H
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
