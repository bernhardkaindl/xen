.. SPDX-License-Identifier: CC-BY-4.0
.. _memory_claims:

Memory Claims
=============

Overview
--------

Xen's page allocator supports a **claims** mechanism that allows a domain
builder to reserve memory before allocation begins, preventing concurrent
allocations from exhausting available pages mid-build.

A claim can be global (host-level) or target a specific NUMA node, ensuring
that a domain's memory is allocated locally on the same node as its vCPUs.

Claims Implementation
---------------------

The Xen hypervisor maintains per-domain claim state for pages claimed, but
not yet allocated for a domain. As pages are allocated for the domain, the
outstanding claim is consumed by allocating pages for the domain, and the
corresponding claim counters are reduced by the number of pages allocated.

The claims APIs allow a privileged guest to stake claims for a specified
number of pages of system memory for the domain.

The outstanding claims of a domain can be set to zero by making a claims
hypercall with the number of pages set to zero, which releases any remaining
claim for the domain.

For the new multi-claim ``XEN_DOMCTL_claim_memory`` API, the passed claims are
treated as absolute replacement targets for the domain's existing claims.
This allows privileged domain builders to grow, shrink, or move claims between
NUMA nodes, and to combine node-local claims with a global host-level claim.

The legacy single global claim path is intentionally left unchanged for
compatibility. Its semantics remain relative to the domain's already allocated
pages, and it does not gain the new replacement/update behaviour.

If the claim call is successful, Xen updates the domain's claim state to
reflect the new claim targets and protects the claimed memory so it remains
allocatable for the domain until it is either allocated for the domain or the
claim is released.

A domain builder (toolstack in a privileged domain) building the domain can then
allocate the guest memory for the domain, which converts the outstanding claim
into actual memory of the new domain, backed by physical pages.

For the legacy single global claim path, the resulting claim remains relative
to the already allocated pages for the domain, so the **pages** argument is
absolute and must correspond to the total number expected to be allocated for
the domain, not an increment above the already allocated pages.

For the new multi-claim API, each claim entry instead specifies the absolute
target claim for its node or for the global host-level bucket, replacing only
the domain's previous claim state for those entries, and on top of any already
allocated pages for the domain. This allows toolstacks to adjust claims
dynamically as the domain build progresses, and to set different claim targets
for different NUMA nodes.

Headroom for Xen's own memory usage
-----------------------------------

Memory allocations by Xen for the domain also consume from the claim:

Toolstacks should stake a claim that is larger than the guest memory
requirement to account for Xen's own memory usage. The exact amount
of extra memory required depends on:

- the host architecture
- the configuration and features used by the domain (e.g. #vCPUS, PV vs HVM)
- and the features enabled by the Xen hypervisor on the host.

Life-cycle of a claim
---------------------

The Domain's maximum memory limit must be set prior to staking a claim as
the sum of the already allocated pages and the claim must be within that limit.

To release the claim after the domain build is complete, call this hypercall
command with the pages argument set to zero. This releases any remaining claim.
`libxenguest` does this after the guest memory has been allocated for the domain
and Xen does this also when it kills the domain.

The host-level claims check subtracts global outstanding claims from total
available pages. If the allocating domain has claims, its own claims are added
back as available (simplified pseudo-code):

.. code:: C

   ASSERT(spin_is_locked(&heap_lock));
   unsigned long global_avail = total_avail_pages - outstanding_claims
                                + d->global_claims + d->node_claims;
   return alloc_request <= global_avail;

Similarly, the per-node check enforces node-level claims by subtracting
outstanding node claims from available node pages, and adding back the
allocating domain's claim for the target node:

.. code:: C

   ASSERT(spin_is_locked(&heap_lock));
   unsigned long avail = node_avail_pages(node)
                         - node_outstanding_claims(node)
                         + d->claims[node];
   return alloc_request <= avail;

Simplified pseudo-code for the claims checks in the buddy allocator:

.. code:: C

    struct page_info *get_free_buddy(order, memflags, d) {
        for ( ; ; ) {
            node = preferred_node_or_next_node();
            if (!node_allocatable_request(d, memflags, 1 << order, node))
                goto try_next_node;
            /* Find a zone on this node with a suitable buddy */
            for (zone = highest_zone; zone >= lowest_zone; zone--)
                for (j = order; j <= MAX_ORDER; j++)
                    if (pg = remove_head(&heap(node, zone, j)))
                        return pg;
         try_next_node:
            if (req_node != NUMA_NO_NODE && memflags & MEMF_exact_node)
                return NULL;
            /* Fall back to the next node and repeat. */
        }
    }

    struct page_info *alloc_heap_pages(d, order, memflags) {
        if (!host_allocatable_request(d, memflags, 1 << order))
            return NULL;
        pg = get_free_buddy(order, memflags, d);
        if (!pg) /* Retry allowing unscrubbed pages */
            pg = get_free_buddy(order, memflags|MEMF_no_scrub, d);
        if (!pg)
            return NULL;
        if (pg has dirty pages)
            scrub_dirty_pages(pg);
        return pg;
    }

.. note:: The first ``get_free_buddy()`` pass skips unscrubbed pages and may
    fall back to other nodes. With ``memflags & MEMF_exact_node``, no fallback
    occurs, so the first pass may return ``NULL``.
    The 2nd pass with ``MEMF_no_scrub`` will consider the unscrubbed pages.
    ``alloc_heap_pages()`` then scrubs them before returning, guaranteeing the
    domain gets the desired node-local pages even when scrubbing is pending.

    Therefore, toolstacks should set ``MEMF_exact_node`` in ``memflags`` when
    allocating for a domain with a NUMA-aware claim, using
    ``XENMEMF_exact_node(node)``.

    For efficient scrubbing, toolstacks might want to run domain builds
    pinned on a CPU of the target NUMA node to scrub the pages on that node
    without cross-node traffic and lower latency to speed up domain build.

Data Structures
---------------

The following diagram shows the relationships between global, per-node,
and per-domain claim counters, all protected by the global ``heap_lock``.

.. mermaid::

   graph TB
    subgraph "Protected by the heap_lock"
       direction TB
       Global --Sum of--> Per-node
       Per-node --Sum of--> Per-domain
    end
    subgraph Per-domain
        direction LR
        per_node_claims["d->claims[node]"]
        global_claims["d->global_claims"]
        global_claims --stores--> global claims for domain
        node_claims["d->node_claims"] -->  node-specific claims for domain[]
        node_claims --tracks sum of--> claims[]
    end
    subgraph Per-node
        direction LR
        node_outstanding_claims--constrains-->node_avail_pages
    end
    subgraph Global
        direction LR
        outstanding_claims--constrains-->total_avail_pages
    end
