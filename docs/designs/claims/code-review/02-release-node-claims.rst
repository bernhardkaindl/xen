Patch 2: Release per-node claims
================================

Extend the existing release path for ``pages == 0`` to remove the domain's
per-node claims from the sums of per-node claims before resetting the
claims of the domain.

When the domain has no per-node claims, the existing behavior remains
unchanged.

Add :c:member:`domain.node_claims` as the cached sum of the
:c:member:`domain.claims` entries to avoid having to loop
over :c:member:`domain.claims` whenever the sum of it is needed.

.. literalinclude:: /../xen/include/xen/sched.h
   :language: C
   :start-at: Cached sum of claims
   :end-at: node_claims;
   :dedent: 4

Subtract the released claim on each node from :c:var:`node_claimed_pages`
and assert that the cached sum matches.
:c:member:`domain.outstanding_pages` includes the per-node claims,
so the existing code already releases them from
:c:member:`outstanding_claims`.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: Release per-node claims;
   :end-at: ASSERT(!d->node_claims);
   :dedent: 8
   :emphasize-lines: 9-10,12

Note: :c:member:`domain.claims` is protected by the global
:c:var:`heap_lock`, so detach it while holding the lock.
As ``xvfree()`` can take the non-recursive :c:var:`heap_lock`,
free the :c:member:`domain.claims` array only after dropping
:c:var:`heap_lock`.

Review questions
----------------

Can an offline node retain a claim that the loop misses?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Iterating online nodes could skip a claim, leave the
cached sum nonzero, or free an array whose entries remain accounted.

**Disposition.** Offlining NUMA nodes is not implemented in Xen,
so the loop can only miss nodes that were never online, so no claim
can be left behind.

When offlining is eventually implemented, the code that offlines
a node will need to properly release any claims associated with
that node to maintain consistency.

Why not free the ``d->claims`` array while holding the accounting locks?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Detaching and freeing in different regions can look
like an unnecessary lifetime gap.

**Disposition.** The protected detachment removes the shared pointer
before the storage is freed. ``xvfree()`` can reach a heap-free path that
acquires ``heap_lock`` itself, so freeing under that lock risks recursion.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: Detach d->claims;
   :end-at: xvfree(old_claims);
   :emphasize-lines: 1-3,43,45
