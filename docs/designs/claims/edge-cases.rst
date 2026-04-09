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

This forced the :term:`meminit` API to fall back from ``1G`` pages to ``2M``
and eventually to ``4K`` pages, reducing performance due to higher TLB
pressure and increased page bookkeeping.

Supporting the use of unclaimed memory to satisfy the remainder of the
request in such cases lets builders continue to use large pages when the
combination of claims and unclaimed memory allows it, possibly improving
runtime performance in such scenarios.
