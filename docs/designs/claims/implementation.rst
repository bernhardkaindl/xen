.. SPDX-License-Identifier: CC-BY-4.0

#####################
Claims Implementation
#####################

.. contents:: Table of Contents
    :backlinks: entry
    :local:

.. note:: This part describes implementation details of claims and their
    interaction with memory allocation in Xen. It covers the functions and
    data structures involved in :term:`installing claims`, allocating memory
    with :term:`claims`, and handling related edge cases.

Functions related to the implementation of claims and their interaction
with memory allocation.

**********************
Installation of claims
**********************

This section describes the functions and data structures involved
in :term:`installing claims` for domains and the internal functions for
validating and installing claim sets.

xc_domain_claim_memory()
------------------------

.. c:function:: int xc_domain_claim_memory(xc_interface *xch, \
                                           uint32_t domid, \
                                           uint32_t nr_claims, \
                                           memory_claim_t *claims)

    :param xch:       The libxenctrl interface to use for the hypercall
    :param domid:     The ID of the domain for which to install the claim set
    :param nr_claims: The number of claims in the claim set
    :param claims:    The claim set to install for the domain
    :type xch:        xc_interface *
    :type domid:      uint32_t
    :type nr_claims:  uint32_t
    :type claims:     memory_claim_t *
    :returns:         0 on success, or a negative error code on failure.

    Wrapper for :c:expr:`XEN_DOMCTL_claim_memory` to install
    :ref:`claim sets <designs/claims/installation:claim sets>` for a domain.

domain_set_outstanding_pages()
------------------------------

.. c:function:: int domain_set_outstanding_pages(struct domain *d, \
                                                 unsigned long pages)

    :param d:     The domain for which to set the outstanding claims
    :param pages: The number of pages to claim globally for the domain
    :type d:      struct domain *
    :type pages:  unsigned long
    :returns: 0 on success, or a negative error code on failure.

    Handles claim installation for :c:expr:`XENMEM_claim_pages` and
    :c:expr:`XEN_DOMCTL_claim_memory` with
    :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_LEGACY` by setting the domain's
    :term:`global claims` to the specified number of pages. It calculates
    the claims as the requested pages minus the domain's total pages.
    When :c:expr:`pages == 0`, it clears the claims of the domain.

domain_set_node_claims()
------------------------

.. c:function:: int domain_set_node_claims(struct domain *d, \
                                           unsigned int nr_claims, \
                                           memory_claim_t *claims)

    :param d: The domain for which to set the node claims
    :param nr_claims: The number of claims in the claim set
    :param claims: The claim set to install for the domain
    :type claims: memory_claim_t *
    :type d: struct domain *
    :type nr_claims: unsigned int
    :returns: 0 on success, or a negative error code on failure.

    Handles :term:`installing claim sets`. It performs the validation
    of the :term:`claim set` and updates the domain's claims accordingly.

    The function works in four phases:

     1. Validating claim entries and checking node-local availability
     2. Validating total claims and checking global availability
     3. Resetting any current claims of the domain
     4. Installing the claim set as the domain's claiming state

    Phase 1 checks claim entries for validity and memory availability:

     1. Target must be :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_GLOBAL` or a node.
     2. Each target node may only appear once in the claim set.
     3. For node-local claims, requested pages must not exceed the available
        memory on that node after accounting for existing claims.
     4. The explicit padding field must be zero for forward compatibility.

    Phase 2 checks:

     1. The sum of claims must not exceed globally available memory.
     2. The claims must not exceed the :c:expr:`domain.max_pages` limit.
        See :doc:`accounting` and :doc:`retirement` for the accounting
        checks that enforce the domain's :c:expr:`domain.max_pages` limit.

************************************
Helper functions for managing claims
************************************

:c:expr:`claims_retire_global()` and :c:expr:`claims_retire_node()` are helper
functions used to retire claims when necessary:

- :c:expr:`claims_retire_allocation()`
  uses them to retire claims when allocating memory.
- :c:expr:`claims_retire_nodes()`
  uses :c:expr:`claims_retire_node()` to reset all node-local claims
  of a domain when resetting the claim state of the domain.
- :c:expr:`reserve_offlined_page()`
  uses them to recall claims when offlining pages reduces
  available memory below the currently claimed memory. See
  :ref:`designs/claims/implementation:Offlining memory in presence of claims`
  for further information.

claims_retire_global()
----------------------

