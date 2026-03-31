.. SPDX-License-Identifier: CC-BY-4.0

Claim Consumption
-----------------

After a successful allocation,
:ref:`designs/claims/implementation:consume_allocation()` consumes
claims in the same critical region that updates the free-page counters.

It performs the following steps to consume the allocation from the
domain's claims, ensuring the domain's total allocation plus claims
remain within its limits:

1. Consume the allocation from ``d->claims[node]`` on the allocation
   node, up to the size of that claim.
2. If the allocation exceeds ``d->claims[node]``, consume the remaining
   pages from the global fallback claim ``d->global_claims``.
3. If the allocation still exceeds the combination of those claims,
   consume the remaining pages from other per-node claims so that the
   domain's total allocation plus claims remain within the domain's
   ``d->max_pages`` limit.

Domain max_pages limit
^^^^^^^^^^^^^^^^^^^^^^

The domain's claims on top of its current allocation must not exceed the
domain's ``max_pages`` limit. This is checked at claim installation time by
:ref:`designs/claims/implementation:domain_set_outstanding_pages()`.

Otherwise, a domain's claims could exceed its entitlement. Such excess
claims would be unusable by that domain but would still prevent other
domains from using the claimed memory.

:ref:`designs/claims/implementation:consume_allocation()` cannot check this
during step 3 because it would have to take the domain's ``page_alloc_lock``
to inspect the domain's limits and current allocation. Taking that lock while
holding the ``heap_lock`` would invert the locking order and could lead to
deadlocks.

.. note::
   See :ref:`designs/claims/accounting:Locking of claims accounting`
   for the locks used to protect claims accounting state and invariants.

Therefore, :ref:`designs/claims/implementation:consume_allocation()`
consumes the remaining allocation from other-node claims to ensure
the sum of the domain's claims and populated pages remains within the
domain's ``max_pages`` limit.
