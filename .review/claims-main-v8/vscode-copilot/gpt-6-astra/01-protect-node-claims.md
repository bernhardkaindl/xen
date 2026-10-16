# Patch 1: protect per-node claims in `get_free_buddy()`

No new finding:
- Reviewed the patch against its parent and consulted the corresponding Opus, Haiku and earlier claims-v8 reviews.
- The new counters are zero and the domain pointer is NULL until a later interface can install claims. The node-total precheck cannot reject a block that the existing per-zone check could allocate at this revision.
- `heap_lock` covers the availability calculation and buddy selection. Anonymous and `MEMF_no_refcount` allocations receive no claim credit.
- Skipping a node reaches the existing exact-node failure handling. The clamp prevents unsigned subtraction wrap, not loss of the reservation guarantee after memory offlining.

Verification:
- Source inspection only at this stage; no compile or runtime result is claimed.