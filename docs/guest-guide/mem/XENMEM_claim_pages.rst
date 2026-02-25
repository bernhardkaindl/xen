.. SPDX-License-Identifier: CC-BY-4.0
.. _XENMEM_claim_pages:

XENMEM_claim_pages
==================

This **xenmem** command allows a privileged guest to stake a memory claim for a
domain, identical to :ref:`XEN_DOMCTL_claim_memory`, but without support for
NUMA-aware memory claims.

Memory claims in Xen
--------------------

The Xen hypervisor maintains a counter of outstanding pages for each domain
which maintains a number of pages claimed, but not allocated for that domain.

If the outstanding pages counter is zero, this hypercall allows a privileged
guest to stake a claim for a specified number of pages of system memory for the
domain.

If the claim is successful, Xen updates the domain's outstanding pages counter
to reflect the new claim, Xen allocates from the pool of claimed memory only
for allocations for domains with a claim for this memory.

A domain builder (toolstack in a privileged domain) building the domain can then
allocate the guest memory for the domain, which converts the outstanding claim
into actual memory of the new domain, backed by physical pages.

Note that the resulting claim is relative to the already allocated pages for the
domain, so the **pages** argument of this hypercall is absolute and must
correspond to the total number expected to be allocated for the domain,
and not incremental to the already allocated pages.

Memory allocations by Xen for the domain also consume the claim, so toolstacks
should stake a claim that is larger than the guest memory requirement to
account for Xen's own memory usage. The exact amount of extra memory required
depends on the configuration and features used by the domain, the host
architecture and the features enabled by the Xen hypervisor on the host.

Life-cycle of a claim
---------------------

The Domain's maximum memory limit must be set prior to staking a claim as
the sum of the already allocated pages and the claim must be within that limit.

To release the claim after the domain build is complete, call this hypercall
command with the pages argument set to zero. This releases any remaining claim.
`libxenguest` does this after the guest memory has been allocated for the domain
and Xen does this also when it kills the domain.

API example (libxc)
-------------------
The following example demonstrates how a toolstack can claim memory before
building the domain and then releasing the claim once the memory population
is complete.

.. code-block:: C

  #include <xenctrl.h>
  ...
      /* Claim memory for the domain build. */
      int ret = xc_domain_claim_pages(xch, domid, nr_pages);

      /* Build the domain and allocate memory for it. */
      ...

      /* Release any remaining claim after populating the domain memory. */
      int ret = xc_domain_claim_pages(xch, domid, 0);
