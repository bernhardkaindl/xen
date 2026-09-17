/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit tests for NUMA setup.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef WRAPPED_XEN_NUMA_H
#define WRAPPED_XEN_NUMA_H

#define CONFIG_NUMA
#include <native/bitmap-support.h>
#include <native/config.h>
#include <native/bitops.h>
#include <native/bitmap-shim.h>
#include <xen/nodemask.h>

#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))
#define mfn_to_maddr(mfn) (mfn_x(mfn) << PAGE_SHIFT)

/*
 * Dummy helper to satisfy allocate_cachealigned_memnodemap(), the memory
 * allocation is instead done in vmap_contig().
 */
static inline mfn_t alloc_boot_pages(unsigned long nr, unsigned long align)
{
    return _mfn(0);
}

static paddr_t mem_hotplug;
unsigned int __read_mostly nr_cpu_ids = NR_CPUS;

#include <xen/numa.h>

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
