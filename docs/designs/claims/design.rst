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
:term:`domain builders` to reserve an amount of available memory before
:term:`populating` the :term:`guest physical memory` of new :term:`domains`
they are creating, configuring and building.

These reservations are called :term:`claims`. They ensure that the claimed
memory remains available for the :term:`domains` when allocating it, even
if other :term:`domains` are allocating memory at the same time.

:term:`Installing claims` is a privileged operation performed by
:term:`domain builders` before they populate the :term:`guest physical memory`.
This prevents other :term:`domains` from allocating memory earmarked
for :term:`domains` under construction. Xen maintains the per-domain
claim state for pages that are claimed but not yet allocated.

When claim installation succeeds, Xen updates the claim state to reflect
the new targets and protects the claimed memory until it is allocated or
the claim is released. As Xen allocates pages for the domain, claims are
redeemed by reducing the claim state by the size of each allocation.

************
Design Goals
************

The design's primary goals are:

1. Allow :term:`domain builders` to claim memory
   on multiple :term:`NUMA nodes` using a :term:`claim set` atomically.

2. Preserve the existing :c:expr:`XENMEM_claim_pages` hypercall command
   for compatibility with existing :term:`domain builders` and its legacy
   semantics, while introducing a new, unrestricted hypercall command for
   new use cases such as NUMA-aware claim sets.

3. Global claims are supported for compatibility with existing domain builders
   and for use cases where a flexible claim that can be satisfied from any node
   is desirable, such as on UMA machines or as a fallback for memory that comes
   available on any node. This means we cannot remove or replace the legacy
   global claim call nor the needed variables maintaining the global claim
   state. They are still very much needed: claims are not just for NUMA use
   cases, but for :term:`parallel domain builds` in general.

   Only on UMA machines is a global claim the same as a claim on node 0,
   but the same is not true for NUMA machines, where global claims can claim
   more memory than any single node, and the global claim can be used as a
   flexible fallback for claiming memory on any node, which can be useful
   when preferred NUMA node(s) should be claimed, but may have insufficient
   free memory at the time of claim installation, and the global claim can
   ensure that the shortfall is available from any node.

4. Use fast allocation-time claims protection in the allocator's hot paths
   to protect claimed memory from parallel allocations from other domain
   builders in case of parallel domain builds, and to protect claimed
   memory from allocations from already running domains.

***************
Design Overview
***************

The legacy :ref:`XENMEM_claim_pages` hypercall is superseded by
:c:expr:`XEN_DOMCTL_claim_memory`. This hypercall installs a :term:`claim set`.
It is an array of :c:expr:`memory_claim_t` entries, where each entry specifies
a page count and a target: either a specific NUMA node ID or a special selector
(for example, a global or flexible claim).

Like legacy claims, claim sets are validated and installed under
:c:expr:`domain.page_alloc_lock` and :c:expr:`heap_lock`: Either the entire
set is accepted, or the request fails with no side effects.  Repeated calls
to install claims replace any existing claims for the domain rather than
accumulating.

As installing claim sets after allocations is not a supported use case,
the legacy behaviour of subtracting existing allocations from installed
claims is somewhat surprising and counterintuitive, and page exchanges
make incremental per-node tracking of already-allocated pages on a per-node
basis difficult. Therefore, claim sets do not retain the legacy behaviour of
subtracting existing allocations, optionally on a per-node basis, from the
installed claims across the individual claim set entries.

Summary:

- Legacy domain builders can continue to use the previous (now deprecated)
  :c:expr:`XENMEM_claim_pages` hypercall command to install single-node claims
  with the legacy semantics and, aside from improvements or fixes to global
  claims in general, observe no changes in their behaviour.