.. c:function:: unsigned long claims_retire_global(struct domain *d, \
                                                   unsigned long \
                                                   pages_to_retire)

    :param d: The domain for which to retire the global claim
    :param pages_to_retire: The number of pages to retire
    :type d: struct domain *
    :type pages_to_retire: unsigned long
    :returns: The number of pages actually retired from the global claim.

    This function retires the specified number of globally claimed pages
    and updates the global outstanding totals accordingly.

claims_retire_node()
--------------------

.. c:function:: unsigned long claims_retire_node(struct domain *d, \
                                                 nodeid_t node, \
                                                 unsigned long pages_to_retire)

    :param d: The domain for which to retire the node claim
    :param node: The node for which to retire the claim
    :param pages_to_retire: The number of pages to retire from the claim
    :type d: struct domain *
    :type node: nodeid_t
    :type pages_to_retire: unsigned long
    :returns: The number of pages actually retired from the claim

    This function retires a specified number of pages from a domain's
    claim on a specific node. It limits the retirement to the number of
    pages actually claimed by the domain on that node and updates the
    node-local claims currently held by the domain on that node,
    and it updates the global and node-level claim state accordingly.

claims_retire_nodes()
---------------------

.. c:function:: void claims_retire_nodes(struct domain *d)

    :param d: The domain for which to retire the node claims.
    :type d: struct domain *

    This function is used by
    :ref:`designs/claims/implementation:domain_set_outstanding_pages()`
    to reset node-local parts of the domain's claiming state.

**********************
Allocation with claims
**********************

The functions below play a key role in allocating memory for domains.

xc_domain_populate_physmap()
----------------------------

 .. c:function:: int xc_domain_populate_physmap(xc_interface *xch, \
                                           uint32_t domid, \
                                           unsigned long nr_extents, \
                                           unsigned int extent_order, \
                                           unsigned int mem_flags, \
                                           xen_pfn_t *extent_start)

    :param xch: The :term:`libxenctrl` interface
    :param domid: The ID of the domain
    :param nr_extents: Number of extents
    :param extent_order: Order of the extents
    :param mem_flags: Allocation flags
    :param extent_start: Starting PFN
    :type xch: xc_interface *
    :type domid: uint32_t
    :type nr_extents: unsigned long
    :type extent_order: unsigned int
    :type mem_flags: unsigned int
    :type extent_start: xen_pfn_t *
    :returns: 0 on success, or a negative error code on failure.

    This function is a wrapper for the ``XENMEM_populate_physmap`` hypercall,
    which is handled by the :c:expr:`populate_physmap()` function in the
    hypervisor. It is used by :term:`libxenguest` for populating the
    :term:`guest physical memory` of a domain. :term:`domain builders` can
    set the :term:`NUMA node affinity` and pass the preferred node to this
    function to steer allocations towards the preferred NUMA node(s) and let
    :term:`claims` ensure that the memory will be available even in cases
    of :term:`parallel domain builds` where multiple domains are being built
    at the same time.


populate_physmap()
------------------

The :term:`meminit` API calls :c:expr:`xc_domain_populate_physmap()`
for populating the :term:`guest physical memory`. It invokes the restartable
``XENMEM_populate_physmap`` hypercall implemented by
:c:expr:`populate_physmap()`.

.. c:function:: void populate_physmap(struct memop_args *a)

    :param a: Provides status and hypercall restart info
    :type a: struct memop_args *

    Allocates memory for building a domain and uses it for populating the
    :term:`physmap`. For allocation, it uses
    :c:expr:`alloc_domheap_pages()`, which forwards the request to
    :c:expr:`alloc_heap_pages()`.

    During domain creation, it adds the ``MEMF_no_scrub`` flag to the request
    for populating the :term:`physmap` to optimize domain startup by allowing
    the use of unscrubbed pages.

    When that happens, it scrubs the pages as needed using hypercall
    continuation to avoid long hypercall latency and watchdog timeouts.

    Domain builders can optimise on-demand scrubbing by running
    :term:`physmap` population pinned to the domain's NUMA node,
    keeping scrubbing local and avoiding cross-node traffic.

alloc_heap_pages()
------------------

