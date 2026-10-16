# 00 - cover letter for per-node NUMA memory claims

**Result:** The cover letter matches the eight-patch submission and intentionally treats creation cleanup as independent work. Applied one small wording correction without renumbering or expanding the series.

## Review

- The set/get interface description matches patches 5-8, including atomic installation of multiple node claims.
- Patches 1-4 do not install per-node claims; their no-functional-effect explanation remains accurate until the set domctl introduces a caller.
- The current creation-state description supersedes historical pause-count wording. The builder-serialization sentence is retained; the public header explicitly includes completion of allocation continuations.
- The test and production-deployment statements are the author's existing reports, not new executions or independent deployment verification by this cycle.
- Per-node unclaimed-memory reporting, placement reporting, tests, and creation cleanup remain separate submissions. Review metadata and test-branch prerequisites are not part of the eight-patch upstream series.
- Replaced the awkward phrase `on domain creation finish` with `when domain creation finishes`, and named the released claims as leftovers, consistently with the independent cleanup patch.

## How does the toolstack discover unclaimed memory per node?

**The suspicion.** Existing NUMA free-memory reporting could be mistaken for the capacity still available after other domains' per-node claims.

**Disposition.** `XEN_SYSCTL_numainfo` sets `memfree` from `avail_node_heap_pages()`, which reports free heap pages including claimed pages. It does not subtract `node_claimed_pages[]`. The set operation checks actual unclaimed capacity atomically and can reject a placement selected from an earlier free-memory query. Any query would still be advisory because availability can change before installation.

**Targeted mitigation.** Keep the existing Related work paragraph that explicitly defers claimed/unclaimed per-node reporting to a separate submission. No new query ABI, implied reservation guarantee, or extra source change belongs in this series.
