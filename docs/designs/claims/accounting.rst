.. SPDX-License-Identifier: CC-BY-4.0

Claims Accounting
-----------------

.. contents:: Table of Contents
   :local:

.. note::
   Claims accounting state is only updated while holding :c:expr:`heap_lock`.
   See :ref:`designs/claims/accounting:Locking of claims accounting`
   for details on the locks used to protect claims accounting state.

This section formalizes the internal state and invariants that Xen must
maintain to ensure correctness.

For readers following the design in order, the preceding sections are:

1. :doc:`/designs/claims/design` introduces the overall model and goals.
2. :doc:`/designs/claims/installation` explains how claim sets are installed.
3. :doc:`/designs/claims/protection` describes how claimed memory is
   protected during allocation.
4. :doc:`/designs/claims/retirement` explains how claims are retired as
   allocations succeed.

Overview
^^^^^^^^

.. table:: Table 1: Claims accounting - All accesses, Aggregate state,
           and invariants protected by :c:expr:`heap_lock`.
   :widths: auto

   ============ ======================================= =======================
   Level           Claims must be lower or equal to       Available memory
   ============ ======================================= =======================
   Node         :c:expr:`node_outstanding_claims[node]` :c:expr:`node_avail_pages[node]`
                  Aggregate state:

                  Over all domains:

                  SUM(:c:expr:`domain.claims[node]`)
   Global       :c:expr:`outstanding_claims` =          :c:expr:`total_avail_pages` =
                 Aggregate state:                        Aggregate state:

                 SUM() over all domains:                 SUM() over all nodes:

                 :c:expr:`domain.global_claims` +        :c:expr:`node_avail_pages[]`
                 :c:expr:`domain.node_claims`

                 Also, the sum over all nodes:

                 :c:expr:`node_outstanding_claims[*]`
   Dom global   :c:expr:`domain.global_claims`          :c:expr:`total_avail_pages`
   Dom per-node :c:expr:`domain.claims[node]`           :c:expr:`node_avail_pages[node]`
   Dom slow tot :c:expr:`domain.global_claims` +        :c:expr:`total_avail_pages`
                SUM(:c:expr:`domain.claims[node]`)
   Aggregate:   :c:expr:`domain.node_claims` =
                SUM(:c:expr:`domain.claims[node]`)
   Domain total :c:expr:`domain.global_claims`          :c:expr:`total_avail_pages`
                + :c:expr:`domain.node_claims`
   Domain mem   :c:expr:`domain_tot_pages(domain)`      Invariant: must be
                 - plus :c:expr:`domain.global_claims`   lower or equal to

                 + plus :c:expr:`domain.node_claims`     :c:expr:`domain.max_pages`
   ============ ======================================= =======================

Claims accounting state
^^^^^^^^^^^^^^^^^^^^^^^

When installing claims and retiring them during allocation, the page
allocator updates:

:c:expr:`domain.global_claims`
  The domain's global claim.

:c:expr:`domain.claims[MAX_NUMNODES]`
  The domain's claims for specific NUMA nodes, indexed by node ID.

Aggregate state
^^^^^^^^^^^^^^^

Xen also maintains aggregate state for fast checks in allocator hot paths:

:c:expr:`outstanding_claims`:
  The sum of all claims across all domains for global and node claims.

:c:expr:`node_outstanding_claims[MAX_NUMNODES]`:
  The sum of all claims across all domains for specific NUMA nodes, indexed
  by node ID, used for efficient checks in the allocator hot paths to ensure
  that node claims do not exceed the available memory on the respective node.

:c:expr:`domain.node_claims`:
  The total of the domain's node claims,
  equal to the sum of :c:expr:`domain.claims[MAX_NUMNODES]` for all nodes
  and used for efficient checks in the allocator.

:c:expr:`domain_tot_pages(domain)`
  The total pages allocated to the domain, used for validating that claims do
  not exceed the domain's maximum page limits. This is the sum of the
  domain's global claim and node claims, i.e. :c:expr:`domain.global_claims`
  + :c:expr:`domain.node_claims`.

Claims accounting invariants
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Xen must maintain the following invariants:

- Global claims:
    :c:expr:`outstanding_claims` :math:`\le` :c:expr:`total_avail_pages`

- Node claims:
    :c:expr:`node_outstanding_claims[alloc_node]` :math:`\le`
    :c:expr:`node_avail_pages[alloc_node]`
- Domain claims:
    :c:expr:`domain.global_claims` + :c:expr:`domain.node_claims` +
    :c:expr:`domain_tot_pages(domain)` :math:`\le` :c:expr:`domain.max_pages`

   See :doc:`retirement` for details on the latter invariant.

Locking of claims accounting
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:alias:: domain.page_alloc_lock

.. c:var:: spinlock_t heap_lock

   Lock for all heap operations including claims. It protects the claims state
   and invariants from concurrent updates and ensures that checks in the
   allocator hot paths see a consistent view of the claims state.

   If :c:expr:`domain.page_alloc_lock` is needed to check
   :c:expr:`domain_tot_pages(domain)` on top of new claims against
   :c:expr:`domain.max_pages` for the domain, it needs to be taken
   before :c:expr:`heap_lock` for consistent locking order to avoid deadlocks.

