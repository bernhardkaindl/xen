.. SPDX-License-Identifier: CC-BY-4.0

Handling Edge Cases
-------------------

Allocations exceeding claims
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When an allocation exceeds the domain's claims, the allocator must check
whether unclaimed memory can satisfy the remainder of the request before
rejecting the allocation.

Previously, if a domain's remaining claim did not fully cover a request,
the allocator rejected the allocation even when enough unclaimed memory
existed to satisfy it.

This forced domain builders to fall back from ``1G`` pages to ``2M`` or
``4K`` pages, reducing performance due to higher TLB pressure and
increased page bookkeeping.

.. note:: See ``libxenguest``'s ``meminit_hvm()`` and ``meminit_pv()``
   functions for examples of this behaviour in the domain builder when
   populating the domain's memory with the largest possible pages.

Now, when unclaimed memory can satisfy the remainder of the request, the
allocator permits the allocation. This lets builders continue to use large
pages when the combination of claims and unclaimed memory allows it,
improving runtime performance.

Domain destruction
^^^^^^^^^^^^^^^^^^

Installing a claim for a domain that is dying must be rejected because
the domain may already have released its claims; installing claims at
that point would permanently lose the claimed memory.

Reviewers on *xen-devel* argued that ``-EINVAL`` is misleading for dying
domains; they requested ``-ESRCH`` instead, since the domain is effectively
gone for the purpose of installing claims. For consistency, the legacy
single-claim path (``XENMEM_claim_pages``) now also returns ``-ESRCH``
for dying domains instead of ``-EINVAL``. This check was added recently
and was not present in previous Xen releases.
