---
reviews:
  - model: Claude Opus 5.5, thinking effort: xhigh
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
  - model: Claude Sonnet 5.5, thinking efforts: high, xhigh, and max
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
findings:
  - id: SCE-1
    severity: info
    status: accepted
    title: No serialization against allocations
  - id: SCE-2
    severity: info
    status: accepted
    title: Memory offlining reducing free memory
  - id: SCE-3
    severity: nit
    status: accepted
    title: claim_set.claim is not const
---
# Review: claims-v8/main/set-claim-entries
## SCE-1 (info, accepted): No serialization against allocations

Maintainer question: can an allocation in flight while claims are set push the total pages of the domain over its `max_pages` limit?

- Yes:
  - `assign_pages()` checks `domain_tot_pages()` against `d->max_pages` without `d->outstanding_pages`,
  - and the validation of a claim request reads `domain_tot_pages()`,
  - while an allocation can be between `alloc_heap_pages()` and `assign_pages()`.
  - Such a page is not counted.

- This is not new:
  - `domain_set_outstanding_pages()` has the same window.

- It is not a supported case:
  - claims are set for populating the guest memory with a single domain builder thread per domain.
  - The domctl requires the domain to be paused to make that explicit.

- A fix is to let `assign_pages()` also check `d->outstanding_pages`:
  - When it fails, the caller frees the page and the allocation of the domain fails.
  - That needs its own review and belongs into a separate submission.

## SCE-2 (info, accepted): Memory offlining reducing free memory

Maintainer question: what if free memory drops below the outstanding claims, for example when pages are offlined?

- Code for per-node claims handles it by protecting against underflow:
  - `get_free_buddy()` clamps the free pages of a node at 0 when they are below the claims.

- A sysctl to offline memory exists:
  - but offlining is not supported by the toolstacks used in the target environments (XenServer, XCP-ng),
  - and offlining memory due to machine checks is limited in size.

- `domain_set_outstanding_pages()` has the same problem for overall claims.

- A separate patch series is planned to address this problem.

## SCE-3 (nit, accepted): claim_set.claim is not const

Maintainer question: this patch only reads the entries through `struct claim_set`'s `claim`. Should it point to `const`?

- `domain_validate_claim_request()`, `check_memory_for_claim_request()` and `domain_install_claims()` only read `request->claim[]`:
  - `request` itself must stay non-const,
  - because the validation writes `total` and `node_pages`.

- The struct is shared with `XEN_DOMCTL_get_memory_claims`:
  - where `domain_get_claim_entries()` fills the entries through `claim`.
  - There it must be non-const,
  - so a `const` here would be removed again.

- The same non-const struct serves both directions:
  - which avoids a second type for the same data.
