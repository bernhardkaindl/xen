/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unit tests for NUMA setup.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef WRAPPED_XEN_NUMA_H
#define WRAPPED_XEN_NUMA_H

#define CONFIG_NUMA
#define __XEN_CPUMASK_H
#include <native/config.h>
#include <xen-tools/bitops.h>
#include <xen-tools/common-macros.h>

#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))
#define mfn_to_pdx(mfn)   mfn_x(mfn)
#define paddr_to_pdx(pa)  ((pa) >> PAGE_SHIFT)
#define mfn_to_maddr(mfn) (mfn_x(mfn) << PAGE_SHIFT)

#define ASSERT assert
#define ASSERT_UNREACHABLE() assert(0)

#define __set_bit set_bit
#define __clear_bit clear_bit

static inline unsigned int find_next_bit(
    const unsigned long *addr, unsigned int size, unsigned int off)
{
    unsigned int i;

    ASSERT(size <= BITS_PER_LONG);

    for ( i = off; i < size; i++ )
        if ( *addr & (1UL << i) )
            return i;

    return size;
}

#define find_first_bit(b, s) find_next_bit(b, s, 0)

/* Minimal cpumask support. */
typedef struct cpumask{ DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;

#define cpumask_clear_cpu(c, m) clear_bit(c, (m)->bits)

/* Define the nodemask helpers used. */
typedef struct nodemask{ DECLARE_BITMAP(bits, CONFIG_NR_NUMA_NODES); } nodemask_t;

#define node_set(node, dst) set_bit(node, (dst).bits)

#define first_node(n) __first_node(&(n), CONFIG_NR_NUMA_NODES)
static inline int __first_node(const nodemask_t *srcp, unsigned int s)
{
    return min(s, find_next_bit(srcp->bits, s, 0));
}

#define next_node(n, m) __next_node(n, &(m), CONFIG_NR_NUMA_NODES)
static inline int __next_node(unsigned int n, const nodemask_t *srcp,
                              unsigned int s)
{
    return min(s, find_next_bit(srcp->bits, s, n + 1));
}

#define nodes_or(dst, src1, src2) \
    bitmap_or((dst).bits, (src1).bits, (src2).bits, CONFIG_NR_NUMA_NODES)

static inline bool nodemask_test(unsigned int node, const nodemask_t *dst)
{
    return test_bit(node, dst->bits);
}

#define node_set_online(node)	   set_bit(node, node_online_map.bits)

#define cycle_node(n, src) __cycle_node(n, &(src), MAX_NUMNODES)
static inline int __cycle_node(int n, const nodemask_t *maskp,
                               unsigned int nbits)
{
    unsigned int nxt = __next_node(n, maskp, nbits);

    if ( nxt == nbits )
        nxt = __first_node(maskp, nbits);

    return nxt;
}

#define for_each_node_mask(node, mask)                  \
    for ( (node) = first_node(mask);                    \
          (node) < MAX_NUMNODES;                        \
          (node) = next_node(node, mask) )

/*
 * Dummy helper to satisfy allocate_cachealigned_memnodemap(), the memory
 * allocation is instead done in vmap_contig().
 */
static inline mfn_t alloc_boot_pages(unsigned long nr, unsigned long align)
{
    return _mfn(0);
}

static paddr_t mem_hotplug;
static unsigned int __read_mostly nr_cpu_ids = NR_CPUS;

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
