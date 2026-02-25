.. SPDX-License-Identifier: CC-BY-4.0
.. _memory_claims:

Memory Claims
=============

Overview
--------

Xen's page allocator supports a **claims** mechanism that allows a domain
builder to reserve memory before allocation begins, preventing concurrent
allocations from exhausting available pages mid-build.
A claim can be global (host-wide) or target a specific NUMA node, ensuring
that a domain's memory is allocated locally on the same node as its vCPUs.

Claims arithmetic
-----------------

The host-wide claims check subtracts global claims from total available pages.
If the domain has claims, its ``d->outstanding_pages`` are added back as
available (simplified pseudo-code):

.. code:: C

   ASSERT(spin_is_locked(&heap_lock));
   unsigned long global_avail = total_avail_pages - outstanding_claims
                                                    + d->outstanding_pages;
   return alloc_request <= global_avail;

Similarly, the per-node check enforces node-level claims by subtracting
outstanding node claims from available node pages, and adding back the domain's
claim if allocating from the claimed node:

.. code:: C

   ASSERT(spin_is_locked(&heap_lock));
   unsigned long avail = node_avail_pages(node)
                         - node_outstanding_claims(node)
                         + (node == d->claim_node ? d->outstanding_pages : 0);
   return alloc_request <= avail;

.. note:: This check is performed when considering an allocation on a specific
    node. When the domain does not have a claim sufficient for the allocation
    on that node, the allocator will protect the outstanding claims on that node
    and instead try to fall back to other nodes (unless the
    allocation was node-exact, in which case
    ``get_free_buddy()`` returns ``NULL``).

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
    fall back to other nodes.
    With ``memflags & MEMF_exact_node``, no fallback occurs, so the first may
    return ``NULL``. The 2nd pass with ``MEMF_no_scrub`` will consider the
    unscrubbed pages.
    ``alloc_heap_pages()`` then scrubs them before returning, guaranteeing the
    domain gets the desired node-local pages even when scrubbing is pending.

    Therefore, toolstacks should use  ``MEMF_exact_node(node)`` to ensure local
    allocations and prevent the allocator from falling back to other nodes.
    For efficient scrubbing, toolstacks might experiment with domain builds
    pinned on a CPU of the target NUMA node to scrub the pages on that node
    without cross-node traffic and lower latency to speed up domain build.

Data Structures
---------------

The following diagram shows the relationships between global,
per-node, and per-domain claim counters, all protected by
``heap_lock``.

.. mermaid::

   graph TB
    subgraph "Protected by the heap_lock"
       direction TB
       Global --Sum of--> Per-node
       Per-node --Sum of--> Per-domain
    end
    subgraph Per-domain
        direction LR
        claim_node["d->claim_node"]
        claim_node --claims on--> outstanding_pages["d->outstanding_pages"]
    end
    subgraph Per-node
        direction LR
        node_outstanding_claims--constrains-->node_avail_pages
    end
    subgraph Global
        direction LR
        outstanding_claims--constrains-->total_avail_pages
    end
