/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include xen/common/sysctl.c in host-side tests.
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_SHIM_H

#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#define __CONSOLE_H__
#define read_console_ring(op)     ((void)(op), -EOPNOTSUPP)

#define DEFINE_RCU_READ_LOCK(x) static rcu_read_lock_t x

/* domlist_read_lock is declared extern in sched.h; provide the definition. */
rcu_read_lock_t domlist_read_lock;

/*
 * XSM hooks used by sysctl.c beyond those already stubbed in
 * domctl-shim.h and memory-shim.h.
 * The test environment permits all operations.
 */
#define xsm_sysctl(xsm, d)          ((void)(d), 0)
#define xsm_readconsole(xsm, clear) ((void)(clear), 0)
#define xsm_page_offline(xsm, cmd)  ((void)(cmd), 0)

#define handle_keypress(key, ctx) ((void)(key), (void)(ctx))
#define scheduler_id()            0
#define sched_adjust_global(op)   ((void)(op), -EOPNOTSUPP)
#define get_cpu_idle_time(cpu)    ((void)(cpu), 0ULL)
#define cpupool_do_sysctl(op)     ((void)(op), -EOPNOTSUPP)
/* CPU topology: single-socket single-core test host. */
#ifdef __x86_64__
#define cpu_to_core(cpu)   ((void)(cpu), 0U)
#define cpu_to_socket(cpu) ((void)(cpu), 0U)
#endif

/* get_upper_mfn_bound: arch-specific stub; return 0 for the test. */
#define get_upper_mfn_bound() 0UL

/* arch_do_physinfo: no-op arch extension to XEN_SYSCTL_physinfo. */
#define arch_do_physinfo(pi) ((void)(pi))

/* arch_do_sysctl: fall-through for unrecognised sysctl commands. */
#define arch_do_sysctl(op, u) ((void)(op), (void)(u), -EOPNOTSUPP)

#define iommu_enabled false
#define iommu_hap_pt_share false
#define vpmu_is_available false
#define opt_gnttab_max_version 0
#endif
#endif
