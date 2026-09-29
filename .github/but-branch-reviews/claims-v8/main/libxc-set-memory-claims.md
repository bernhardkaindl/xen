---
type: but-branch-review
series: claims-v8/main
branch: claims-v8/main/libxc-set-memory-claims
reviews:
  - date: 2026-10-03T13:06:38+02:00
    model: Claude Sonnet 5.5
    harness: GitHub Copilot (VS Code, Xen Patch Reviewer and Improver)
    effort: high
    verdict: approve
findings:
  - id: LSM-1
    severity: nit
    status: accepted
    title: Header comment omits the EBUSY failure
---

# Review: claims-v8/main/libxc-set-memory-claims

## LSM-1 (nit, accepted): Header comment omits the EBUSY failure

Maintainer question: the comment in `tools/include/xenctrl.h` does not say
that the call fails with `EBUSY` on a domain that is not paused, nor list the
other error codes. Should it?

- The comment covers what the wrapper itself does: the entries, the meaning
  of `target` (a NUMA node, or `XEN_DOMCTL_CLAIM_MEMORY_HOST`) and that
  `nr == 0` releases the claims.
- The error codes are documented with the hypercall in `domctl.h`. Repeating
  them here would duplicate the information, and duplicates can become stale.
