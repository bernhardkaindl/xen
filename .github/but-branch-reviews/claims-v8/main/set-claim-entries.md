---
type: but-branch-review
series: claims-v8/main
branch: claims-v8/main/set-claim-entries
reviews:
  - date: 2026-10-03T13:06:38+02:00
    model: Claude Sonnet 5.5
    harness: GitHub Copilot (VS Code, Xen Patch Reviewer and Improver)
    effort: high
    verdict: approve
findings:
  - id: DSM-1
    severity: info
    status: accepted
    title: No serialization against allocations
  - id: SCE-2
    severity: info
    status: accepted
    title: Memory offlining reducing free memory
---

# Review: claims-v8/main/set-claim-entries

## DSM-1 (info, accepted): No serialization against allocations

Maintainer question: can an allocation in flight while claims are set push
the total pages of the domain over its `max_pages` limit?

- Yes. `assign_pages()` checks `domain_tot_pages()` against `d->max_pages`
  without `d->outstanding_pages`, and the validation of a claim request reads
  `domain_tot_pages()` while an allocation can be between
  `alloc_heap_pages()` and `assign_pages()`. Such a page is not counted.
- This is not new: `domain_set_outstanding_pages()` has the same window.
- It is not a supported case: claims are set for populating the guest memory
  with a single domain builder thread per domain. The domctl requires the
  domain to be paused to make that explicit.
- A fix is to let `assign_pages()` also check `d->outstanding_pages`. When
  it fails, the caller frees the page and the allocation of the domain fails.
  That needs its own review and belongs into a separate submission.

## SCE-2 (info, accepted): Memory offlining reducing free memory

Maintainer question: what if free memory drops below the outstanding claims,
for example when pages are offlined?

- This is not new: `domain_set_outstanding_pages()` has the same problem.
- Maintainers consider it rare enough on the servers where claims are used
  that it is not a problem for now.
- A sysctl to offline memory exists, but offlining is not a supported case
  for this series. It can be fixed later.
- The allocator does not underflow: `get_free_buddy()` clamps the free pages
  of a node at 0 when they are below the claims.
