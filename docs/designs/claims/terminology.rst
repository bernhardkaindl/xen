.. SPDX-License-Identifier: CC-BY-4.0

Terminology
###########

.. Terms should appear in alphabetical order by their initial synonym.

.. glossary::

 claims
  Reservations of memory for :term:`domains` that are installed by
  :term:`domain builders` before :term:`populating` the domain's memory.
  While that memory remains online, claims protect the reservation against
  allocations by other :term:`domains`.

 claim set
  An array of :c:type:`xen_domctl_memory_claim_t` entries, each specifying a
  page count and a target (a NUMA node ID or the host-wide target constant).
  A non-empty set is installed atomically for a domain with no outstanding
  claims during domain creation. An empty set releases outstanding claims.
  See :ref:`designs/claims/installation:claim sets` for details.

 claim set installation
 installing claim sets
 installing claims
  The process of validating and installing a claim set under
  :c:member:`domain.page_alloc_lock` and :c:var:`heap_lock`, ensuring that
  either the entire set is installed or the request fails without changing
  the claims. Non-empty sets cannot replace existing claims.
  The chapter on
  :ref:`designs/claims/installation:NUMA-aware claims installation`
  provides further information on the structure and semantics of claim sets.

 domain builders
  Privileged entities (such as :term:`toolstacks` in management :term:`domains`)
  responsible for constructing and configuring :term:`domains`, including
  installing :term:`claims`, :term:`populating` memory, and setting up other
  resources before the :term:`domains` are started.

 host-wide claims
  :term:`claims` that can be satisfied from any NUMA node, required for
  compatibility with existing domain builders and for use cases where
  strict node-local placement is not required or not possible, such as on
  UMA machines or as a fallback for memory that comes available on any node.

 libxenctrl
  A low-level C API library to interact with the Xen hypervisor, to make
  :term:`hypercalls`. If hypercalls are to Xen what system calls are to the
  Linux kernel, then :term:`libxenctrl` is the universal, low-level system C
  runtime library that provides the interface for making those hypercalls.

 libxenguest
  A higher-level library, layered on top of :term:`libxenctrl`,
  specifically designed for :term:`domain builders` to build and
  configure :term:`domains`, including installing :term:`claims`
  and :term:`populating` :term:`guest physical memory`. It provides
  a more convenient and domain-builder-friendly interface for these
  operations, abstracting away details of creating the architecture-specific
  memory map expected by guest operating systems which were initially
  written to run on the bare metal (on full hardware) and not in a
  virtualized environment.

 meminit
  The phase of a domain build where the guest's physical memory is populated,
  which involves allocating and mapping physical memory for the domain's guest
  :term:`physmap`. This should be performed after installing :term:`claims`
  to protect the process against parallel allocations of other domain builder
  processes in case of parallel domain builds.

  It is implemented in :term:`libxenguest` and optionally installs
  :term:`claims` to ensure the claimed memory is reserved before populating
  the :term:`physmap` using calls to :c:func:`xc_domain_populate_physmap()`.

 nodemask
  A bitmap representing a set of NUMA nodes, used for status information
  like :c:var:`node_online_map` and the :c:member:`domain.node_affinity`.

 node
 NUMA node
 NUMA nodes
  A grouping of CPUs and memory in a NUMA architecture. NUMA nodes have
  varying access latencies to memory, and NUMA-aware claims allow
  :term:`domain builders` to reserve memory on specific NUMA nodes
  for performance reasons. This design uses Xen's host NUMA topology and
  node numbering, normally discovered from platform topology information.

  Claim targets are Xen host-node IDs, not guest vNUMA node IDs. Population
  requests using ``XENMEMF_vnode`` translate a guest node through the domain's
  configured virtual-to-host node mapping. The claims interface performs no
  such translation; builders must select host-node targets themselves.

  The NUMA node ID is a numeric identifier for a NUMA node, used whenever code
  specifies a NUMA node, such as the target of a claim or indexing into arrays
  related to NUMA nodes.

  NUMA node IDs start at 0 and are less than :c:macro:`MAX_NUMNODES`.

    Some NUMA nodes may be offline. The :c:var:`node_online_map` tracks which
    nodes are online, and claim installation only accepts online node targets.

 NUMA node affinity
  The preference of a :term:`domain` for a set of NUMA nodes, which can
  be set up by :term:`domain builders` to make :c:func:`get_free_buddy`
  (which selects the NUMA node to allocate from) prefer specific NUMA nodes for
  performance reasons.

  It is represented by the :c:member:`domain.node_affinity`, which is a
  bitmap of NUMA nodes indicating the preferred NUMA nodes for the domain.
  By default, domains have NUMA node auto-affinity, which means their NUMA
  node affinity is determined automatically by the hypervisor based on the
  CPU affinity of their vCPUs, but it can be disabled and configured manually
  by domain builders.

 guest physical memory
  The memory addressed through the domain's guest physical address space.
  Guest physical addresses need not equal host machine addresses; the
  :term:`physmap` describes the mapping between them.

 physmap
  The domain's physical-to-machine mapping, relating guest physical frame
  numbers to host machine frames. It is the mapping, not a synonym for the
  guest physical memory itself. Population establishes entries in this map;
  populate-on-demand can initially record placeholders instead of new frames.

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

 Xenctrl
  An OCaml library provided by Xen for :term:`domain builders` running
  in privileged :term:`domains` to interact with the hypervisor. This series
  adds C libxenctrl set/get wrappers; it does not add corresponding OCaml
  bindings for the new per-node claims operations.