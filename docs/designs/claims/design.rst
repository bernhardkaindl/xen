.. SPDX-License-Identifier: CC-BY-4.0

#############
Claims Design
#############

.. contents:: Table of Contents
    :backlinks: entry
    :local:

************
Introduction
************

Xen's page allocator supports a :term:`claims` API that allows privileged
:term:`domain builders` to reserve a quantity of available memory before
:term:`populating` the :term:`guest physical memory` of new :term:`domains`
they are creating, configuring, and building.

These reservations are called :term:`claims`. They ensure that the claimed
memory remains available for the :term:`domains` when allocating it, even if
other :term:`domains` are allocating memory at the same time, provided memory
offlining has not removed the capacity backing the claims. Claims reserve page
counts, not contiguous extents, particular address zones, or a placement policy.

:term:`Installing claims` is a privileged operation performed by
:term:`domain builders` before they populate the :term:`guest physical memory`.
This prevents other :term:`domains` from allocating memory earmarked for
:term:`domains` under construction. Xen maintains the per-domain claim state
for pages that are claimed but not yet allocated.

When claim installation succeeds, Xen updates the claim state to reflect the
new targets and protects the claimed memory until it is allocated or the claim
is released. For ref-counted domain-heap allocations, Xen redeems the smaller
of the allocation size and the domain's outstanding total, even when unclaimed
memory would have been sufficient to admit the allocation.

************
Design Goals
************

The design's primary goals are:

1. Allow :term:`domain builders` to claim memory
   on multiple :term:`NUMA nodes` using a :term:`claim request` atomically.

2. Preserve the existing :c:macro:`XENMEM_claim_pages` hypercall command
   for compatibility with existing :term:`domain builders` and its legacy
   semantics, while introducing set/get domctls for new use cases such as
   NUMA-aware claim sets.

3. Host-wide claims are supported for compatibility with existing
   :term:`domain builders` and for use cases where a flexible claim that
   can exist at the host level is desirable.

   The global outstanding count includes both host-wide and per-node claims.
   It bounds total reservations even when no individual node has enough
   information to account for the host-wide portion.

4. Use fast allocation-time claims protection in the allocator's hot paths
   to protect claimed memory from parallel allocations by other domain
   builders in case of parallel domain builds, and to protect claimed
   memory from all other allocations as well.

***************
Design Overview
***************

The legacy :c:macro:`XENMEM_claim_pages` hypercall remains available
for compatibility. The new :c:macro:`XEN_DOMCTL_set_memory_claims`
domctl installs a :term:`claim request`.
It is an array of :c:type:`xen_domctl_memory_claim_t` entries,
where each entry specifies a page count and a target: either
a specific NUMA node ID or the host-wide selector.

Claim sets are validated and installed under :c:member:`domain.page_alloc_lock`
and :c:var:`heap_lock`: Either the entire set is accepted, or the request fails
without changing the claims. A non-empty set can only be installed during
domain creation and when the domain has no outstanding claims. An empty set
releases outstanding claims, including after domain creation has finished.

Each entry specifies additional outstanding pages, not a final allocation
target. Existing allocations are permitted before creation finishes, but their
total plus the requested claims must fit within the domain's maximum page
count. Unlike the legacy interface, Xen does not subtract existing allocations
from the requested page counts. The builder must serialize installation with
allocations for that domain, including completion of allocation continuations.

Legacy domain builders can continue to use :c:macro:`XENMEM_claim_pages`.
Updated builders can install a single set of host-wide and per-node claims
without attributing existing allocations to individual claim targets. This
does not permit page exchanges or other allocation operations to race with
claim installation for the same domain.

For readers following the design in order, the next sections cover the
following topics:

1. :doc:`/designs/claims/installation` explains how claim sets are installed.
2. :doc:`/designs/claims/protection` describes how claimed memory is
   protected during allocation.
3. :doc:`/designs/claims/redeeming` explains how claims are redeemed as
   allocations succeed.
4. :doc:`/designs/claims/accounting` describes the accounting model that
   underpins those steps.

********************
Key design decisions
********************

