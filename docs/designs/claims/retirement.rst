.. SPDX-License-Identifier: CC-BY-4.0

Claim Retirement
----------------

After a successful allocation,
:ref:`designs/claims/implementation:claims_retire_allocation()` retires
claims up to the size of the allocation in the same critical region
that updates the free-page counters.

The function performs the following steps to retire the matching claims
for this allocation, ensuring the domain's total memory allocation as
:c:expr:`domain_tot_pages(domain)` plus its outstanding claims as
:c:expr:`domain.global_claims + domain.node_claims` remain within the
domain's limits, defined by :c:expr:`domain.max_pages`:

Step 1:
 Retire claims from :c:expr:`domain.claims[alloc_node]` on the allocation
 node, up to the size of that claim.
Step 2:
 If the allocation exceeds :c:expr:`domain.claims[alloc_node]`, retire the
 remaining pages from the global fallback claim :c:expr:`domain.global_claims`
 (if one exists).
Step 3:
 If the allocation exceeds the combination of those claims, retire the
 remaining pages from other per-node claims so that the domain's total
 allocation plus claims remain within the domain's :c:expr:`domain.max_pages`
 limit.

Enforcing the :c:expr:`domain.max_pages` limit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:c:expr:`domain_tot_pages(domain)` +
:c:expr:`domain.global_claims + domain.node_claims`
must not exceed the :c:expr:`domain.max_pages` limit, otherwise
the domain would exceed its memory entitlement.

At claim installation time
 This check is done by
 :c:expr:`domain_set_node_claims()` and
 :c:expr:`domain_set_outstanding_pages()`.

.. :sidebar::
   See :ref:`designs/claims/accounting:Locking of claims accounting`
   for the locks used to protect claims accounting state and invariants.

A memory allocation time
 It is also possible for claims to become excessive after allocating memory
 if the domain has claims that are not retired by the allocation:

 If allocations would not retire enough claims to keep the sum of the domain's
 allocation and claims within the domain's :c:expr:`domain.max_pages` limit,
 the combination of the allocation and claims could exceed the domain's limit.

 In this case, the domain's claims could exceed its memory entitlement.
 Such excess beyond :c:expr:`domain.max_pages` claims could be actually
 physically allocated for that domain, but would still prevent other
 domains from using the excess claimed memory.

 :ref:`designs/claims/implementation:claims_retire_allocation()` cannot execute
 this exact step race-free during step 3 because it would have to take the
 :c:expr:`domain.page_alloc_lock` to inspect the domain's limits
 and current allocation. Taking that lock while holding the
 :c:expr:`heap_lock` would invert the locking order and could
 lead to deadlocks.

Therefore, :ref:`designs/claims/implementation:claims_retire_allocation()`
retires the remaining allocation from other-node claims to ensure
the sum of the domain's claims and populated pages remains within the
domain's :c:expr:`domain.max_pages` limit.
