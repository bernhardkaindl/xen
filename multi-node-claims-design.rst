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

Design Choice: Mutual Exclusivity of Claim Types
-------------------------------------------------

A domain can have **either** a global host-wide claim **or** per-NUMA-node
claims, but never both simultaneously. This mutual exclusivity simplifies
the implementation and avoids ambiguity in the allocation and consumption
paths.

The two claim types serve different use cases:

- **Host-wide claims** (``d->outstanding_pages``): Used by the existing
  ``XENMEM_claim_pages`` hypercall path and any domain builder that does
  not need NUMA-aware memory placement. The claim is a single count of
  pages reserved against the global ``total_avail_pages`` pool.

- **Per-node claims** (``d->claims[node]``): Used by NUMA-aware domain
  builders that need to reserve specific amounts of memory on specific
  NUMA nodes. Each entry in the array is a count of pages reserved against
  that node's ``node_avail_pages[node]`` pool.

The field ``d->outstanding_pages`` must only be non-zero when the domain
has a global host-wide claim. Conversely, any ``d->claims[node]`` entry
must only be non-zero when the domain has per-node claims. The setting
and release paths enforce this invariant by rejecting a new claim if any
claim of either type is already active.

This is the cleanest separation because:

1. **No double-counting**: The global ``outstanding_claims`` counter tracks
   the sum of all claims (both host-wide and per-node). If a domain had
   both types simultaneously, the consumption logic would need to determine
   which counter to decrement for each allocation, introducing fragile
   ordering dependencies.

2. **Clear consumption semantics**: When an allocation occurs on a
   particular node, the consumption path can simply check: does this
   domain have host-wide claims? If so, decrement ``d->outstanding_pages``
   and ``outstanding_claims``. Otherwise, decrement ``d->claims[node]``
   and both ``node_outstanding_claims[node]`` and ``outstanding_claims``.
   There is no ambiguity.

3. **Clean protection semantics**: The ``protect_outstanding_claims()``
   function, which decides whether an allocation should proceed despite
   claimed memory reducing the apparent free count, can select the
   appropriate claim counter based solely on whether the check is
   node-level or global. At the node level, it uses ``d->claims[node]``;
   at the global level, it uses ``d->outstanding_pages``. No
   cross-referencing is needed.

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

1. **Per-domain host-wide**: ``d->outstanding_pages`` — total pages
   claimed globally by this domain (only for host-wide claims).

2. **Per-domain per-node**: ``d->claims[node]`` — pages claimed by this
   domain on a specific NUMA node (only for per-node claims).

3. **Global host-wide**: ``outstanding_claims`` — sum of all domains'
   outstanding claims (both types). Used by ``total_avail_pages -
   outstanding_claims`` to compute globally available unclaimed memory.

4. **Global per-node**: ``node_outstanding_claims[node]`` — sum of all
   domains' per-node claims on a given node. Used by
   ``node_avail_pages[node] - node_outstanding_claims[node]`` to compute
   per-node available unclaimed memory.

Invariants:

- ``outstanding_claims == sum over all domains of (d->outstanding_pages +
  sum over all nodes of d->claims[node])``

- ``node_outstanding_claims[node] == sum over all domains of
  d->claims[node]``

- For any domain, either ``d->outstanding_pages > 0`` or some
  ``d->claims[node] > 0``, but never both.

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
- The total claim (sum of per-node claims) minus ``domain_tot_pages(d)``
  must represent the actual new pages needed, analogous to the host-wide
  path.

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

For a domain with per-node claims, the node-level check is the primary
gate. The global check will see ``d->outstanding_pages == 0``, so it falls
back to the unclaimed-pages-only check. This is correct because if the
node-level check passed (meaning the node has enough unclaimed + claimed
pages for this domain), the global check should also pass (the global pool
is at least as large as any single node's pool).

Consumption (``consume_outstanding_claims``)
--------------------------------------------

When pages are actually allocated to a domain, the corresponding claims
must be consumed. This function is called with the allocation size and the
node from which the allocation was satisfied.

For host-wide claims (``d->outstanding_pages > 0``):

- Consume up to ``min(allocation, d->outstanding_pages)`` from
  ``d->outstanding_pages`` and ``outstanding_claims``.

For per-node claims (``d->claims[]`` active):

- First, consume from ``d->claims[alloc_node]`` up to the allocation size.
  Decrement ``d->claims[alloc_node]``, ``node_outstanding_claims[alloc_node]``,
  and ``outstanding_claims`` by the consumed amount.

- If the allocation exceeds the claim on ``alloc_node`` (i.e., pages were
  allocated from a node where the domain didn't have enough claim, or had
  no claim at all), check whether the total booked pages
  (``domain_tot_pages(d) + allocation + remaining_total_claims``) exceeds
  ``d->max_pages``. If so, reduce claims on other nodes to bring the total
  within budget. This handles the case where a domain builder allocates
  from an unintended node (e.g., fallback allocation), and the claims on
  other nodes must shrink to reflect that the domain is closer to its
  ``max_pages`` limit.

- When reducing claims on other nodes, **all three counters** must be
  decremented: ``d->claims[node]``, ``node_outstanding_claims[node]``,
  and ``outstanding_claims``.

Release
-------

Claims are released in two scenarios:

1. **Explicit release**: The caller invokes
   ``domain_set_outstanding_pages(d, 0, 0, NULL)``. This clears all claims
   for the domain.

2. **Domain death**: ``domain_kill()`` calls
   ``domain_set_outstanding_pages(d, 0, 0, NULL)`` to release any active
   claims before teardown.

The release path iterates over all nodes to clear per-node claims, or
clears ``d->outstanding_pages`` for host-wide claims. Due to the mutual
exclusivity invariant, only one type needs to be active at any time, but
as a defensive measure the release path should clear both unconditionally.
