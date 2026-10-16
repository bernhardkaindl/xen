# 00 - Cover letter: xen/mm: introduce per-node NUMA memory claims

**Result:** the cover letter describes the current eight-patch series.

- The interface list matches patches 5 to 8.
- Patches 1 to 4 have no functional effect until patch 5 adds the set domctl.
- Patch 3 documents the redemption count: "Number of claimed pages that
  this allocation redeems."
- Patch 4 compares outstanding claims with the remaining free pages,
  avoiding an addition-overflow dependency.
- Patch 5 documents builder-side serialization. Pausing prevents
  guest-initiated allocations, not allocations by another builder thread.
- Patch 7 documents the set and get meanings of the shared request's
  `nr_entries`; the comment on the sums comes from patch 4.
- The per-patch review notes are separate review metadata, not part of the eight-patch submission.
- The builder-allocation window and memory-offlining limitations remain
  the accepted scope constraints in SCE-1/SCE-2.

## Validation

Compile tests were not rerun. The whitespace checks passed.

The availability predicate was checked with Python integers as an
arbitrary-precision oracle. For each width (32 and 64 bits), the model used
six boundary values: 0, 1, half the unsigned maximum, half plus 1,
maximum minus 1, and maximum. All 6^3 = 216 triples were checked, followed
by 10,000 random triples using a generator seeded with 0.

Thus 2 * (216 + 10,000) = 20,432 comparisons matched the mathematical
rejection condition `request + outstanding > available`. The model
short-circuits when `request > available`, so subtraction is performed
only when it cannot underflow. Full-width values stress the arithmetic;
some exceed the actual request limits.

A separate 32-bit counterexample reproduced the old addition wrapping:
available = 0xf0000000, outstanding = 0xe0000000, request = 0x30000000.
The old unsigned sum wraps to 0x10000000 and accepts; the current
comparison rejects. These are arithmetic-model checks, not execution of
the C function, Xen runtime tests, or proof that the counterexample is
reachable on supported hardware.

## Review cycle: 2026-10-12

- Scope:
  - Reviewed the eight code patches in chronological order and compared the corresponding reports from the other models.
  - No new functional blocker was found within the documented builder-serialization and memory-offlining constraints.

- Review metadata corrections:
  - The reports are separate metadata, not included in the submitted code patches.
  - Removed unsupported nodemask timing and bounce-page safety claims.
  - Distinguished libxc bounce allocation failure from Xen claim-availability failure.
  - Removed the obsolete statement that patch 6's message does not mention its changelog entry.
  - Updated the historical wrapper-documentation finding to reflect the documented pause requirement.

- Non-build validation:
  - All eight code patches and the tracked review edits pass `git diff --check`.
  - An exhaustive mathematical redemption model passed 52,500 cases: three nodes with claims of 0 to 4 pages, a host-wide claim of 0 to 4 pages, allocation sizes of 0 to 20 pages, and allocation from each claimed node or a fourth unclaimed node.
  - The model checked full redemption, nonnegative counters, the cached node sum, the remaining host-wide claim, and preservation of other domains' global claims.
  - The availability predicate matched an arbitrary-precision oracle for 432 boundary triples across 32- and 64-bit widths.
  - These checks model the reviewed arithmetic; they do not execute the C implementation.
  - Compile tests and Xen runtime tests were not rerun.

## Deeper review with test references: 2026-10-13

### Result

No new implementation blocker was established for the documented,
initially paused, serialized domain-build use case. No source-code or test
changes were made in this cycle.

Two earlier source-review claims needed qualification:

- The controller pause count is not exclusively changed under domctl_lock.
  Guest trap/VM-exit paths can increment it through the asynchronous
  debugger-pause path. A positive count is not a generic synchronous
  quiescence proof for an already running domain. Patch 5's review now
  distinguishes that case from an initially paused domain build.
- An EFAULT during claim-array copying suppresses domctl copy-back, but
  EFAULT during the final domctl copy-back follows an attempted, potentially
  partial copy. The old statement that every EFAULT preserves the input
  count was too strong. Patch 8's review now makes the distinction.

### Invariants and lifetime

- On a node with free pages F, total node claims C and the allocating
  domain's claim c, normal admission requires q <= F - C + c.
  Node redemption removes min(c, q). If q <= c, free and claims fall
  equally. Otherwise the admission inequality ensures that freeing no
  other domain's claim is necessary. Other-node redemption only reduces
  claims there and does not remove their free pages.
- Globally, an allocation is admitted if unclaimed pages cover q or the
  allocating domain's outstanding claim covers q. Redemption of min(O, q)
  then preserves free pages >= total outstanding claims. This reasoning
  assumes the invariant held before allocation; it is not a solution to
  the accepted memory-offlining limitation.
- Redemption uses page_to_nid(pg), not the requested node. Requested-node
  fallback therefore consumes the actual allocation node's claim first.
  Fragmentation and address-zone constraints can still prevent allocation:
  claims reserve page counts, not particular contiguous extents.
- Every new claim-array reader and writer is serialized by heap_lock.
  Get snapshots into its own buffer before dropping that lock. Release
  and replacement detach the old pointer before freeing it outside the
  lock, so a concurrent get cannot dereference a freed array.
- Validation and installation retain page_alloc_lock across the max_pages
  check. This excludes concurrent page-count/limit writes during those
  steps, not an allocation already between alloc_heap_pages and
  assign_pages. Builder-side serialization remains necessary.
- A newly onlined node can be rolled back only before its pages reach the
  heap. Positive claims cannot be installed on it in that interval because
  the per-node availability check fails. After transfer_pages_to_heap,
  memory_add has passed its rollback point.
- No-owner allocations imply no-refcount before heap admission and receive
  no claim credit. Failed page assignment returns memory without restoring
  claims, following the existing build-abort assumption.

### Reference tests and verification limits

- The native node-claims tests cover validation boundaries, count/capacity
  queries, ordering, multi-domain protection, redemption sequences,
  requested-node fallback, fragmentation and array lifetime.
- They compile the implementation into a shimmed environment. Real bounce
  buffers, libc errno conversion, guest-copy faults, XSM enforcement,
  destruction and concurrent locking are not established by those tests.
  The corresponding real paths were source-traced where relevant.
- The later mem-claim suite uses real libxc and a running Xen host, but was
  only read in this cycle. No build or runtime result is claimed.
- Each of the eight parent-to-commit diffs passes git diff --check. Added C
  and header lines are below 80 columns, and subjects are at most 72.
- Patch 4 introduces struct claim_request with its comment on the sums;
  patch 7 only adds the set and get meanings of nr_entries.
- Later commits do not change the reviewed allocator, domctl, wrappers or
  associated claim headers. The larger stack therefore does not mask a fix
  to those reviewed implementation files.
- Compile tests, native tests and Xen runtime tests were not run.