.. c:function:: struct page_info *alloc_heap_pages(unsigned int zone_lo, \
                                                   unsigned int zone_hi, \
                                                   unsigned int order, \
                                                   unsigned int memflags, \
                                                   struct domain *d)

    :param zone_lo: The lowest zone index to consider for allocation
    :param zone_hi: The highest zone index to consider for allocation
    :param order: The order of the pages to allocate (2^order pages)
    :param memflags: Memory allocation flags that may affect the allocation
    :param d: The domain for which to allocate memory or NULL
    :type zone_lo: unsigned int
    :type zone_hi: unsigned int
    :type order: unsigned int
    :type memflags: unsigned int
    :type d: struct domain *
    :returns: The allocated page_info structure, or NULL on failure

    This function allocates a contiguous block of pages from the heap.
    It checks claims and available memory before attempting the
    allocation. On success, it updates relevant counters and retires
    claims as necessary.

    It first checks whether the request can be satisfied given the domain's
    claims and available memory using :c:expr:`claims_permit_request()`.
    If claims and availability permit the request, it calls
    :c:expr:`get_free_buddy()` to find a suitable block of free pages
    while respecting node and zone constraints.

    If ``MEMF_no_scrub`` is allowed, it may return unscrubbed pages. When that
    happens, :c:expr:`populate_physmap()` scrubs them if needed with hypercall
    continuation to avoid long hypercall latency and watchdog timeouts.

    Simplified pseudo-code of its logic:
.. code:: C

    struct page_info *alloc_heap_pages(unsigned int zone_lo,
                                       unsigned int zone_hi,
                                       unsigned int order,
                                       unsigned int memflags,
                                       struct domain *d) {
        /* Check whether claims and available memory permit the request.
         * `avail_pages` and `claims` are placeholders for the appropriate
         * global or node-local availability/counts used by the real code. */
        if (!claims_permit_request(d, avail_pages, claims, memflags,
                                   1UL << order, NUMA_NO_NODE))
            return NULL;

        /* Find a suitable buddy block. Pass the zone range, order and
         * memflags so the helper can apply node and zone selection. */
        pg = get_free_buddy(zone_lo, zone_hi, order, memflags, d);
        if (!pg)
            return NULL;

        claims_retire_allocation(d, 1UL << order, node_of(pg));
        update_counters_and_stats(d, order);
        if (pg_has_dirty_pages(pg))
            scrub_dirty_pages(pg);
        return pg;
    }

get_free_buddy()
----------------

.. c:function:: struct page_info *get_free_buddy(unsigned int zone_lo, \
                                                 unsigned int zone_hi, \
                                                 unsigned int order, \
                                                 unsigned int memflags, \
                                                 const struct domain *d)

    :param zone_lo: The lowest zone index to consider for allocation
    :param zone_hi: The highest zone index to consider for allocation
    :param order: The order of the pages to allocate (2^order pages)
    :param memflags: Flags for conducting the allocation
    :param d: domain to allocate memory for or NULL
    :type zone_lo: unsigned int
    :type zone_hi: unsigned int
    :type order: unsigned int
    :type memflags: unsigned int
    :type d: struct domain *
    :returns: The allocated page_info structure, or NULL on failure

    This function finds a suitable block of free pages in the buddy
    allocator while respecting claims and node-level available memory.

    Called by :c:expr:`alloc_heap_pages()` after verifying the request is
    permissible, it iterates over nodes and zones to find a buddy block
    that satisfies the request. It checks node-local claims before
    attempting allocation from a node.

    Using :c:expr:`claims_permit_request()`, it checks whether the node
    has enough unclaimed memory to satisfy the request or whether the
    domain's claims can permit the request on that node after accounting
    for outstanding claims.

    If the node can satisfy the request, it searches for a suitable block
    in the specified zones. If found, it returns the block; otherwise it
    tries the next node until all online nodes are exhausted.

    Simplified pseudo-code of its logic:
.. code:: C

    /*
     * preferred_node_or_next_node() represents the policy to first try the
     * preferred/requested node then fall back to other online nodes.
     */
    struct page_info *get_free_buddy(unsigned int zone_lo,
                                     unsigned int zone_hi,
                                     unsigned int order,
                                     unsigned int memflags,
                                     const struct domain *d) {
        nodeid_t request_node = MEMF_get_node(memflags);

        /*
         * Iterate over candidate nodes: start with preferred node (if any),
         * then try other online nodes according to the normal placement policy.
         */
        while (there are more nodes to try) {
            nodeid_t node = preferred_node_or_next_node(request_node);
            if (!node_allocatable_request(d, node_avail_pages[node],
                                          node_outstanding_claims[node],
                                          memflags, 1UL << order, node))
                goto try_next_node;

            /* Find a zone on this node with a suitable buddy */
            for (int zone = highest_zone; zone >= lowest_zone; zone--)
                for (int j = order; j <= MAX_ORDER; j++)
                    if ((pg = remove_head(&heap(node, zone, j))) != NULL)
                        return pg;
         try_next_node:
            if (request_node != NUMA_NO_NODE && (memflags & MEMF_exact_node))
                return NULL;
            /* Fall back to the next node and repeat. */
        }
        return NULL;
    }

