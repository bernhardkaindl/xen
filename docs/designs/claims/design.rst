.. SPDX-License-Identifier: CC-BY-4.0

#############
Claims Design
#############

.. contents:: Table of Contents
    :backlinks: entry
    :local:

Xen's page allocator supports a :term:`claims` API that allows privileged
:term:`domain builders` to reserve an amount of available memory before
:term:`populating` the :term:`physmap` of :term:`domains`.

These reservations are called :term:`claims`. They ensure that the claimed
memory remains available for the :term:`domains` when allocating it, even
if other :term:`domains` are allocating memory at the same time.

:term:`Installing claims` is a privileged operation performed by
:term:`domain builders` before the guest :term:`physmap` is populated.
This prevents other :term:`domains` from allocating memory earmarked
for :term:`domains` under construction. Xen maintains the per-domain
claim state for pages that are claimed but not yet allocated.

When claim installation succeeds, Xen updates the claim state to reflect
the new targets and protects the claimed memory until it is allocated or
the claim is released. As Xen allocates pages for the domain, claims are
consumed by reducing the claim state by the size of each allocation.

***************
Design overview
***************

The legacy :ref:`XENMEM_claim_pages` hypercall is superseded by a new
:c:expr:`XEN_DOMCTL_claim_memory` hypercall that supports installing a
:term:`claim set`, which is an array of :c:expr:`memory_claim_t` entries
that specify claims on multiple :term:`NUMA nodes` and/or :term:`global claims`
that can be satisfied from any node.

Claim sets are validated and installed under :c:expr:`domain.page_alloc_lock`
and :c:expr:`heap_lock`: either the entire set is accepted, or the request fails
with no side effects.  New claims replace any existing claims for the
domain rather than accumulating, because page exchanges make incremental
per-node tracking of already-allocated pages impractical.

********************
Key design decisions
********************

.. glossary::

 :c:expr:`node_outstanding_claims[MAX_NUMNODES]`
  Tracks the sum of all claims on a node. :c:expr:`get_free_buddy()` checks
  it before scanning zones on a node, so claimed memory is protected from
  other allocations.

 :c:expr:`consume_allocation()`
  Retires claims in order: First, it retires claims from the allocation node's
  claim. If this is not sufficient, it retires claims from the global claim as
  a fallback. This allows the global claim to be used as a flexible fallback
  for claiming allocations on any node. Finally, remaining claims are retired
  from other nodes to prevent the increase of :c:expr:`domain_tot_pages(domain)`
  caused by the allocation on top of
  :c:expr:`domain.global_claims` and :c:expr:`domain.node_claims` to exceed
  :c:expr:`domain.max_pages`.

 :c:expr:`domain.global_claims` (formerly :c:expr:`domain.outstanding_claims`)
  Support for :term:`global claims` is maintained for two reasons: Firstly,
  for compatibility with existing domain builders, and secondly, for use cases
  where a flexible claim that can be satisfied from any node is desirable.

  When the preferred NUMA node(s) for a domain do not have sufficient free
  memory to satisfy the domain's memory requirements, global claims provide
  flexible fallback for the memory shortfall from the preferred node(s) that
  can be satisfied from any available node.

  In this case, :term:`domain builders` can exploit a combination of passing
  the preferred node to :c:expr:`xc_populate_physmap()` and
  :term:`NUMA node affinity` to steer allocations towards the preferred NUMA
  node(s) and letting the global claim ensure that the shortfall is available.

  This allows the domain builder to define a set of desired NUMA nodes to
  allocate from and even specify which nodes to prefer for an allocation,
  but the claim for the shortfall is flexible, not specific to any node.

**********
Claim sets
**********

Claim sets extend the claims API to support installing claims on multiple
NUMA nodes atomically. They may optionally include a global claim (memory
that can come from any node).

Legacy domain builders can continue to use the previous (now deprecated)
interface with its legacy semantics without changes. New domain builders
can take advantage of claim sets to install NUMA-aware claims.

*****
Goals
*****

The design's primary goals are:

