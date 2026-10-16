---
commit: xen/domctl: add XEN_DOMCTL_set_memory_claims
reviews:
  - model: Claude Opus 5.5, thinking effort: xhigh
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
  - model: Claude Sonnet 5.5, thinking efforts: high, xhigh, and max
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
findings: []
---
Maintainer question: `XENMEM_claim_pages` needs no paused domain, why does this domctl?

- It is just not the supported use case:
  - and the code should make that clear.

- `XENMEM_claim_pages` is a legacy hypercall:
  - which we do not want to change.
