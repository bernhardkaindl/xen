# Patch 3: redeem per-node claims for allocations

No new finding:
- Checked the node, host-wide and other-node steps against the inclusive outstanding count. Node redemption reduces both inclusive and node totals by the same amount, leaving the host-wide remainder unchanged.
- A remainder after the host-wide step implies exhaustion of the allocation-node claim and host-wide claim. It cannot exceed the remaining other-node claims, which justifies the final assertion.
- The allocation node is obtained from `page_to_nid(pg)`, not the requested node. Fallback therefore redeems the claim where pages actually came from first.
- A failed assignment still loses redeemed claims, as explicitly documented under the existing domain-build-abort assumption. This is not transactional restoration of claims.
- At this revision the node counters remain zero; host-wide redemption preserves the previous behavior.
- Consulted the corresponding Opus, Haiku and earlier claims-v8 notes and retained the author-approved guard, assertion and comment layout.

Verification:
- Source and arithmetic review; compile and runtime tests were not run.