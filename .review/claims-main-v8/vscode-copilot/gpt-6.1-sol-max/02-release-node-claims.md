# 02 - release per-node claims in `domain_set_outstanding_pages()`

**Result:** No actionable defect found. No code or commit-message change suggested.

## Code review

- Reviewed this patch against its parent, including the corresponding Opus findings and the actual node-online writers.
- Zero `d->node_claims` stops the loop before a NULL `d->claims` can be dereferenced. With no claim installer at this revision, the new path leaves existing behavior unchanged.
- The node totals and cached sum are decremented under `heap_lock`; the existing subtraction of `d->outstanding_pages` releases both host-wide and per-node claims from the host total.
- Detachment is protected by `heap_lock`. Freeing after both locks are released avoids recursively acquiring `heap_lock` through `xvfree()`.
- Preserve the previously accepted early-exit loop. The final assertion documents the maintained invariant; it is not intended to diagnose every conceivable corruption of later array entries.

## Can an offline node retain a claim that the release loop misses?

**The suspicion.** Iterating only online nodes could leave a nonzero cached sum, trigger the final assertion, or discard a still-accounted node claim when the array is freed.

**Disposition.** At this revision the only runtime `node_set_offline()` call is the x86 `memory_add()` rollback path. It clears a newly introduced node only on failure before `transfer_pages_to_heap()`; the successful path has crossed its no-rollback point before exposing that memory to allocations. Such a rolled-back node cannot supply free heap memory backing a claim. Other online-map writers establish nodes during initialization. The later installer must continue to reject nonzero claims without available memory.

**Targeted mitigation.** No source change or additional locking. Retain this as a lifecycle qualification rather than claiming that arbitrary future NUMA hot-remove would be supported without revisiting the loop.

## Suggested commit-message diff

None.
