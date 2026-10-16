# Patch 1: protect per-node claims in `get_free_buddy()`

No implementation finding:
- Reviewed the parent-to-patch diff and allocator context at this revision, alongside the corresponding Opus, Astra and Haiku notes.
- Zero-initialized claims make the node-total test redundant with the existing per-zone tests. The patch is behaviorally self-contained.
- `heap_lock` protects admission and buddy selection; anonymous and no-refcount allocations receive no claim credit. Skipping a node preserves exact-node handling.
- The offlining clamp prevents subtraction wrap, not depletion of reservations by memory offlining.

Verification:
- Source inspection only; no compile or runtime tests were run.