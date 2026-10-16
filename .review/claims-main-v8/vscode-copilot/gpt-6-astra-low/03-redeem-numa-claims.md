# Patch 3: redeem per-node claims for allocations

No new finding:
- Reviewed this patch before the installer, with the corresponding Opus, Astra and Haiku notes. Preserved the explicitly accepted guard, assertion and count-comment layout.
- Node redemption reduces the inclusive and node totals equally, preserving the host-wide remainder. Host-wide redemption therefore cannot consume another node's claim accidentally.
- A remainder after the first two steps implies that the allocation-node and host-wide claims are exhausted. Since the initial redemption count is bounded by the inclusive outstanding count, the remainder is covered by other-node claims.
- The allocator uses `page_to_nid(pg)`, so fallback redeems the actual allocation node first. With no node claims, the new helpers reproduce the previous host-wide subtraction.
- Failed assignment does not restore redeemed claims. The existing comment and review-only message explicitly rely on aborting the failed domain build.

Verification:
- Source and arithmetic review only; no compile or runtime tests were run.