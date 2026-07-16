.. SPDX-License-Identifier: CC-BY-4.0

.. c:macro:: XEN_DOMCTL_claim_memory

   Hypercall command for installing claim sets for a domain.

   This hypercall command allows :term:`domain builders` to install a
   :term:`claim set` targeting :term:`NUMA nodes` and :term:`global claims`.

   The hypervisor tracks the installed claims for each domain and enforces
   them during memory allocation, so that claimed memory is protected from
   other allocations and the domain's memory requirements can be met even
   when other parallel domain builders are also allocating memory for other
   domains in parallel.

   :ref:`designs/claims/installation:Claim set installation` describes
   how the Xen hypervisor processes the claim sets installed via this
   hypercall command.

API example using libxenctrl
----------------------------

The example below shows how a domain builder can install a claim set and
later replace or clear it. :c:expr:`memory_claim_t` contains padding for future
expansion; zero-initialise the structure or use designated initializers to
ensure forward compatibility.

.. code-block:: C

  #include <xenctrl.h>

  void example_claims(xc_interface *xch, uint32_t domid)
  {
    /* Claim 1024 pages on node 0, 1024 pages on node 1, and 1024 global */
    memory_claim_t claims[] = {
      {.pages = 1024, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL},
      {.pages = 1024, .node = 0},
      {.pages = 1024, .node = 1}
    };
    xc_domain_claim_memory(xch, domid, ARRAY_SIZE(claims), claims);

    /* Replace the claim set with claims on nodes 1, 2, and 3 */
    memory_claim_t claims2[] = {
      {.pages = 1024, .node = 1},
      {.pages = 1024, .node = 2},
      {.pages = 1024, .node = 3},
    };
    xc_domain_claim_memory(xch, domid, ARRAY_SIZE(claims2), claims2);

    /* Release any remaining claim once the domain is built */
    memory_claim_t clear[] = {
      {.pages = 0, .node = XEN_DOMCTL_CLAIM_MEMORY_GLOBAL}
    };
    xc_domain_claim_memory(xch, domid, ARRAY_SIZE(clear), clear);
  }

Call sequence diagram
---------------------

The following sequence diagram illustrates the call flow for claiming memory
for a domain using this hypercall command from an OCaml domain builder:

.. mermaid:: DOMCTL_claim_memory-seqdia.mmd
  :caption: Sequence diagram: Call flow for claiming memory for a domain

Claim workflow
--------------

This diagram illustrates a workflow for claiming and populating memory:

.. mermaid:: DOMCTL_claim_memory-workflow.mmd
  :caption: Workflow diagram: Claiming and populating memory for a domain

Used functions & data structures
--------------------------------

This diagram illustrates the key functions and data structures involved in
installing claims via the :c:expr:`XEN_DOMCTL_claim_memory` hypercall command:

.. mermaid:: DOMCTL_claim_memory-data.mmd
  :caption: Diagram: Function and data relationships for installing claims
