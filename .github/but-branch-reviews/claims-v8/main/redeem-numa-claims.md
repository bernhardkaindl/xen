---
type: but-branch-review
series: claims-v8/main
branch: claims-v8/main/redeem-numa-claims
reviews:
  - date: 2026-10-03T13:06:38+02:00
    model: Claude Sonnet 5.5
    harness: GitHub Copilot (VS Code, Xen Patch Reviewer and Improver)
    effort: high
    verdict: approve
findings: []
---
# Review: claims-v8/main/redeem-numa-claims
## RNC-1 (info, accepted): Node offlining has to handle claims

Maintainer question: what happens to the claims on a node that goes offline?
- Offlining nodes is not implemented in Xen, so there is nothing to handle
  today.
- `domain_reduce_node_claims()` uses `for_each_online_node()` to loop over the
  nodes, so it skips offline nodes. With claims left on an offlined node, it
  would not find them and its final `ASSERT(released == pages)` would fail.
- An implementation of node offlining would have to release the claims on
  the offlined node, so this belongs to that future change.
