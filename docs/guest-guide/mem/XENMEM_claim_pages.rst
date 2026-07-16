.. SPDX-License-Identifier: CC-BY-4.0
.. _XENMEM_claim_pages:

XENMEM_claim_pages
==================

.. note:: This API is deprecated;
   Use :c:expr:`XEN_DOMCTL_claim_memory` for new code.

.. c:macro:: XENMEM_claim_pages

   Hypercall command for installing legacy claims.

   :ref:`designs/claims/installation:Legacy claim installation` describes
   the API for installing legacy claims via this hypercall command.

   It passes a single claim entry to the hypervisor via a
   :c:expr:`xen_memory_reservation` structure with the page count in the
   :c:expr:`xen_memory_reservation.nr_extents` field and the domain ID
   :c:expr:`xen_memory_reservation.domid` field. The claim entry's target is
   implicitly global, and the legacy claim path is invoked in the hypervisor
   to process the claim:

.. c:struct:: xen_memory_reservation

   Structure for passing claim requests to the hypervisor via
   :ref:`XENMEM_claim_pages` and other memory reservation hypercalls.

   .. code-block:: C

      struct xen_memory_reservation {
          xen_pfn_t   *extent_start; /* not used for XENMEM_claim_pages */
          xen_ulong_t  nr_extents;   /* pass page counts to claim */
          unsigned int extent_order; /* must be 0 */
          unsigned int mem_flags;    /* XENMEMF flags. */
          domid_t      domid;        /* domain to apply the claim to */
      };
      typedef struct xen_memory_reservation xen_memory_reservation_t;

   .. c:member:: xen_ulong_t nr_extents

      For :ref:`XENMEM_claim_pages`, the page count to claim.

   .. c:member:: domid_t domid

        Domain ID for the claim.

   .. c:member:: unsigned int mem_flags

      Must be 0 for :ref:`XENMEM_claim_pages`; not used for claims.

      In principle, it supports all the :c:expr:`XENMEMF_*` flags, including
      the possibility of passing a single NUMA node ID, but using it to pass
      a NUMA node ID is not currently supported by the legacy claim path.

      During review of the NUMA extension of the legacy claim path, it
      was used, but the request was made to instead create a new hypercall
      which is now :c:expr:`XEN_DOMCTL_claim_memory` with support for claim sets.

   .. c:member:: unsigned int extent_order
   .. c:member:: xen_pfn_t *extent_start

      Both are not used for :ref:`XENMEM_claim_pages`, but are used for other
      memory reservation hypercalls.

   See :ref:`designs/claims/installation:Legacy claim installation` for details.

API example using libxenctrl
----------------------------

The example below claims pages, populates the domain,
and then clears the claim.

.. code-block:: C

  #include <xenctrl.h>

  int build_with_claims(xc_interface *xch, uint32_t domid,
                        unsigned long nr_pages)
  {
      int ret;

      /* Claim pages for the domain build. */
      ret = xc_domain_claim_pages(xch, domid, nr_pages);
      if ( ret < 0 )
          return ret;

      /* Populate the domain's physmap. */
      ret = xc_domain_populate_physmap(xch, domid, /* ... */);
      if ( ret < 0 )
          return ret;

      /* Release any remaining claim after populating the domain memory. */
      ret = xc_domain_claim_pages(xch, domid, 0);
      if ( ret < 0 )
          return ret;

      /* Unpause the domain to allow it to run. */
      return xc_unpause_domain(xch, domid);
  }
