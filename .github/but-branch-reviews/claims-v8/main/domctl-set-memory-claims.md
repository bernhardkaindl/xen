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
findings: []
---
## DSC-1 (info, accepted): Paused domain required for setting claims

Maintainer question: `XENMEM_claim_pages` needs no paused domain, why does
this domctl?

- Because it is not the current case of claims, should make clear
  that claim calls are not supported after it has been unpaused.
