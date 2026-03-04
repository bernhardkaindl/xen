.. SPDX-License-Identifier: CC-BY-4.0
.. _XEN_DOMCTL_claim_memory:

XEN_DOMCTL_claim_memory
=======================

This **domctl** command allows a privileged domain to stake a memory claim for
a domain identical to :ref:`XENMEM_claim_pages`, but with support for
NUMA-aware memory claims.

A claim entry with a node value of ``XEN_DOMCTL_CLAIM_MEMORY_NO_NODE`` stakes
a claim for host memory, exactly like :ref:`XENMEM_claim_pages` does.

NUMA-aware memory claims
------------------------

Memory locality is an important factor for performance in NUMA systems.
Allocating memory close to the CPU that will use it can reduce latency
and improve overall performance.

By claiming memory on specific NUMA nodes, toolstacks can ensure that they
will be able to allocate memory for the domain on those nodes. This is
particularly beneficial for workloads that are sensitive to memory latency,
such as in-memory databases.

**Note:** The ABI supports multiple claims for future expansion. At the moment,
Xen accepts a single claim entry (either a NUMA-aware or host-wide claim).

Implementation notes
--------------------

As described in :ref:`XENMEM_claim_pages`, Xen keeps track of the number
of claimed pages in the domain's ``d->outstanding_pages`` counter.

Xen declares a NUMA-aware claim by assigning ``d->claim_node`` to a NUMA node,
which declares that ``d->outstanding_pages`` is claimed on ``d->claim_node``.

See :ref:`hypervisor-guide` > :ref:`memory_management` > :ref:`memory_claims`
for details on the API semantics and implementation details of the claims
infrastructure of the Xen buddy allocator backing this hypercall.

Used functions & data structures
--------------------------------

This diagram illustrates the key functions and data structures involved in the
implementation of the ``domctl`` hypercall command ``XEN_DOMCTL_claim_memory``:

.. mermaid:: DOMCTL_claim_memory-classes.mmd
  :caption: Diagram: Function and data relationships of XEN_DOMCTL_claim_memory

Call sequence diagram
---------------------

The following sequence diagram illustrates the call flow for claiming memory
for a domain using this hypercall command from an OCaml toolstack:

.. mermaid:: DOMCTL_claim_memory-seqdia.mmd
  :caption: Sequence diagram: Call flow for claiming memory for a domain

Claim workflow
--------------

The following diagram illustrates a workflow for claiming and populating memory:

.. mermaid:: DOMCTL_claim_memory-workflow.mmd
  :caption: Workflow diagram: Claiming and populating memory for a domain

API example (libxc)
-------------------
The following example demonstrates how a toolstack can claim memory before
building the domain and then releasing the claim once the memory population
is complete.

Note: ``memory_claim_t`` contains padding to allow for future expansion.
Thus, the structure must be zero-initialised to ensure forward compatibility.
This can be achieved by using the ``XEN_NODE_CLAIM_INIT`` macro, which sets the
pages and node fields while zero-initialising the padding of the structure,
zero-initialising the entire structure, or by using a compound literal with
designated initialisers to set the pages and node fields while zero-initialising
the padding of the structure.

.. code-block:: C

  #include <xenctrl.h>

  int claim_guest_memory(xc_interface *xch, uint32_t domid,
                         uint64_t pages)
  {
      memory_claim_t claim[] = {
        /*
         * Example 1:
         * Uses the ``XEN_NODE_CLAIM_INIT`` macro to zero-initialise the padding
         * and set the pages and node fields for a NUMA-aware claim on node 0.
         */
        XEN_NODE_CLAIM_INIT(pages, 0)  /* Claim memory on NUMA node 0 */
      };

      /* Claim memory from NUMA node 0 for the domain build. */
      return xc_domain_claim_memory(xch, domid, 1, claim);
  }

  int release_claim(xc_interface *xch, uint32_t domid)
  {
      memory_claim_t claim[] = {
        /*
         * Example 2:
         * Uses a compound literal with designated initialisers to set the
         * fields to release the claim while zero-initialising the rest
         * of the structure for forward compatibility.
         */
        (memory_claim_t){
          /*
           * pages == 0 releases any outstanding claim.
           * The node field is not used in this case, but must be set to
           * XEN_DOMCTL_CLAIM_MEMORY_NO_NODE for forward compatibility.
           */
          .pages = 0,
          .node = XEN_DOMCTL_CLAIM_MEMORY_NO_NODE,
        }
      };

      /* Release any remaining claim once population is done. */
      return xc_domain_claim_memory(xch, domid, 1, claim);
  }