*******************************************
Helper functions for allocation with claims
*******************************************

For allocating memory while respecting claims, :c:expr:`alloc_heap_pages()`
and :c:expr:`get_free_buddy()` use :c:expr:`claims_permit_request()` to
check whether the claims permit the request before attempting allocation.

If permitted, the allocation proceeds, and after success,
:c:expr:`claims_retire_allocation()` retires the claims for the allocation
based on the domain's claiming state and the node of the allocation.

See :ref:`designs/claims/design:Key design decisions` for the
rationale behind this design and the accounting checks that enforce
the :c:expr:`domain.max_pages` limit during allocation with claims.

claims_permit_request()
-----------------------

.. c:function:: bool claims_permit_request(const struct domain *d, \
                                           unsigned long avail_pages, \
                                           unsigned long claims, \
                                           unsigned int memflags, \
                                           unsigned long request, \
                                           nodeid_t node)

    :param d: domain for which to check
    :param avail_pages: pages available globally or on node
    :param claims: outstanding claims globally or on node
    :param memflags: memory allocation flags for the request
    :param request: pages requested for allocation
    :param node: node of the request or NUMA_NO_NODE for global
    :type d: const struct domain *
    :type avail_pages: unsigned long
    :type claims: unsigned long
    :type memflags: unsigned int
    :type request: unsigned long
    :type node: nodeid_t
    :returns: true if claims and available memory permit the request, \
              false otherwise.

    This function checks whether a memory allocation request can be
    satisfied given the current state of available memory and outstanding
    claims for the domain. It calculates the amount of unclaimed memory
    and determines whether it is sufficient to satisfy the request.

    If unclaimed memory is insufficient, it checks if the domain's claims
    can cover the shortfall, taking into account whether the request is
    node-specific or global.

claims_retire_allocation()
--------------------------

.. c:function:: void claims_retire_allocation(struct domain *d, \
                                              unsigned long allocation, \
                                              nodeid_t alloc_node)

    :param d: The domain for which to retire claims
    :param allocation: The number of pages allocated
    :param alloc_node: The node on which the allocation was made
    :type d: struct domain *
    :type allocation: unsigned long
    :type alloc_node: nodeid_t

    See :doc:`retirement` for details on retiring claims after allocation.

**************************************
Offlining memory in presence of claims
**************************************

When offlining pages, Xen must ensure that available memory on a node or
globally does not fall below outstanding claims. If it does, Xen recalls
claims from domains until accounting is valid again.

This is triggered by privileged domains via the
``XEN_SYSCTL_page_offline_op`` sysctl or by machine-check memory errors.

Offlining currently allocated pages does not immediately reduce available
memory: pages are marked offlining and become offline only when freed.
Pages marked offlining will not become available again, so this does not
affect claim invariants.

However, when already free pages are offlined, free memory can drop
below outstanding claims; in that case the offlining process calls
:c:expr:`reserve_offlined_page()` to offline the page.

It checks whether offlining the page would cause available memory on the
page's node, or globally, to fall below the respective outstanding claims:

- When
  :c:expr:`node_outstanding_claims[offline_node]` exceeds
  :c:expr:`node_avail_pages[offline_node]` for the node of the offlined page,
  :c:expr:`reserve_offlined_page()` calls :c:expr:`claims_retire_node()`
  to recall claims on that node from domains with claims on the node of the
  offlined buddy until the claim accounting of the node is valid again.

- When total :c:expr:`outstanding_claims` exceeds :c:expr:`total_avail_pages`,
  :c:expr:`reserve_offlined_page()` calls :c:expr:`claims_retire_global()` to
  recall global claims from domains with global claims until global accounting
  is valid again.

This can violate claim guarantees, but it is necessary to maintain system
stability when memory must be offlined.

reserve_offlined_page()
-----------------------

.. c:function:: int reserve_offlined_page(struct page_info *head)

    :param head: The page being offlined
    :type head: struct page_info *
    :returns: 0 on success, or a negative error code on failure.

    This function is called during the offlining process to offline pages.

    If offlining a page causes available memory to fall below outstanding
    claims, it checks the node and global claim accounting and recalls
    claims from domains as necessary to ensure accounting invariants hold
    after a buddy is offlined.
