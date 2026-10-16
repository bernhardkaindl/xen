# 03 - redeem per-node claims for allocations

**Result:** No actionable defect found. No code or commit-message change suggested.

## Code review

- Reviewed the parent-to-patch diff before considering the installer. Without per-node claims, host-wide redemption remains identical to the removed two subtractions.
- The allocation node is the node of the returned page, not merely the requested node; fallback therefore redeems the appropriate claim first.
- Every helper requires `heap_lock`. Node redemption reduces the domain entry, cached sum, domain total, node total, and host total by the same bounded amount.
- Host-wide redemption changes only the domain and host totals. Its subtraction is guarded by `d->outstanding_pages >= d->node_claims`.
- The helper argument widths match the `unsigned long` allocation count. Preserve the existing continuation alignment, adjacent guards, and previously accepted placement of the redemption-count comment.

## Can exact redemption from other nodes underflow or assert after fallback?

**The suspicion.** The third step skips the actual allocation node and asserts complete redemption, including when fallback allocated from a node with no domain claim.

**Disposition.** Let the outstanding total be `O`, the per-node sum `N`, the host-wide part `H = O - N`, and the required redemption `r <= O`. If the first step redeems `a`, it reduces `O` and `N` equally and leaves `H` unchanged. A remainder after the second step is `r - a - H <= N - a`. A nonzero remainder also means the first step exhausted the allocation node's entry, so all remaining node claims are on other nodes. Each subtraction is bounded by its current entry and remaining request.

**Targeted mitigation.** No source change. The existing three comments and ordered commit-message explanation already supply the useful review guidance; duplicating the proof in code would obscure the short helpers.

## Can failed assignment restore the redeemed reservation?

**The suspicion.** `alloc_heap_pages()` redeems claims before `assign_pages()` can reject the allocation, so freeing a rejected allocation does not restore its reservation.

**Disposition.** This is the existing build-time claims model, not a new rollback regression. The intended builder abandons a failed domain build and releases the remaining claims. These helpers do not claim transactional rollback across page assignment.

**Targeted mitigation.** Keep the patch's existing review-only explanation. No additional recovery mechanism is proposed within this series.

## Suggested commit-message diff

None.
