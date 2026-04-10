.. SPDX-License-Identifier: CC-BY-4.0

########################
Claim Installation Paths
########################

**********
Claim sets
**********

A claim set is an array of :c:expr:`memory_claim_t` entries, each specifying
a page count and a target.  Targets are either a NUMA node ID, or one of two
special values:

.. c:macro:: XEN_DOMCTL_CLAIM_MEMORY_GLOBAL

   Value for the :c:expr:`xen_memory_claim.target` field of a claim set entry
   to specify a global claim satisfied from any node, useful when strict
   per-node placement is not required or as a fallback for memory that
   may be populated on any node.

   These claims are retired on allocation only when the allocation node's
   claims are exhausted, so they provide a way to claim memory when the
   available memory on the allocation nodes is not fully sufficient to
   satisfy the domain's needs, but the global pool has sufficient free
   memory to cover the shortfall and the domain can tolerate some fallback
   to non-preferred nodes without selecting a specific node for the fallback.

   Supported by :c:expr:`XEN_DOMCTL_claim_memory` but not the legacy claim path.

.. c:macro:: XEN_DOMCTL_CLAIM_MEMORY_LEGACY

   This is a special selector for :c:expr:`xen_memory_claim.target` that can
   only be used in a single-entry claim set to indicate that the claim set
   should be processed by the legacy claim installation logic. It is not a
   valid target for regular claims and is not supported for multi-entry
   claim sets and is only used for backward compatibility and is not
   intended for use in new code.

.. note:: The legacy path is deprecated. Use :c:expr:`XEN_DOMCTL_claim_memory`
   with :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_GLOBAL` for global claims in new
   code instead of :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_LEGACY`.

.. c:type:: memory_claim_t

   Typedef for :c:expr:`xen_memory_claim`,
   the structure for passing claim sets to the hypervisor.

.. c:struct:: xen_memory_claim

   Underlying structure for passing claim sets to the hypervisor.

   This structure represents an individual claim entry in a claim set.
   It specifies the number of pages claimed and the target of the claim,
   which can be a specific NUMA node or a special value for global claims.

   The structure includes padding for future expansion, and it is important
   to zero-initialise it or use designated initializers to ensure forward
   compatibility. Members are as follows:

   .. c:member:: uint64_aligned_t pages

      Number of pages for this claim entry.

   .. c:member:: uint32_t target

      The target of the claim, which can be a specific NUMA node
      or a special selector to steer the claim to the global pool
      or to invoke the legacy claim path.
      Valid values are either a node ID in the range of valid NUMA nodes, or:

      :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_GLOBAL` for a global claim, or
      :c:expr:`XEN_DOMCTL_CLAIM_MEMORY_LEGACY` for the legacy claim path.

   .. c:member:: uint32_t pad

      Reserved for future use, must be 0 for forward compatibility.

.. c:type:: uint64_aligned_t

   64-bit unsigned integer type with alignment requirements suitable for
   representing page counts in the claim structure.

**********************
Claim set installation
**********************

Claim set installation is invoked via :c:expr:`XEN_DOMCTL_claim_memory` and
:ref:`designs/claims/implementation:domain_set_node_claims()` implements
the claim set installation logic.

Claim sets using
:c:expr:`XEN_DOMCTL_CLAIM_MEMORY_LEGACY` are dispatched to
:ref:`designs/claims/implementation:domain_set_outstanding_pages()`
for the legacy claim installation logic.

See :doc:`accounting` for details on the claims accounting state.

*************************
Legacy claim installation
*************************

.. note:: The legacy path is deprecated.
   Use :c:expr:`XEN_DOMCTL_claim_memory` for new code.

Legacy claims are set via the :ref:`XENMEM_claim_pages` command,
implemented by
:ref:`designs/claims/implementation:domain_set_outstanding_pages()`
with the following semantics:

- The request contains exactly one global claim entry of the form
  :c:expr:`xen_memory_claim.target = XEN_DOMCTL_CLAIM_MEMORY_LEGACY`.
- It sets :c:expr:`domain.global_claims` to the requested pages, minus
  the domain's total pages, i.e. the pages allocated to the domain so far,
  so that the domain's global outstanding claims reflect the shortfall of
  allocated pages from claimed pages:
  :c:expr:`xen_memory_claim.pages - domain_tot_pages(domain)`.
- Passing :c:expr:`xen_memory_claim.pages == 0`
  clears all claims installed for the domain.

Aside from the edge cases for allocations exceeding claims and
offlining pages, the legacy path is functionally unchanged.
