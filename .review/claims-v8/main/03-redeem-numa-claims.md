---
reviews:
  - model: Claude Opus 5.5, thinking effort: xhigh
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
  - model: Claude Sonnet 5.5, thinking efforts: high, xhigh, and max
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
findings: []
---
## RNC-1 (info, accepted): Node offlining has to handle claims

Maintainer question: what happens to the claims on a node that goes offline?

- Offlining nodes is not implemented in Xen:
  - so there is nothing to handle today.

- `domain_redeem_other_node_claims()` and the release loop of patch 2 use `for_each_online_node()`:
  - so they skip offline nodes.
  - With claims left on an offlined node, they would not find them,
  - and `ASSERT(!pages)` or `ASSERT(!d->node_claims)` would fail.

- An implementation of node offlining would have to release the claims on the offlined node:
  - so this belongs to that future change.
