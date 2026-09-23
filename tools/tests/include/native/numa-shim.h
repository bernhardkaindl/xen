/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shim for including Xen's numa.h and numa.c into test environments
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_INCLUDE_NATIVE_NUMA_SHIM_H
#define TOOLS_TESTS_INCLUDE_NATIVE_NUMA_SHIM_H

/* Provide architectore-specific infrastructure: direct map offset helpers */
#define mfn_to_pdx(mfn)            mfn_x(mfn)
#define paddr_to_pdx(pa)           ((pa) >> PAGE_SHIFT)
#define maddr_to_directmapoff(ma)  ((unsigned long)(ma))
#define directmapoff_to_maddr(off) ((paddr_t)(off))
#include <asm/page.h>

/* Provide Xen nodemask definitions with _mfn()/mfn_x() type conversion */
#include <xen/nodemask.h>

/* For the purposes of the testing assume arch NID == Xen NID. */
#define numa_node_to_arch_nid(n) (n)

/* Include Xen's NUMA definitions and functions */
#include <xen/numa.h>

/* Remaining architecture-specific helpers required after Xen's numa.h */
#define arch_numa_disabled()     false
#define arch_numa_unavailable(x) false
#define vmap_contig(mfn, nr)     (assert(!mfn_x(mfn)), calloc(PAGE_SIZE, nr))
void numa_fw_bad(void)           { }

#endif /* TOOLS_TESTS_INCLUDE_NATIVE_NUMA_SHIM_H */
