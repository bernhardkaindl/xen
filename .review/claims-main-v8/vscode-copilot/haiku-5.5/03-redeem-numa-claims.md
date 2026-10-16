# 03 - xen/mm: redeem per-node claims for allocations

**Result:** no open findings.
- Redemption consumes the selected node's claim, then the host-wide claim,
  then claims on other nodes.
- Node redemption changes all five relevant counters; host-wide redemption
  changes only the domain and global outstanding totals.
- The other-node remainder is bounded by the remaining node claims, so
  ASSERT(!pages) states an invariant, not an input check.
- "Exactly" distinguishes full redemption from the helpers that redeem
  "up to" the requested amount.
- The retained count explanation starts with a capital letter.
- The short adjacent guards and established layout are preserved.
