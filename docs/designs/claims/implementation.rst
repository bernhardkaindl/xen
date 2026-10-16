.. SPDX-License-Identifier: CC-BY-4.0

Implementation
##############

.. contents:: Table of Contents
    :backlinks: entry
    :local:

.. note:: This part describes implementation details of claims and their
    interaction with memory allocation in Xen. It covers the functions and
    data structures involved in :term:`installing claims` and allocating memory
    with :term:`claims`.

Functions related to the implementation of claims and their interaction
with memory allocation.

**********************
Installation of claims
**********************

This section describes the functions and data structures involved in
:term:`installing claims` for domains, and the internal functions for
validating and installing claim sets.

.. c:function:: int domain_set_claims(domain, request)

    :param domain: The domain for which to set the claims
    :param request: The claim entries and derived page totals
    :type domain: struct domain *
    :type request: struct claim_request *
    :returns: 0 on success, or a negative error code on failure.

    Installs a non-empty claim set for a domain that has no outstanding
    claims. The domctl handler rejects requests after domain creation has
    finished. The domain builder must serialize installation with allocations
    for the domain, including completion of allocation continuations.

    The operation validates entries and computes total and per-node page
    counts, allocates a zeroed per-node array if needed, then checks memory
    availability and installs the state under :c:var:`heap_lock`. Each entry
    must request at least one page, have zero padding, and target a unique
    online node or :c:macro:`XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST`. The request
    total plus the domain's existing allocation must fit within
    :c:member:`domain.max_pages`. Global unclaimed memory must cover the sum
    of all entries, including node claims, and each targeted node's unclaimed
    memory must cover that node's entry. A domain with outstanding claims is
    rejected with ``-EINVAL``; existing claims are not replaced.

.. c:function:: int domain_get_claims(domain, request)

    :param domain: The domain whose claims are retrieved
    :param request: Entry buffer and its capacity; updated with the required
                         number of entries
    :type domain: const struct domain *
    :type request: struct claim_request *
    :returns: 0 on success with nr_entries updated to the number of claims
                written to the buffer, or a negative error code on failure.

    Retrieves the host-wide claim first, followed by per-node claims in
    ascending node order. The input count is the buffer capacity; on
    ``-ENOBUFS``, it is updated to the number of entries required.
    The helper can fill a private prefix before returning ``-ENOBUFS``;
    the domctl handler does not copy that prefix to the guest. Snapshotting
    takes :c:var:`heap_lock`, while the domctl's RCU reference protects the
    domain's lifetime. No domctl lock is required for get.

*************
Claim release
*************

Claims are released by passing an empty set to the set domctl. This reaches
:c:func:`domain_set_outstanding_pages()` with a zero page count, which removes
both host-wide and per-node claims and detaches the per-node array for freeing
after :c:var:`heap_lock` is released. The legacy
:c:macro:`XENMEM_claim_pages` path continues to use
:c:func:`domain_set_outstanding_pages()` for its existing interface.

Domain destruction also releases claims through this helper. The independent
creation-cleanup patch calls it when controller unpause first reduces the
pause count to zero, before architecture finalization and guest execution,
to release leftover build claims. That
one-time cleanup does not prohibit later legacy ``XENMEM_claim_pages`` calls.
It is not a prerequisite for the eight-patch NUMA-claims interface.

Both new domctls pass through the ordinary privileged domctl authorization
check before accessing claim entries. Under FLASK, set and release require
``domain2:setclaim``; get requires ``domain:getdomaininfo``. Sharing that
FLASK permission does not grant the old getdomaininfo operation's separate
dummy-XSM exception to the new get operation.

**********************
Allocation with claims
**********************

The functions below play a key role in allocating memory for domains.

 .. c:function:: int xc_domain_populate_physmap(xch, domid, extents, order, \
                                                mem_flags, extent_start)

    :param xch: The :term:`libxenctrl` interface
    :param domid: The ID of the domain
    :param extents: Number of extents
    :param order: Order of the extents
    :param mem_flags: Allocation flags
    :param extent_start: Array of guest frame numbers identifying the bases
                         of the extents to populate
    :type xch: xc_interface *
    :type domid: uint32_t
    :type extents: unsigned long
    :type order: unsigned int
    :type mem_flags: unsigned int
    :type extent_start: xen_pfn_t *
    :returns: The number of extents populated, possibly fewer than requested,
              or -1 with errno set on failure.

    For ordinary population of a non-translated domain, completed entries
    are replaced with machine frame numbers. With translated paging, Xen
    installs the mappings without overwriting the input guest frame numbers.
    Populate-on-demand records placeholders rather than returning newly
    allocated machine frames. Do not interpret every successful result as
    an array of host MFNs.

    This function is a wrapper for the ``XENMEM_populate_physmap`` hypercall,
    which is handled by the :c:func:`populate_physmap()` function in the
    hypervisor. It is used by :term:`libxenguest` for populating the
    :term:`guest physical memory` of a domain. :term:`domain builders` can
    set the :term:`NUMA node affinity` and pass the preferred node to this
    function to steer allocations towards the preferred NUMA node(s) and let
    :term:`claims` reserve the page counts against competing allocations during
    :term:`parallel domain builds`. Successful population still depends on
    suitable extents, address constraints, and other resources; the wrapper
    can return a short count.

