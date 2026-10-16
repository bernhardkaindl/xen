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
    title: Detailed errno list remains in the public hypercall header
---
## LSM-1 (nit, accepted): Detailed errno list remains in the public hypercall header

The current wrapper comment documents the paused-domain requirement.
The old concern that this usage restriction was omitted is obsolete.

- The comment covers what the wrapper itself does:
  - the entries,
  - the meaning of `target` (a NUMA node, or `XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST`),
  - that installing claims requires the domain to be paused,
  - and that `nr == 0` releases the claims.

- The error codes are documented with the hypercall in `domctl.h`:
  - Repeating them here would duplicate the information,
  - and duplicates can become stale.