.. glossary::

 :c:expr:`node_claimed_pages[MAX_NUMNODES]`
  Tracks the sum of all claims on a node. :c:func:`get_free_buddy()` checks
  it before scanning zones on a node, so claimed memory is protected from
  other allocations.

 :c:expr:`domain_redeem_node_claim()`
   Redeems up to the requested number of pages from one node's claim and
   updates the domain, node, and host claim counters together. It is the first
   step of the allocator's three-step redemption sequence, not a standalone
   check of :c:member:`domain.max_pages`. See :doc:`redeeming` for the full
   sequence and its domain-limit qualifications.

 :c:expr:`domain.outstanding_pages`
   It remains the authoritative source for the total outstanding claims of a
   domain, and is updated on claim installation, redemption, and release. It
   includes both host-wide claims and per-node claims. The per-node portions
   are also tracked by :c:member:`domain.node_claims` and
   :c:member:`domain.claims`.
   Support for :term:`host-wide claims` is maintained for two reasons: first,
   for compatibility with existing domain builders, and second, for use cases
   where a flexible claim that can be satisfied from any node is desirable.

   When the preferred NUMA node(s) for a domain do not have sufficient free
   memory to satisfy the domain's memory requirements, host-wide claims provide
   a flexible fallback for the memory shortfall from the preferred node(s) that
   can be satisfied from any available node.

   In this case, :term:`domain builders` can use a combination of passing
   the preferred node to :c:func:`xc_domain_populate_physmap()` and
   :term:`NUMA node affinity` to steer allocations towards the preferred
   NUMA node(s), while letting host-wide claims ensure that the shortfall
   is available.

   This allows the domain builder to define a set of desired NUMA nodes to
   allocate from and even specify which nodes to prefer for an allocation,
   but the claim for the shortfall is flexible, not specific to any node.

*********
Non-goals
*********

Using per-node allocator data
=============================

Some data structures could be moved into the per-node allocator data
allocated by `init_node_heap()` to avoid bouncing those data structures
between nodes. Those can be moved to the per-node allocator data in the
future, but that is not a priority. While that would reduce this bouncing,
it would not eliminate the need to take the global :c:var:`heap_lock`,
which is still needed to protect the allocator's state during allocation
and freeing of pages.

The global :c:var:`heap_lock` remains a shared synchronisation point for
allocation, freeing, and the heap metadata used by background scrubbing.
Scrubbing page contents happens outside the lock. The relative contention
cost and the overhead of per-node claims accounting have not been isolated
by the product observations in :doc:`performance`.

Removing :c:var:`heap_lock` would require a separate allocator design that
synchronizes buddy lists, free-page counters, and claim admission and redemption.
An atomic unclaimed-page counter alone would not make those operations atomic
with each other. This series neither changes that synchronization nor chooses
a replacement locking scheme.

However, we aim to move that data into the per-node allocator data in the
future to reduce the need to bounce those data structures between nodes.

Legacy behaviours
=================

Installing claims is a privileged operation performed by domain builders
during domain creation. Existing allocations count towards the domain limit,
but do not reduce the outstanding page counts requested by the new interface.

For the following reasons, claim sets do not retain the legacy behaviour
of subtracting existing allocations from installed claims:

- Xen does not currently maintain a ``d->node_tot_pages[node]`` count,
  and the hypercall to exchange extents of memory with new memory makes
  such accounting relatively complicated.

- Existing allocations need not be attributed to individual claim targets.

- Claim sets are a new API and can provide more intuitive semantics
  without subtracting existing allocations from installed claims. This
  also simplifies the implementation and makes it easier to maintain.

Versioned domctl interface
==========================

``XEN_DOMCTL_set_memory_claims`` and ``XEN_DOMCTL_get_memory_claims`` are
commands of the existing domctl hypercall, not new hypercall numbers. They
use ``XEN_DOMCTL_INTERFACE_VERSION``. The libxenctrl wrappers supply that
version and handle the claim-array bounce buffers for their callers.

The versioning rules in the public domctl header apply: incompatible changes
can require a version bump, whereas pure additions and compatible extensions
do not. Adding these two commands therefore does not itself require a bump.
Zero-checked padding and reserved target values leave room for compatible
extensions, but do not make the interface unversioned.

The legacy ``XENMEM_claim_pages`` memory operation remains available with its
existing ABI. Adding the domctl wrappers neither removes that operation nor
eliminates the distinction between memory operations and versioned domctls.

*****************
Future extensions
*****************

Per-node availability reporting is separate from this series.
``XEN_SYSCTL_numainfo`` reports free heap memory, including memory reserved by
claims; it does not expose ``node_claimed_pages[]`` or per-node unclaimed
capacity. ``XEN_DOMCTL_get_memory_claims`` reports one domain's reservations,
not the host's remaining capacity. A builder must handle admission failure
even after selecting nodes from a free-memory query: only installation makes
the reservation atomic, and a prior capacity query cannot prevent races.

The reserved fields and bits in the structures of this
hypercall allow for many future extensions without breaking existing callers.

Current claims are measured in base pages and already admit and redeem
high-order allocations where a suitable buddy exists. They do not reserve
contiguous extents or guarantee a particular superpage size. A future
extent-aware reservation would require more than the current page counters.

Another possible extension is support for requests with
:c:macro:`MEMF_no_refcount`, such as P2M and HAP allocations. Those requests
currently receive no domain-claim credit and do not redeem claims.

See :ref:`designs/claims/protection:Callers using MEMF_no_refcount`
for more information.