Variables and data structures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:type:: uint8_t nodeid_t

   Type for :term:`NUMA node` IDs. The :c:expr:`memflags` variable of
   :c:expr:`xc_populate_physmap()` and related functions for populating
   the :term:`physmap` allocates 8 bits in the flags for the node ID, which
   limits the theoretical maximum value of ``CONFIG_NR_NUMA_NODES`` at 254,
   which is far beyond the current maximum of 64 supported by Xen and should
   be sufficient for the foreseeable future.

.. c:macro:: MAX_NUMNODES

   The maximum number of NUMA nodes supported by Xen. Used for validating
   node IDs in the :c:expr:`memory_claim_t` entries of claim sets.
   When Xen is built without NUMA support, it is 1.
   The default on x86_64 is 64 which is sufficient for current hardware and
   allows for efficient storage of e.g. the :c:expr:`node_online_map` for
   online nodes and :c:expr:`domain.node_affinity` in a single 64-bit value,
   and in the :c:expr:`domain.claims[MAX_NUMNODES]` array.

   ``xen/arch/Kconfig`` limits the maximum number of NUMA nodes to 64. While
   Xen can be compiled for up to 254 nodes, configuring machines to split
   the installed memory into more than 64 nodes would be unusual.
   For example, dual-socket servers, even when using multiple chips per CPU
   package should typically be configured for 2 NUMA nodes by default.

.. c:var:: long total_avail_pages

   Total available pages in the system, including both free and claimed pages.
   This is used for validating that global claims do not exceed the total
   available memory in the system.

.. c:var:: long outstanding_claims

   The total global claims across all domains. This is maintained for
   efficient checks in the allocator hot paths to ensure the global claims
   invariant that total claims do not exceed the total available memory is not
   violated.

.. c:var:: long node_avail_pages[MAX_NUMNODES]

   Available pages for each NUMA node, including both free and claimed pages.
   This is used for validating that node claims do not exceed the available
   memory on the respective NUMA node.

.. c:var:: long node_outstanding_claims[MAX_NUMNODES]

   The total claims across all domains for each NUMA node, indexed by node
   ID. This is maintained for efficient checks in the allocator hot paths.

.. c:macro:: domain_tot_pages(domain)

   The total pages allocated to the domain, used for validating that this
   allocation and the domain's claims do not exceed :c:expr:`domain.max_pages`.

.. c:struct:: domain

   .. c:member:: unsigned int global_claims

      The domain's global claim, representing the number of pages claimed
      globally for the domain.

   .. c:member:: unsigned int node_claims

      The total of the domain's node claims, equal to the sum of
      :c:expr:`claims` for all nodes.
      It is maintained for efficient checks in the allocator hot paths
      without needing to sum over the per-node claims each time.

   .. c:member:: unsigned int claims[MAX_NUMNODES]

      The domain's claims for each :term:`NUMA node`, indexed by node ID.

      As :c:expr:`domain` is allocated using a dedicated page for each domain,
      this allows for efficient and fast storage with direct indexing without
      consuming any additional memory for an additional allocation.

      The page allocated for struct :c:expr:`domain` is large enough
      to accommodate this array several times, even beyond the current
      :c:expr:`MAX_NUMNODES` limit of 64, so it should be sufficient even
      for future expansion of the maximum number of supported NUMA nodes
      if needed. The allocation has a build-time assertion for safety to
      ensure that struct :c:expr:`domain` fits within the allocated page.

      The sum of these claims is stored in :c:expr:`domain.node_claims`
      for efficient checks in the allocator hot paths which need to know
      the total number of node claims for the :term:`domain`.

   .. c:member:: unsigned int max_pages

      The maximum number of pages the domain is allowed to claim, set at
      domain creation time.

   .. c:member:: rspinlock_t page_alloc_lock

      Lock for checking :c:expr:`domain_tot_pages(domain)` on top of new claims
      against :c:expr:`domain.max_pages` when installing these new claims.
      This is a recursive spinlock to allow for nested calls into the allocator
      while holding it, such as when retiring claims during page allocation.
      It is taken before :c:expr:`heap_lock` when installing claims to ensure a
      consistent locking order and may not be taken while holding
      :c:expr:`heap_lock` to avoid deadlocks.

   .. c:member:: nodemask_t node_affinity

      A :c:expr:`nodemask_t` representing the set of NUMA nodes the domain
      is affine to. This is used for efficient checks in the allocator hot
      paths to quickly get the set of nodes a domain is affine to for
      memory allocation decisions.

.. c:type:: nodemask_t

   A bitmap representing a set of NUMA nodes, used for status information
   like :c:expr:`node_online_map` and the :c:expr:`domain.node_affinity`
   and to track which nodes are online and which nodes are in a domain's
   node affinity.

.. c:var:: nodemask_t node_online_map

   A bitmap representing which NUMA nodes are currently online in the system.
   This is used for validating that claims are only made for online nodes and
   for efficient checks in the allocator hot paths to quickly determine which
   nodes are online. Currently, Xen does not support hotplug of NUMA nodes,
   so this is set at boot time based on the platform firmware configuration
   and does not change at runtime.

Claims Accounting Diagram
^^^^^^^^^^^^^^^^^^^^^^^^^

This diagram illustrates the claims accounting state and the invariants:

.. mermaid:: invariants.mmd
  :caption: Diagram: Claims accounting state and invariants
