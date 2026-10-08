---
reviews:
  - model: Claude Opus 5.5, thinking effort: xhigh
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
  - model: Claude Sonnet 5.5, thinking efforts: high, xhigh, and max
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
findings:
  - id: LSM-1
    severity: nit
    status: accepted
    title: Header comment omits the EBUSY failure
---
## LSM-1 (nit, accepted): Header comment omits the EBUSY failure

Maintainer question: the comment in `tools/include/xenctrl.h` does not say that the call fails with `EBUSY` on a domain that is not paused, nor list the other error codes. Should it?

- The comment covers what the wrapper itself does:
  - the entries,
  - the meaning of `target` (a NUMA node, or `XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST`),
  - and that `nr == 0` releases the claims.

- The error codes are documented with the hypercall in `domctl.h`:
  - Repeating them here would duplicate the information,
  - and duplicates can become stale.