- Updated domain builders can take advantage of claim sets to install
  NUMA-aware :term:`claims` on multiple :term:`NUMA nodes` and/or globally
  in a single step.

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

 :c:expr:`node_outstanding_claims[MAX_NUMNODES]`
  Tracks the sum of all claims on a node. :c:expr:`get_free_buddy()` checks
  it before scanning zones on a node, so claimed memory is protected from
  other allocations.

 :c:expr:`redeem_claims_for_allocation()`
   When allocating memory for a domain, the page allocator redeems the
   matching claims for this allocation, ensuring the domain's total memory
   allocation as :c:expr:`domain_tot_pages(domain)` plus its outstanding claims
   as :c:expr:`domain.global_claims + domain.node_claims` remain within the
   domain's limits, defined by :c:expr:`domain.max_pages`.
   See :doc:`redeeming` for details on redeeming claims.

 :c:expr:`domain.global_claims` (formerly :c:expr:`domain.outstanding_claims`)
  Support for :term:`global claims` is maintained for two reasons: first,
  for compatibility with existing domain builders, and second, for use cases
  where a flexible claim that can be satisfied from any node is desirable.

  When the preferred NUMA node(s) for a domain do not have sufficient free
  memory to satisfy the domain's memory requirements, global claims provide
  a flexible fallback for the memory shortfall from the preferred node(s) that
  can be satisfied from any available node.

  In this case, :term:`domain builders` can exploit a combination of passing
  the preferred node to :c:expr:`xc_domain_populate_physmap()` and
  :term:`NUMA node affinity` to steer allocations towards the preferred NUMA
  node(s), while letting the global claim ensure that the shortfall is
  available.

  This allows the domain builder to define a set of desired NUMA nodes to
  allocate from and even specify which nodes to prefer for an allocation,
  but the claim for the shortfall is flexible, not specific to any node.

*********
Non-goals
*********

Using per-node allocator data
=============================

Some data structures could be moved into the per-node allocator data
allocated by `init_node_heap()`, to avoid bouncing those data structures
between nodes, but that would not eliminate the need to take the global
:c:expr:`heap_lock`, which is still needed to protect the allocator's
internal state during allocation and deallocation.

The synchronisation point for taking the global :c:expr:`heap_lock` is
the main point of contention during allocation, freeing and scrubbing
pages. The overhead of accessing the per-node claims accounting data
is expected to be minimal.

However, we aim move that data into the per-node allocator data in the
future to reduce the need to bounce those data structures between nodes.

Legacy behaviours
=================

Installing claims is a privileged operation performed by domain builders
before they populate guest memory. As such, tracking previous allocations
is not in scope for claims.

For the following reasons, claim sets do not retain the legacy behaviour
of subtracting existing allocations from installed claims:

- Xen does not currently maintain a ``d->node_tot_pages[node]`` count,
  and the hypercall to exchange extents of memory with new memory makes
  such accounting relatively complicated.

- The legacy behaviour is somewhat surprising and counterintuitive.
  Because installing claims after allocations is not a supported use case,
  subtracting existing allocations at installation time is unnecessary.

- Claim sets are a new API and can provide more intuitive semantics
  without subtracting existing allocations from installed claims. This
  also simplifies the implementation and makes it easier to maintain.

Versioned hypercall
===================

The :term:`domain builders` using the :c:expr:`XEN_DOMCTL_claim_memory`
hypercall also need to use other version-controlled hypercalls which
are wrapped through the :term:`libxenctrl` library.

Wrapping this call in :term:`libxenctrl` is therefore a practical approach;
otherwise, we would have a mix of version-controlled and unversioned hypercalls,
which could be confusing for API users and for future maintenance. From the
domain builders' viewpoint, it is more consistent to expose the claims
hypercall in the same way as the other calls they use.

Stable interfaces also have drawbacks: with stable syscalls, Linux needs
to maintain the old interface indefinitely, which can be a maintenance burden
and can limit the ability to make improvements or changes to the interface
in the future. Linux carries many system call successor families, e.g., oldstat,
stat, newstat, stat64, fstatat, statx, with similar examples including openat,
openat2, clone3, dup3, waitid, mmap2, epoll_create1, pselect6 and many more.
Glibc hides that complexity from users by providing a consistent API, but it
still needs to maintain the old system calls for compatibility.

In contrast, versioned hypercalls allow for more flexibility and evolution of
the API while still providing a clear path to adopt new features. The reserved
fields and reserved bits in the structures of this hypercall allow for many
future extensions without breaking existing callers.

********
Glossary
********

