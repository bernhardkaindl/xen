Multi-Node NUMA Claims Design
==============================

Background
----------

Xen's page allocator supports a "claims" mechanism that allows a domain to
reserve a portion of available memory before actually allocating it. This
prevents other domains from consuming memory that has been earmarked for a
domain under construction. The existing implementation supports a single
host-wide claim per domain (``d->outstanding_pages``).

This design extends the claims mechanism to support per-NUMA-node claims,
enabling a domain builder to reserve specific amounts of memory on specific
NUMA nodes. This is essential for NUMA-aware domain construction where a
domain's memory should be distributed across nodes.

Design Choice: ``d->outstanding_pages`` as Unified Total
---------------------------------------------------------

The two claim types serve different use cases:

- **Host-wide claims** (``d->outstanding_pages`` only): Used by the
  existing ``XENMEM_claim_pages`` hypercall path and any domain builder
  that does not need NUMA-aware memory placement. The claim is a single
  count of pages reserved against the global ``total_avail_pages`` pool.
  All ``d->claims[node]`` entries are zero.

- **Per-node claims** (``d->claims[node]``): Used by NUMA-aware domain
  builders that need to reserve specific amounts of memory on specific
  NUMA nodes. Each entry in the array is a count of pages reserved against
  that node's ``node_avail_pages[node]`` pool.

In both modes, ``d->outstanding_pages`` holds the domain's **total**
outstanding claim count. For host-wide claims this is set directly. For
per-node claims it equals ``sum(d->claims[])``. The setting and release
paths maintain this invariant, and reject a new claim if any claim of
either type is already active.

This differs from an earlier design that kept ``d->outstanding_pages``
at zero during per-node claiming. That approach had a **correctness
bug** in the global protection check:

  The global check in ``alloc_heap_pages`` tests whether the request fits
  within ``unclaimed_pages + d->outstanding_pages``. With
  ``d->outstanding_pages == 0`` during per-node claiming, the domain gets
  **no credit** at the global level. This incorrectly rejects allocations
  when other domains' claims reduce the global unclaimed pool below the
  request size, even though the node-level check passed.

  Example: Node 0 has 100 free pages with 90 claimed by domain A. Node 1
  has 10 free pages with 5 claimed by domain B. Globally: 110 free, 95
  claimed, 15 unclaimed. Domain A requests 20 pages from node 0.
  Node check: 20 ≤ 10 + 90 = 100 → pass. Global check with the old
  design: 20 ≤ 15 + 0 = 15 → **fail**. With the corrected design:
  20 ≤ 15 + 90 = 105 → **pass**.

Having ``d->outstanding_pages`` always hold the total also:

1. **Avoids hot-path loops**: The global protection check uses
   ``d->outstanding_pages`` in O(1) without iterating ``d->claims[]``.

2. **Simplifies the global invariant**: ``outstanding_claims`` equals
   the sum of ``d->outstanding_pages`` over all domains — no need to
   separately account for per-node claims.

3. **Clear consumption semantics**: The consumption path always
   decrements ``d->outstanding_pages`` and ``outstanding_claims``
   together. For per-node claims it additionally adjusts
   ``d->claims[node]`` and ``node_outstanding_claims[node]``.

Per-Node Claims Storage
-----------------------

Per-node claims are stored as an inline array in ``struct domain``::

    unsigned int claims[MAX_NUMNODES]; /* per-NUMA-node claims */

Where ``MAX_NUMNODES`` is ``CONFIG_NR_NUMA_NODES`` (range 2..64, default 64).

**Memory cost**: With the default of 64 nodes and ``sizeof(unsigned int)``
= 4, the array consumes 256 bytes per domain.

**Justification**: ``struct domain`` is allocated from a single 4 KiB page
(enforced by ``BUILD_BUG_ON(sizeof(*d) > PAGE_SIZE)`` in
``alloc_domain_struct()``). The struct currently has ample headroom within
the page. An inline array of 256 bytes (or even several such arrays for
future per-node bookkeeping) is a small fraction of the page and avoids
the complexity, error handling, and cache-miss cost of a separate
heap-allocated array. Since the page is already fully allocated regardless
of how much of it ``struct domain`` occupies, using the spare space is
efficient, not wasteful.

An alternative of dynamically allocating a ``claims`` array only when
per-node claims are active was considered and rejected: it adds allocation
failure paths in a context that is already holding two locks
(``page_alloc_lock`` and ``heap_lock``), and complicates teardown. The
inline array is zero-initialized at domain creation and requires no
separate lifecycle management.

Global Accounting
-----------------

Three levels of accounting are maintained:

1. **Per-domain total**: ``d->outstanding_pages`` — total pages claimed
   by this domain across all claim types. For host-wide claims this is
   the single claim value. For per-node claims this equals
   ``sum(d->claims[])``.

2. **Per-domain per-node**: ``d->claims[node]`` — pages claimed by this
   domain on a specific NUMA node (zero for host-wide claims).

3. **Global total**: ``outstanding_claims`` — sum of all domains'
   ``d->outstanding_pages``. Used by ``total_avail_pages -
   outstanding_claims`` to compute globally available unclaimed memory.

4. **Global per-node**: ``node_outstanding_claims[node]`` — sum of all
   domains' ``d->claims[node]`` on a given node. Used by
   ``node_avail_pages[node] - node_outstanding_claims[node]`` to compute
   per-node available unclaimed memory.

Invariants:

- ``outstanding_claims == sum over all domains of d->outstanding_pages``

- For per-node claims:
  ``d->outstanding_pages == sum over all nodes of d->claims[node]``

- ``node_outstanding_claims[node] == sum over all domains of
  d->claims[node]``

