.. SPDX-License-Identifier: CC-BY-4.0

Protection of Claims
--------------------

Claimed memory must be protected from unrelated allocations while remaining
available to the claiming domain.

The allocator performs two checks.

Global check
^^^^^^^^^^^^

``alloc_heap_pages()`` first verifies whether the request fits the global
pool after accounting for claims. The request is permitted when either:

- Enough unclaimed memory exists globally to satisfy the request.
- The requesting domain's outstanding claims cover the shortfall.

For this check, the domain's applicable claim is
``d->global_claims + d->node_claims``. The domain therefore receives
credit for its complete claim set, whether reservations are global,
per-node, or both.

Node check
^^^^^^^^^^

After passing the global check, the allocator calls ``get_free_buddy()``
for finding free pages. It loops over the NUMA nodes to find a suitable
node with enough free memory to satisfy the request.

It performs an additional node-local claims check using the domain's claim
for that node (``d->claims[node]``) to determine whether the node is qualified
to satisfy the request before examining that node's free lists.

Unless the caller requested an exact node, the allocator loops
over nodes until it finds one where the request can be satisfied
by the unclaimed memory and the node-local claim for that node.

If no qualifying node is found, the allocator rejects the request
due to insufficient memory.