.. glossary::

 claims
  Reservations of memory for :term:`domains` that are installed by
  :term:`domain builders` before :term:`populating` the domain's memory.
  Claims ensure that the reserved memory remains available for the
  :term:`domains` when allocating it, even if other :term:`domains` are
  allocating memory at the same time.

 claim set
  An array of :c:expr:`memory_claim_t` entries, each specifying a page count
  and a target (either a NUMA node ID or a special value for global claims),
  that can be installed atomically for a domain to reserve memory on multiple
  NUMA nodes. The chapter on :ref:`designs/claims/installation:claim sets`
  provides further information on the structure and semantics of claim sets.

 claim set installation
 installing claim sets
 installing claims
  The process of validating and installing a claim set for a domain under
  :c:expr:`domain.page_alloc_lock` and :c:expr:`heap_lock`, ensuring that
  either the entire set is accepted and installed, or the request fails with
  no side effects.
  The chapter on :ref:`designs/claims/installation:claim set installation`
  provides further information on the structure and semantics of claim sets.

 domain builders
  Privileged entities (such as :term:`toolstacks` in management :term:`domains`)
  responsible for constructing and configuring :term:`domains`, including
  installing :term:`claims`, :term:`populating` memory, and setting up other
  resources before the :term:`domains` are started.

 domains
  Virtual machine instances managed by Xen, built by :term:`domain builders`.

 global claims
  :term:`claims` that can be satisfied from any NUMA node, required for
  compatibility with existing domain builders and for use cases where
  strict node-local placement is not required or not possible, such as on
  UMA machines or as a fallback for memory that comes available on any node.

 libxenctrl
  A library used by :term:`domain builders` running in privileged
  :term:`domains` to interact with the hypervisor, including making
  hypercalls to install claims and populate memory.

 libxenguest
  A library used by :term:`domain builders` running in privileged
  :term:`domains` to interact with the hypervisor, including making
  hypercalls to install claims and populate memory.

 meminit
  The phase of a domain build where the guest's physical memory is populated,
  which involves allocating and mapping physical memory for the domain's guest
  :term:`physmap`. This should be performed after installing :term:`claims`
  to protect the process against parallel allocations of other domain builder
  processes in case of parallel domain builds.

  It is implemented in :term:`libxenguest` and optionally installs
  :term:`claims` to ensure the claimed memory is reserved before populating
  the :term:`physmap` using calls to :c:expr:`xc_domain_populate_physmap()`.

 nodemask
  A bitmap representing a set of NUMA nodes, used for status information
  like :c:expr:`node_online_map` and the :c:expr:`domain.node_affinity`.

 node
 NUMA node
 NUMA nodes
  A grouping of CPUs and memory in a NUMA architecture. NUMA nodes have
  varying access latencies to memory, and NUMA-aware claims allow
  :term:`domain builders` to reserve memory on specific NUMA nodes
  for performance reasons. Platform firmware configures what constitutes
  a NUMA node, and Xen relies on that configuration for NUMA-related features.

  When this design refers to NUMA nodes, it is referring to the NUMA nodes
  as defined by the platform firmware and exposed to Xen, initialized at boot
  time and not changing at runtime (so far).

  The NUMA node ID is a numeric identifier for a NUMA node, used whenever code
  specifies a NUMA node, such as the target of a claim or indexing into arrays
  related to NUMA nodes.

  NUMA node IDs start at 0 and are less than :c:expr:`MAX_NUMNODES`.

  Some NUMA nodes may be offline, and the :c:expr:`node_online_map` is used
  to track which nodes are online. Currently, Xen does not support hotplug
  of NUMA nodes, so the set of online NUMA nodes is determined at boot time
  based on the platform firmware configuration and does not change at runtime.

 NUMA node affinity
  The preference of a :term:`domain` for a set of NUMA nodes, which can be used
  by :term:`domain builders` to guide memory allocation even when not forcing
  the buddy allocator to only consider (or prefer) a specific node when
  allocating memory, but even a set of preferred NUMA nodes.

  By default, domains have NUMA node auto-affinity, which means their NUMA
  node affinity is determined automatically by the hypervisor based on the
  CPU affinity of their vCPUs, but it can be disabled and configured.

 guest physical memory
 physmap
  The mapping of a domain's guest physical memory to the host's
  machine address space. The :term:`physmap` defines how the guest's
  physical memory corresponds to the actual memory locations on the host.

 populating
  The process of allocating and mapping physical memory for a domain's guest
  :term:`physmap`, performed by the :term:`domain builders`, preferably after
  installing :term:`claims` to protect the process against parallel allocations
  of other domain builder processes in case of parallel domain builds.

 toolstacks
  Privileged entities (running in privileged :term:`domains`) responsible for
  managing :term:`domains`, including building, configuring, and controlling
  their lifecycle using :term:`domain builders`. One toolstack may run
  multiple :term:`domain builders` in parallel to build multiple :term:`domains`
  at the same time.