The :term:`meminit` API calls :c:func:`xc_domain_populate_physmap()`
for populating the :term:`guest physical memory`. It invokes the restartable
``XENMEM_populate_physmap`` hypercall implemented by
:c:func:`populate_physmap()`.

.. c:function:: void populate_physmap(struct memop_args *a)

    :param a: Provides status and hypercall restart info
    :type a: struct memop_args *

    Allocates memory for building a domain and uses it for populating the
    :term:`physmap`. For allocation, it uses
    :c:func:`alloc_domheap_pages()`, which forwards the request to
    :c:func:`alloc_heap_pages()`.

    On the ordinary domain-heap path during creation, it adds
    :c:macro:`MEMF_no_scrub` and ``MEMF_keep_scrub`` to the request.
    This permits allocation of unscrubbed pages while retaining their
    per-page scrub markers and deferring assignment to the domain.

    When that happens, it uses deferred scrubbing and scrubs the pages
    as needed using hypercall continuation to avoid long hypercall
    latency and watchdog timeouts. Only after scrubbing does it assign the
    pages to the domain. A preempted scrub can therefore retain allocated,
    already-redeemed pages that are not yet counted by ``domain_tot_pages()``.

    A builder can improve scrubbing locality by running the population call
    on a CPU local to the memory being populated. CPU pinning alone does not
    force allocations onto that node; allocation flags, affinity, and fallback
    still determine the actual memory node. Deferred scrubbing here is not
    the same operation as populate-on-demand.

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
    Under :c:var:`heap_lock`, it checks whether claimed memory leaves enough
    memory for the request, then searches for a suitable block. On success it
    updates the free-page counters and redeems applicable claims.

    Its claims-related sequence is:

    1. Take :c:var:`heap_lock` and perform global admission. If unclaimed
       pages do not cover the whole request, only a ref-counted request for
       a domain whose outstanding claims cover the whole request may proceed.
       The check does not combine a partial claim with unclaimed pages.
    2. Find a buddy using :c:func:`get_free_buddy()`, retrying with unscrubbed
       pages permitted when appropriate. Split the buddy to the requested order.
    3. Decrement zone, node, and global free-page counters.
    4. For a domain with outstanding claims and without ``MEMF_no_refcount``,
       redeem ``min(d->outstanding_pages, request)`` pages in actual-node,
       host-wide, then other-node order.
    5. Complete page-state updates and drop the heap lock. Required TLB
       flushing and scrubbing happen afterwards. Assignment to the domain
       happens later in the caller, not inside this critical section.

.. c:function:: struct page_info *get_free_buddy(unsigned int zone_lo, \
                                                 unsigned int zone_hi, \
                                                 unsigned int order, \
                                                 unsigned int memflags, \
                                                 const struct domain *d)

    :param zone_lo: The lowest zone index to consider for allocation
    :param zone_hi: The highest zone index to consider for allocation
    :param order: The order of the pages to allocate (2^order pages)
    :param memflags: Flags for conducting the allocation
    :param d: The domain to allocate memory for or NULL
    :type zone_lo: unsigned int
    :type zone_hi: unsigned int
    :type order: unsigned int
    :type memflags: unsigned int
    :type d: const struct domain *
    :returns: The allocated page_info structure, or NULL on failure

    This function finds a suitable block of free pages in the buddy
    allocator while respecting claims and node-level available memory.

    Called by :c:func:`alloc_heap_pages()` after verifying the request is
    permissible, it iterates over nodes and zones to find a buddy block
    that satisfies the request. It checks node-local claims before
    attempting allocation from a node.

    For each candidate node, it checks whether unclaimed memory and the
    domain's claim on that node can satisfy the request. The per-node
    availability calculation is clamped at zero if claims exceed available
    memory after offlining.

    If the node can satisfy the request, it searches for a suitable block
    in the specified zones. If found, it returns the block; otherwise it
    tries another node while the normal affinity and exact-node policy permit.

    Own-node credit is added only for a domain's ref-counted request with an
    attached claims array. Anonymous and ``MEMF_no_refcount`` requests receive
    no credit. A host-wide claim does not provide node-specific credit.

    Claims do not override zone limits, buddy fragmentation, scrub eligibility,
    or exact-node constraints. With a specific requested node and
    ``MEMF_exact_node``, failure on that node ends the search. Without a
    specific node, exact-node flags restrict fallback to the preferred mask
    rather than selecting an arbitrary single node.

**************************************
Offlining memory in presence of claims
**************************************

Offlining free heap pages reduces :c:var:`total_avail_pages` and
:c:expr:`node_avail_pages[node]` without changing outstanding claims. A page
still in use is instead marked for offlining; when freed, it is excluded
from the available heap. Claims can therefore exceed available memory after
offlining, and the original reservation guarantee no longer holds. The
allocator clamps per-node availability to zero when subtracting
:c:var:`node_claimed_pages`; it does not recall claims from domains.

.. c:function:: int reserve_offlined_page(struct page_info *head)

    :param head: The head of a free buddy block containing offlined pages
    :type head: struct page_info *
    :returns: The number of pages newly removed from availability.

    The caller holds :c:var:`heap_lock`. The function removes the containing
    buddy block, returns its unaffected portions to the free lists, and
    excludes offlined pages from the available-page counters. The head need
    not itself be an offlined page. Outstanding claims are not changed; the
    per-node availability check clamps its subtraction, but cannot restore
    the lost capacity.
