---
type: but-branch-review
series: claims-v8/main
branch: claims-v8/main/domctl-set-memory-claims
reviews:
  - date: 2026-10-03T13:06:38+02:00
    model: Claude Sonnet 5.5
    harness: GitHub Copilot (VS Code, Xen Patch Reviewer and Improver)
    effort: high
    verdict: approve
findings:
  - id: DSC-1
    severity: info
    status: accepted
    title: Paused domain is required for setting claims, not for releasing them
---

# Review: claims-v8/main/domctl-set-memory-claims

## DSC-1 (info, accepted): Paused domain required for setting claims

Maintainer question: `XENMEM_claim_pages` needs no paused domain, why does
this domctl?

- Setting claims while the domain allocates memory has the window described
  in DSM-1 (an allocation between `alloc_heap_pages()` and `assign_pages()`
  is not counted). The domain build is the only user, and the domain is
  paused then.
- A restriction can be relaxed later without breaking toolstacks, while a
  missing one cannot be added later, so the stricter rule is kept.