- For any domain, either ``d->outstanding_pages > 0`` with all
  ``d->claims[node] == 0`` (host-wide), or ``d->outstanding_pages > 0``
  with some ``d->claims[node] > 0`` (per-node), or everything is zero
  (no claims).

Setting Claims (``domain_set_outstanding_pages``)
-------------------------------------------------

The function signature becomes::

    int domain_set_outstanding_pages(struct domain *d, unsigned long pages,
                                     unsigned int node_claims,
                                     const unsigned int *claims);

Parameters:

- ``pages``: Total pages for a host-wide claim (used when ``node_claims``
  is 0). Must be 0 when ``node_claims > 0``.
- ``node_claims``: Number of entries in the ``claims`` array. When > 0,
  this indicates per-node claiming mode.
- ``claims``: Array indexed by NUMA node ID. ``claims[i]`` is the number
  of pages to claim on node ``i``. The array must have ``node_claims``
  entries.

Behavior:

- ``pages == 0 && node_claims == 0``: **Release** all claims for this domain
  (both host-wide and per-node, whichever is active).
- ``pages > 0 && node_claims == 0``: Set a **host-wide** claim for ``pages``
  total pages.
- ``pages == 0 && node_claims > 0``: Set **per-node** claims from the
  ``claims`` array. The total is computed as the sum of all entries.
- ``pages > 0 && node_claims > 0``: Invalid, returns ``-EINVAL``.

Validation requirements for per-node claims:

- Each node with a non-zero claim must be online (``node_online(node)``).
- Each node's claim must not exceed the available unclaimed pages on that
  node.

Note on ``domain_tot_pages()`` adjustment:

For host-wide claims, the stored claim is ``pages - domain_tot_pages(d)``,
accounting for pages the domain has already allocated. For per-node claims,
there is currently no per-node equivalent of ``domain_tot_pages()`` — we do
not yet track how many of a domain's allocated pages came from each node.
Therefore, per-node claims are stored as the raw values from the
``claims[]`` array, and ``d->outstanding_pages`` is their exact sum. A
future ``d->total_node_pages[node]`` counter will allow the per-node path
to mirror the host-wide adjustment.

Protection (``protect_outstanding_claims``)
-------------------------------------------

This function decides whether an allocation of ``request`` pages should
proceed, given that claimed memory reduces the apparent free pool.

Two call sites exist:

1. **Node-level check** (in ``get_free_buddy``): Called with
   ``node_avail_pages[node]``, ``node_outstanding_claims[node]``, and the
   specific ``node``. Uses ``d->claims[node]`` as the domain's applicable
   claim.

2. **Global check** (in ``alloc_heap_pages``): Called with
   ``total_avail_pages``, ``outstanding_claims``, and ``NUMA_NO_NODE``.
   Uses ``d->outstanding_pages`` as the domain's applicable claim.

Because ``d->outstanding_pages`` always holds the domain's total claim
(whether host-wide or the sum of per-node claims), the global check
correctly grants the domain credit for its full claimed reservation.
This is an O(1) check with no need to iterate ``d->claims[]``.

For a domain with per-node claims, the node-level check provides the
primary NUMA-aware gate, while the global check ensures the domain can
still allocate even when other domains' claims reduce the global
unclaimed count.

Consumption (``consume_outstanding_claims``)
--------------------------------------------

When pages are actually allocated to a domain, the corresponding claims
must be consumed. This function is called with the allocation size and the
node from which the allocation was satisfied.

The consumption logic differs by claim type:

**Per-node claims**:

1. Consume from ``d->claims[alloc_node]`` up to the allocation size.
   Decrement ``d->claims[alloc_node]``, ``d->outstanding_pages``,
   ``node_outstanding_claims[alloc_node]``, and ``outstanding_claims``
   by the consumed amount. Claims on other nodes are **not** touched —
   they remain valid reservations for future allocations from those
   nodes.

2. Check whether the domain is now over-booked: if
   ``domain_tot_pages(d) + allocation + d->outstanding_pages`` exceeds
   ``d->max_pages``, trim per-node claims on other nodes to bring the
   total within budget, decrementing all four counters in lockstep
   (``d->claims[node]``, ``d->outstanding_pages``,
   ``node_outstanding_claims[node]``, ``outstanding_claims``).

   This handles the case where a domain builder allocates from a
   fallback node (e.g., a node where the domain had no claim), and the
   extra allocation pushes the domain closer to its ``max_pages``
   limit — the claims on other nodes must then shrink to reflect that.
   If the domain is not over-booked, all other claims are preserved.

**Host-wide claims** (``d->claims[]`` all zero):

1. Consume ``min(allocation, d->outstanding_pages)`` directly from
   ``d->outstanding_pages`` and ``outstanding_claims``.
2. The over-booking loop is a no-op since there are no per-node claims
   to trim.

The invariant ``sum(d->claims[]) == d->outstanding_pages`` is maintained
in both modes because ``d->outstanding_pages`` is only decremented
alongside the corresponding ``d->claims[]`` entries (or alone in
host-wide mode where all entries are zero).

Release
-------

Claims are released in two scenarios:

1. **Explicit release**: The caller invokes
   ``domain_set_outstanding_pages(d, 0, 0, NULL)``. This clears all claims
   for the domain.

2. **Domain death**: ``domain_kill()`` calls
   ``domain_set_outstanding_pages(d, 0, 0, NULL)`` to release any active
   claims before teardown.

The release path clears ``d->outstanding_pages`` and iterates over all
nodes to clear any ``d->claims[node]`` entries, decrementing
``outstanding_claims`` and ``node_outstanding_claims[node]``
accordingly. As a defensive measure, both are cleared unconditionally
regardless of which claim type was active.
