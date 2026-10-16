# Patch 4: install NUMA memory claims

No new finding within the documented domain-build scope:
- Validation bounds every incremental sum by `max_pages` before narrowing to the domain's unsigned-int counters. The final limit check first handles an existing page count above the limit, avoiding subtraction underflow.
- Reserved targets, duplicate targets, nonzero padding, zero-sized entries and invalid nodes are rejected before installation. The caller still needs to bound and snapshot the entry array.
- The global availability check uses a guarded subtraction. Successful admission bounds the subsequent global addition by free memory; per-node increments are bounded by the corresponding availability check.
- All published counters and the array pointer change under `heap_lock`, after checks succeed. The caller frees either the unused allocation or the detached old array after unlocking. A fully redeemed old array contains only zero entries.
- `page_alloc_lock` protects validation against page-count and limit changes while held. It does not account for an allocation already between heap removal and assignment. The documented builder-serialization requirement remains necessary.
- Hot-add rollback removes a newly online node only before `transfer_pages_to_heap()`. Its zero availability prevents a positive claim before that point, closing the online-node-loop concern from patch 2.
- Consulted the corresponding Opus, Haiku and earlier claims-v8 notes. Preserve the accepted shared mutable request type and the deferred allocation/offlining work.

Suggested review-only message addition:
- "The caller must serialize claim installation with the complete allocation operation, including any scrub continuation. `page_alloc_lock` does not include pages already removed from the heap and awaiting assignment in `pending_scrub`."
- The cross-series source check confirms that `populate_physmap()` resumes the same stashed extent without redeeming twice. This sharpens the existing serialization requirement rather than proposing a new one.

Verification:
- Inspected installer arithmetic, ownership, `assign_pages()` and hot-add rollback at this patch revision. No compile or runtime tests were run.