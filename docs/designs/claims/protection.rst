.. SPDX-License-Identifier: CC-BY-4.0

Protection
##########

.. contents:: Table of Contents
    :backlinks: entry
    :local:

Claimed memory must be protected from allocations without applicable claims
while remaining available to allocations with applicable claims.

Claims exist as long as they are outstanding, which is from the moment they
are installed until they are redeemed by allocations or explicitly released.
Domain destruction also releases outstanding claims.

While memory remains online, claims are a commitment of memory to a domain.
The allocator protects claimed pages from allocations that cannot redeem the
applicable claims. Offlining can reduce available memory below claims, so it
can invalidate the original reservation guarantee.

Redeeming claims is the process of applying a portion of the claims of
a domain to an allocation to allow the allocation to proceed by exchanging
the claim for the allocated memory, so that the allocation can use the
claimed memory and the portion of the claim used for the allocation is
no longer outstanding.

For example, if a domain has an outstanding claim of 100 pages on a node,
and it redeems 20 pages of that claim for an allocation, the domain would
have 80 pages of that claim still outstanding, and the allocation would be
satisfied using the claimed memory, so the domain can use that allocated
memory and the claim would be reduced by the redeemed amount.

For the protection of claims, the allocator performs checks to ensure that
claimed memory is not allocated without redeeming applicable claims, while
still allowing the claiming domain to allocate claimed memory by redeeming
claims.

When the system is not under heavy memory pressure and not fully-claimed,
the allocator can satisfy allocation requests using unclaimed memory.

However, when the system is under heavy memory pressure or nearly fully-claimed,
the checks for protecting claims become critical to ensure that claimed memory
is not allocated without redeeming applicable claims.

*********************************
Reference-counting of allocations
*********************************

Claims protection distinguishes between two kinds of allocation requests.
Here, "reference-counted" refers to charging the allocation to the domain's
normal page limit and redeeming its claims, not to whether the returned pages
have page references or an owner.

Reference-counted requests
==========================

This means that the request comes for a domain and the :c:expr:`memflags`
of the request do not include :c:expr:`MEMF_no_refcount`.

In this case, the request is reference-counted to the domain's
total memory allocation, and the domain's claims can be used
to protect and redeem the allocation using claims.

For example, the allocation requests by :term:`domain builders` for the
:term:`guest physical memory` normally use ref-counted domain-heap allocations,
which can redeem claims. Population modes that do not allocate new domain-heap
pages, such as populate-on-demand placeholders or direct-mapped memory, do not
necessarily follow that allocation and redemption path.

Not reference-counted requests
==============================

This means that the request is not for a domain, or the ``memflags``
of the request includes :c:macro:`MEMF_no_refcount`.

Such requests are not charged to the domain's normal page limit and receive
no credit from its claims. This does not necessarily make the pages unowned:
``alloc_domheap_pages()`` can assign them to the domain as ``PGC_extra`` pages.
They then contribute to both ``d->tot_pages`` and ``d->extra_pages``, leaving
``domain_tot_pages(d)`` unchanged, and still have ordinary page references.
``MEMF_no_owner`` separately suppresses ownership assignment and also implies
``MEMF_no_refcount`` on this allocation path.

Such requests receive no credit from a domain's claims and do not redeem
them. The allocator still checks global and per-node claims to protect
other reservations, so these requests can use only unclaimed memory.

Callers using MEMF_no_refcount
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Example callers which use :c:macro:`MEMF_no_refcount` when allocating memory
or use :c:macro:`MEMF_no_owner` which also sets :c:macro:`MEMF_no_refcount`
in the context of domains include:

- ``p2m_alloc_page()`` for allocating pages for the page-to-machine mapping.
- ``hap_set_allocation()`` for allocating memory for hardware-assisted paging.
- ``vmx_alloc_vlapic_mapping()`` for allocating the vLAPIC page for a HVM guest.
- ``vmtrace_alloc_buffer()`` for allocating the buffer for VM tracing.
- ``ioreq_server_alloc_mfn()`` for allocating memory for I/O requests.

