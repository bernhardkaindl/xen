.. SPDX-License-Identifier: CC-BY-4.0
.. _XEN_DOMCTL_claim_memory:

XEN_DOMCTL_claim_memory
=======================

This **domctl** command allows a privileged domain to stake a memory claim for
a domain identical to :ref:`XENMEM_claim_pages`, but with support for
NUMA-aware memory claims.

A claim entry with a node value of ``XEN_DOMCTL_CLAIM_MEMORY_GLOBAL`` stakes
a claim for host memory, similar to :ref:`XENMEM_claim_pages`.

NUMA-aware memory claims
------------------------

Memory locality is an important factor for performance in NUMA systems.
Allocating memory close to the CPU that will use it can reduce latency
and improve overall performance.

By claiming memory on specific NUMA nodes, Xen toolstacks can ensure that
they will be able to allocate memory for the domain on those nodes. This is
particularly beneficial for workloads that are sensitive to memory latency,
such as in-memory databases.

Implementation notes
--------------------

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

Note: ``memory_claim_t`` contains padding for future expansion.
Thus, the structure must be zero-initialised to ensure forward compatibility,
C does this when using designated initializers or compound literals, but if
the structure is initialised in another way, the padding must be explicitly
zeroed to ensure forward compatibility.

.. code-block:: C

  #include <xenctrl.h>

  int claim_guest_memory(xc_interface *xch, uint32_t domid, uint64_t pages)
  {
      /* Claim pages on node 0 and 1 and globally (unspecific) as well */
      return xc_domain_claim_memory(xch, domid, 1,
          &(memory_claim_t){
             .pages = pages, .node = 0,
             .pages = pages, .node = 1,
             .pages = pages, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL,
          });

      /* Release all claims and restate the claim on nodes 1, 2 and 3 */
      return xc_domain_claim_memory(xch, domid, 1,
          &(memory_claim_t){
             .pages = pages, .node = 1,
             .pages = pages, .node = 2,
             .pages = pages, .node = 3,
          });

      /* Release any remaining claim (do this once the domain is built */
      return xc_domain_claim_memory(xch, domid, 1,
          &(memory_claim_t){
             .pages = 0, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL
          });
  }
