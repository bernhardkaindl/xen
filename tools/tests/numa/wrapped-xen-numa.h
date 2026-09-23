/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit tests for NUMA setup.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef WRAPPED_XEN_NUMA_H
#define WRAPPED_XEN_NUMA_H

#define CONFIG_DEBUG
#define CONFIG_NUMA
#define CONFIG_NR_NUMA_NODES 64
#define CONFIG_NR_CPUS 256
#define MAX_RANGES 128

#include <native/config.h>
#include <native/bitmap-wrapper.h>
#include <native/numa-shim.h>
#define bitmap_clear(bitmap, order) bitmap_clear(bitmap, 0, order)

/*
 * Dummy helper to satisfy allocate_cachealigned_memnodemap(), the memory
 * allocation is instead done in vmap_contig().
 */
static inline mfn_t alloc_boot_pages(unsigned long nr, unsigned long align)
{
    return _mfn(0);
}

static inline void panic(const char *msg)
{
    printf("%s\n", msg);
    abort();
}

static paddr_t mem_hotplug;
unsigned int __read_mostly nr_cpu_ids = NR_CPUS;

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
