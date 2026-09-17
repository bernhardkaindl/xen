/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shim for including xen/common/numa.c into test environments
 *
 * Copyright (C) 2026 Cloud Software Group
 */

/* definitions for NUMA testing */
#define arch_numa_disabled()    false
#define arch_numa_unavailable() false
#define vmap_contig(mfn, nr) (assert(!(mfn)), calloc(PAGE_SIZE, nr))
void numa_fw_bad(void) { }

#include <common/numa.c>

/* For the purposes of the testing assume arch NID == Xen NID. */
unsigned int numa_node_to_arch_nid(nodeid_t n)
{
    return (unsigned int)n;
}