Example actions happening at runtime on the request of running domains
which use :c:macro:`MEMF_no_refcount` or :c:macro:`MEMF_no_owner` to
avoid charging replacement allocations as additional domain memory include:

- ``memory_exchange()`` for exchanging memory pages of a domain.
- ``gnttab_transfer()`` for transferring pages between domains.

***********************
Claim protection checks
***********************

The allocator performs two protective checks to protect claimed memory
from being allocated to other domains while still allowing the claiming
domain to allocate it.

Before starting, the allocator takes the global :c:var:`heap_lock`.
This ensures that any previous changes to the state of the system's
unclaimed memory and the domain's total outstanding claims are complete
and visible, and no concurrent changes to those values can happen.

Protection of host-wide claims
==============================

The first check [1]_ the allocator performs is a check protecting host-wide
claims which are part of the total pool of the claims on the entire host.

1. Get the total amount of unclaimed memory available in the system.
   It is the sum of the free pages on all NUMA nodes
   (:c:var:`total_avail_pages`) minus the total amount of claimed
   memory across all domains (:c:var:`outstanding_claims`). This
   includes all host-wide claims and all node-specific claims.

2. Check whether the request can be satisfied by the unclaimed memory itself.

   If so, the allocation calls :c:func:`get_free_buddy()` to perform the
   node-specific checks and find free pages on the appropriate node(s)
   to satisfy the request.

   This is the common case, especially for smaller allocations and when the
   host is not under heavy memory pressure and not fully-claimed.

If the request exceeds unclaimed memory, the fallback check protects claims.
This need not imply high physical memory usage: free pages may simply be
reserved by other builders, or a large contiguous request may exceed the
currently unclaimed count.

In these situations, the allocator needs to ensure that the domain has
enough claims to redeem the claimed memory to satisfy this request,
otherwise the request has to fail:

1. If the request is not for a domain or the request is disabling reference
   counting, the request fails.

2. If the total claims of the domain (:c:member:`domain.outstanding_pages`)
   cover the entire allocation request, not merely the shortfall
   after unclaimed pages are counted,
   the allocation can proceed further. Else, the request fails.

Protection of node-specific claims
==================================

This check protects claimed memory on the specific node from being allocated
without sufficient claims.

After passing the host-wide claims protection check, the allocator calls
:c:expr:`get_free_buddy()` to pick nodes for allocation and check the
node's suitability [2]_ for this request:

1. Get the number of unclaimed memory available on that node using the
   free pages on that NUMA node (``node_avail_pages[node]``) minus the
   total amount of claimed memory across all domains for that node
   (``node_claimed_pages[node]``). If offlining has reduced available memory
   below claims, the result is clamped to zero.

2. If the request can be satisfied by the sum of the unclaimed memory
   on that node and, for ref-counted domain requests only, the domain's claim
   on that node, the allocation
   can proceed on that node, else this node cannot satisfy this request.

3. If the allocation specifies a particular node with ``MEMF_exact_node``,
   or the allocator has no further permitted nodes to consider, it fails.
   Without a particular requested node, ``MEMF_exact_node`` prevents fallback
   outside the preferred node mask rather than restricting the search to one
   arbitrary node.

4. Else, if the allocator has to consider further nodes for this request,
   the allocator continues to repeat the same process for the next node.

.. rubric:: Footnotes

.. [1] A ref-counted exact-node request fully covered by the domain's claim
   on that node also satisfies the global claim-coverage fallback.
   Checking that first could avoid the global free-versus-claimed
   comparison, but successful allocations still update global free-page
   and claim counters under :c:var:`heap_lock`. Such a shortcut would not
   itself remove the lock or enable concurrent frees. This series keeps
   the common admission path; it does not implement or benchmark this
   alternative. Finer-grained locking requires a separate allocator design.

.. [2] A ref-counted request covered by the domain's claim on the candidate
   node could pass node-level claim admission without computing the
   unclaimed portion. It would still need a suitable buddy block and
   updates to node free-page and claim counters. No such fast path or
   measured performance benefit is part of this series.
