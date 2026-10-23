Patch 1: Protect per-node claims
================================

With per-node claims installed, ensure that allocations respect
the per-node claims. Otherwise, no functional change occurs.

Use the unclaimed amount of memory as the node's available capacity.
If the node's total available capacity is below the request, skip
the node.

This applies also when no per-node claims are installed, but in
this case, the existing per-zone check of the node would already
have skipped the node after needlessly checking all zones, so the
per-node check avoids this overhead when the memory of that node
is below the allocation request.

As only ref-counted allocations redeem claims, only ref-counted
allocations get credit for the domain's claims, which mirrors the
existing host-wide claims boundary for ref-counted allocations.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: Free memory can drop below
   :end-at: goto next_node;
   :dedent: 8
   :emphasize-lines: 7-8

The goto statement in the code snippet above is used to skip to the next
node if the current node does not have enough available capacity to satisfy
the allocation request and to end the search if no suitable node is found.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: zone-- > zone_lo
   :end-at: nodemask_test
   :emphasize-lines: 4-8

Review questions
----------------

Can adding the domain's own claim overflow availability?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Adding ``d->claims[node]`` can look like an unchecked sum.

**Disposition.** ``d->claims[node]`` is bounded by the node's aggregate
claim, ensuring that adding it back cannot overflow the available capacity
unless memory offlining reduces the node's available capacity below the
domain's claim. In this case, the per-zone checks still apply and would
reject the allocation unless sufficient memory is made available again.
