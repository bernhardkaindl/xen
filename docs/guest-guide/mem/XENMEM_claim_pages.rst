.. SPDX-License-Identifier: CC-BY-4.0
.. _XENMEM_claim_pages:

claim_pages
***********

 .. note:: This legacy API remains available for compatibility.
    Use :c:expr:`XEN_DOMCTL_set_memory_claims` and
    :c:expr:`XEN_DOMCTL_get_memory_claims` for new code.

 .. c:macro:: XENMEM_claim_pages

   Hypercall command for installing legacy claims.

   :ref:`designs/claims/installation:Legacy claim installation` describes
   the API for installing legacy claims via this hypercall command.

   It passes a final total-page target via
   :c:member:`xen_memory_reservation.nr_extents` and the target domain ID via
   :c:member:`xen_memory_reservation.domid`. Xen subtracts pages already
   allocated to the domain to obtain the new host-wide outstanding claim.
   A nonzero target must exceed the domain's current total and must not
   exceed ``max_pages``; the domain must have no outstanding claims.

   A zero target releases all outstanding claims, including per-node claims.
   Unlike the new domctls, this legacy path does not require an unfinished
   creation phase. Dummy XSM requires control-domain privilege; FLASK checks
   ``domain2:setclaim``. LLC coloring rejects the legacy operation,
   including release, with ``EOPNOTSUPP``.

Data structure for the hypercall command for installing legacy claims:

 .. c:struct:: xen_memory_reservation

   Structure for passing claim requests to the hypervisor via
   :c:macro:`XENMEM_claim_pages` and other memory :term:`hypercalls`.

   .. code-block:: C

      struct xen_memory_reservation {
           XEN_GUEST_HANDLE(xen_pfn_t) extent_start;
           xen_ulong_t nr_extents;
           unsigned int extent_order;
           unsigned int mem_flags;
           domid_t domid;
      };
      typedef struct xen_memory_reservation xen_memory_reservation_t;

   .. c:member:: xen_ulong_t nr_extents

      For :c:macro:`XENMEM_claim_pages`, the final total-page target, not an
      additional outstanding-page count. Zero releases all claims.

   .. c:member:: domid_t domid

      Domain ID for the claim.

   .. c:member:: unsigned int mem_flags

      Must be zero for :c:macro:`XENMEM_claim_pages`; nonzero flags are
      rejected with ``EINVAL``. Use the new domctls for per-node claims.

   .. c:member:: unsigned int extent_order

      Must be zero for :c:macro:`XENMEM_claim_pages`.

   The ``extent_start`` guest handle must be null for
   :c:macro:`XENMEM_claim_pages`. A non-null handle is rejected even though
   the operation does not transfer page-frame IDs.

See :ref:`designs/claims/installation:Legacy claim installation` for details.

API example using libxenctrl
----------------------------

The helper below installs a host-wide claim for a domain build. Its argument
is the final total-page target, not the number of additional pages to claim.

 .. code-block:: C

  #include <xenctrl.h>

  int claim_for_domain_build(xc_interface *xch, uint32_t domid,
                      unsigned long total_pages)
  {
     return xc_domain_claim_pages(xch, domid, total_pages);
  }

The domain builder must populate the physmap and complete the remaining
domain-build steps separately. It can clear outstanding claims with
``xc_domain_claim_pages(xch, domid, 0)``, including on a failed build before
destroying the domain. Unpausing alone is not a complete domain build.
