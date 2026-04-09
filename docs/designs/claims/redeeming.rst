.. SPDX-License-Identifier: CC-BY-4.0

Redeeming Claims
----------------

After a successful allocation,
:ref:`designs/claims/implementation:redeem_claims_for_allocation()`
redeems claims up to the size of the allocation in the same critical
region that updates the free-page counters.

The function performs the following steps to redeem the matching claims
for this allocation, ensuring the domain's total memory allocation as
:c:expr:`domain_tot_pages(domain)` plus its outstanding claims as
:c:expr:`domain.global_claims + domain.node_claims` remain within the
domain's limits, defined by :c:expr:`domain.max_pages`:

Steps to redeem claims for an allocation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Step 1:
  Redeem claims from :c:expr:`domain.claims[alloc_node]` on the allocation
  node, up to the size of that claim.
Step 2:
  If the allocation exceeds :c:expr:`domain.claims[alloc_node]`, redeem the
  remaining pages from the global fallback claim :c:expr:`domain.global_claims`
  (if one exists).
Step 3:
  If the allocation exceeds the combination of those claims, redeem the
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

At memory allocation time
  If (unexpectedly) a domain builder ends up allocating memory from
  different nodes than it claimed from, the domain's total allocation
  plus claims could exceed the domain's :c:expr:`domain.max_pages`
  limit, unless the page allocator redeems claims from other nodes
  to ensure the sum of the domain's claims and populated pages
  remains within the :c:expr:`domain.max_pages` limit.

  :ref:`designs/claims/implementation:redeem_claims_for_allocation()`
  cannot reliably check :c:expr:`domain.max_pages` race-free because
  :c:expr:`domain.max_pages` is not protected by the :c:expr:`heap_lock`
  taken by the page allocator during allocation.

  To check the domain's limits, it would have to take the
  :c:expr:`domain.page_alloc_lock` to inspect the domain's
  limits and its current allocation. However, taking that lock
  while holding the :c:expr:`heap_lock` would invert the locking
  order and could lead to deadlocks.

  Therefore, :ref:`designs/claims/implementation:redeem_claims_for_allocation()`
  redeems the remaining allocation from other-node claims in Step 3.
