/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include xen/common/sysctl.c in host-side tests.
 *
 * This shim provides the minimal Xen definitions that sysctl.c
 * needs to run in a host-side test environment.  It replaces a
 * minimal subset of the Xen environment that xen/common/sysctl.c
 * interacts with with stubs so it can run in the test environment,
 * allowing test scenarios to verify the behavior of sysctl.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_SHIM_H

/*
 * Guard against language servers and linters picking up this header.
 *
 * This shim is intended to be used in test programs for testing
 * the code of xen/common/sysctl.c in a host-side test environment,
 * and test programs need to define TEST_WRAP_XEN_COMMON_SYSCTL_C
 * to enable the definitions in this header.
 */
#ifndef TEST_WRAP_XEN_COMMON_SYSCTL_C
#warning "Include this header only in integration tests using sysctl.c"
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
 * symbols that sysctl.c actually uses from those headers are provided below.
 */

/* console.h: read_console_ring stub provided below. */
#define __CONSOLE_H__

/*
 * RCU read-side locking.
 *
 * struct _rcu_read_lock, rcu_read_lock_t, rcu_read_lock(), and
 * rcu_read_unlock() are now provided by page-alloc-shim.h (before sched.h
 * is included).  Only keep the helpers that sysctl-shim.h itself uses.
 */
#define DEFINE_RCU_READ_LOCK(x) static rcu_read_lock_t x

/* domlist_read_lock is declared extern in sched.h; provide the definition. */
rcu_read_lock_t domlist_read_lock;

/*
 * XSM hooks used by sysctl.c beyond those already stubbed in
 * domctl-shim.h and memory-shim.h.
 * The test environment permits all operations.
 */
#define xsm_sysctl(xsm, d)             ((void)(d), 0)
#define xsm_readconsole(xsm, clear)    ((void)(clear), 0)
#define xsm_page_offline(xsm, cmd)     ((void)(cmd), 0)

/* console.h stubs: read_console_ring is not tested here. */
#define read_console_ring(op)           ((void)(op), -EOPNOTSUPP)

/*
 * keyhandler.h is already included by page-alloc-shim.h; stub
 * handle_keypress as a no-op so the debug-keys sysctl case compiles.
 */
#define handle_keypress(key, ctx)       ((void)(key), (void)(ctx))

/*
 * Scheduler stubs.
 *
 * sched.h is blocked by __SCHED_H__ in page-alloc-shim.h.
 */
#define scheduler_id()                  0
#define sched_adjust_global(op)         ((void)(op), -EOPNOTSUPP)
#define get_cpu_idle_time(cpu)          ((void)(cpu), 0ULL)

/* cpupool_do_sysctl: cpupool operations are not tested here. */
#define cpupool_do_sysctl(op)           ((void)(op), -EOPNOTSUPP)

/*
 * CPU and node count stubs.
 *
 * cpumask.h is blocked by __XEN_CPUMASK_H in page-alloc-shim.h.
 * nodemask.h is blocked by __LINUX_NODEMASK_H there too.
 */
#define nr_cpu_ids                      1U
#define num_online_cpus()               1U

/* cpumask_last: highest present CPU index.  Single-CPU host → 0. */
#define cpumask_last(mask)              0U

/*
 * cpu_present_map: reuse cpu_online_map which is already defined as a
 * static cpumask_t in page-alloc-shim.h.
 */
#define cpu_present_map                 cpu_online_map
#define cpu_present(i)                  ((i) == 0U)

/*
 * last_node(mask): index of the highest-set bit in the nodemask.
 * nodemask_t is unsigned long in this test environment (harness.h).
 * Return MAX_NUMNODES-1 so num_nodes = MAX_NUMNODES consistently with
 * num_online_nodes().
 */
#define last_node(mask)                 ((int)(MAX_NUMNODES - 1))

/* CPU topology: single-socket single-core test host. */
#ifdef __x86_64__
#define cpu_to_core(cpu)                ((void)(cpu), 0U)
#define cpu_to_socket(cpu)              ((void)(cpu), 0U)
#endif

/* get_upper_mfn_bound: arch-specific stub; return 0 for the test. */
#define get_upper_mfn_bound()           0UL

/* arch_do_physinfo: no-op arch extension to XEN_SYSCTL_physinfo. */
#define arch_do_physinfo(pi)            ((void)(pi))

/* arch_do_sysctl: fall-through for unrecognised sysctl commands. */
#define arch_do_sysctl(op, u)           ((void)(op), (void)(u), -EOPNOTSUPP)

/*
 * cpu_khz: nominal 1 GHz for the test environment.
 *
 * Using a variable rather than a macro avoids the preprocessor expanding
 * the name inside struct-member access expressions like pi->cpu_khz.
 */
static unsigned long cpu_khz = 1000000UL;

/* IOMMU: not configured in the test environment. */
#define iommu_enabled      false
#define iommu_hap_pt_share false

/*
 * vmtrace_available is already #define'd false via xen/domain.h when
 * CONFIG_VMTRACE is not set.
 *
 * vpmu_is_available is declared extern in xen/domain.h with no CONFIG
 * guard; shadow it here so no definition is needed at link time.
 */
#define vpmu_is_available false

/* xen/grant_table.h is blocked by __XEN_GRANT_TABLE_H__ in memory-shim.h. */
#define opt_gnttab_max_version 0
#endif
#endif