1. Allow :term:`domain builders` to claim memory
   on multiple :term:`NUMA nodes` using a :term:`claim set` atomically.

2. Preserve the existing :c:expr:`XENMEM_claim_pages` hypercall command
   for compatibility with existing :term:`domain builders` with its legacy
   semantics, while introducing a new, unrestricted hypercall command for
   new use cases such as NUMA-aware claim sets.

3. Use fast allocation-time claims protection in the allocator's hot paths.

*********
Non-goals
*********

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

Stability and extensibility of the hypercall
============================================

While the claim sets hypercall is designed to be flexible and powerful
for privileged domain builders, it is not intended to be a stable interface
for general use.

The users of this hypercall are privileged domain builders that use the
unstable ``domctl`` hypercall interface via the ``libxenctrl`` library.
This is an interface that is only provided for privileged domain builders
and is not exposed to other domains.

Being defined as a stable interface like ``XEN_DOMCTL_get_domain_state``
is not a goal because the latter is only exported to ``libxenmanage``
for ``xenstored`` without using ``libxenctrl`` at all, as it aims to
work independently of the hypervisor version.

Such stability is not required for the claim sets hypercall, as it is only
used by privileged domain builders that can be updated together with the
hypervisor to use the new API, and it is not intended for general use by
other domains or tools.

The extra space included in the claim sets hypercall allows for adding new
features or parameters to the claim sets API in the future without breaking
compatibility with existing domain builders that use the current version of
the API, but it does not imply that the API itself is intended to be stable
for general use. It could change in the future if needed to support new
features or requirements.

********************
Life-cycle of claims
********************

A claim can be released by the domain builder at any time, but domain builders
are expected to release claims after completing the domain build. Examples:

- Domain builders call claims installation with
  :c:expr:`xen_memory_claim.pages = 0` to release claims.
- ``libxenguest``'s ``meminit`` API releases any remaining claims after
  populating memory.
- Xen releases remaining claims itself when it destroys a domain.

******************
Historical context
******************

.. glossary::

 Initial implementation (v1)

  The `implementation of single-node claims <v1_>`_ by Alejandro Vallejo
  introduced "node-exact" claims, allowing :term:`domain builders` to
  claim memory on one :term:`NUMA node`.
  It laid the groundwork for future extensions to support multi-node claim
  sets, but it did not include support for claiming memory on multiple nodes.

  It passed the NUMA node in the :c:expr:`xen_memory_reservation.mem_flags`
  field of :ref:`XENMEM_claim_pages` and set it as the target of the claim.
  It also explored protecting claimed memory in the buddy allocator, but
  wasn't successful in protecting it if memory was tight.

  Feedback was to `extend the API to support multi-node claim sets <v1mul_>`_:
   - Introduce a new hypercall to claim on multiple nodes atomically
   - The infrastructure for only designating a single node for claims
     could possibly be retained initially, but at least in the long term,
     the infrastructure should be extended to support multi-node claim sets.

 Previous implementation of single-node claims (v4)

  The latest `v4 submission <v4_>`_ was still single-node and received
  multiple `suggestions to replace it with multi-node claim sets <v4-03_>`_,
  which led to the current design of claim sets that support multi-node claims.

.. _v1:
   https://patchew.org/Xen/20250314172502.53498-1-alejandro.vallejo@cloud.com/
.. _v1mul:
   https://lists.xenproject.org/archives/html/xen-devel/2025-06/msg00484.html
.. _v4:
    https://lists.xenproject.org/archives/html/xen-devel/2026-02/msg01387.html
.. _v4-03: https://patchwork.kernel.org/project/xen-devel/
   patch/6927e45bf7c2ce56b8849c16a2024edb86034358.1772098423
   .git.bernhard.kaindl@citrix.com/

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
  NUMA nodes.

 claim set installation
 installing claim sets
 installing claims
  The process of validating and installing a claim set for a domain under
  :c:expr:`domain.page_alloc_lock` and :c:expr:`heap_lock`, ensuring that
  either the entire set is accepted and installed, or the request fails with
  no side effects.

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
  UMA machines or as a fallback for memory that come available on any node.

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