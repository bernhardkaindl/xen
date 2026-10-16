# Reviewer concerns and evidence

## Does the node check change allocation before claims can be installed?

**The suspicion.** Patch 1 adds a rejection before the existing per-zone buddy search, so its preparatory status needs justification independent of later patches.

**Disposition.** At this revision, `node_claimed_pages[]` is zero and `d->claims` is NULL. `node_avail_pages[]` counts all free pages on the node; if it cannot cover the allocation, no individual zone can cover it either. The new rejection preserves the existing result and exact-node handling.

**Targeted mitigation.** No additional comment is needed: patch 1's commit message already gives this argument.

## Does clamping availability preserve claims after memory offlining?

**The suspicion.** Patch 1 clamps `F - C` at zero and adds the allocating domain's node claim; this can look like a complete solution when free memory falls below claims.

**Disposition.** The clamp only prevents unsigned wrap. The zone checks still prevent allocation of nonexistent pages, but offlining can leave aggregate reservations larger than available memory. Claim admission guarantees assume the reservation invariant has not already been broken by offlining.

**Targeted mitigation.** Keep the limitation explicit in reviewer-facing discussion. Patch 1's existing review-only note already describes the underflow guard; do not turn it into a claim that offlining preserves reservations.

## Can other-node redemption exhaust its loop with pages still owed?

**The suspicion.** Patch 3 finishes with `ASSERT(!pages)` after visiting other online nodes, which would be unsafe if earlier redemption steps could leave an uncovered remainder.

**Disposition.** Let the inclusive outstanding count be `O`, node total `N`, host-wide remainder `H = O - N`, and requested redemption `r <= O`. Redeeming `a` on the allocation node reduces `O` and `N` equally. A remainder after the host-wide step is `r - a - H <= N - a`; the allocation-node claim is already exhausted, so those remaining claims are on other nodes. This relies on positive claims remaining on online nodes, checked with the installer below.

**Targeted mitigation.** No extra implementation comment is needed: the existing step comments explain the ordering. This bounded-remainder argument is available for a maintainer question without adding algebra to the production code.

## Does the new assignment guard make concurrent claim installation safe?

**The suspicion.** Patch 4 now includes outstanding claims in `assign_pages()` and its message could suggest that this removes the domain builder's serialization obligation.

**Disposition.** The guard prevents assignment from exceeding the inclusive limit, but heap removal and claim redemption happen earlier. Scrub continuations can retain an unassigned allocation in `pending_scrub`. The check cannot turn these separate operations into an atomic reservation transaction. Its new read also needs `heap_lock`, because allocator redemption does not take `page_alloc_lock`; the applied fix takes a locked snapshot in the established order. While assignment holds `page_alloc_lock`, subsequent redemption can only make that snapshot conservative.

**Targeted mitigation.** The patch 4 review proposes a commit-message diff explaining the snapshot and retaining the full-allocation serialization requirement, including scrub continuations. No claim is made that the arithmetic boundary counterexample is a reachable domain state.

## Is creation-finished cleanup required by per-node claims?

**The suspicion.** Inserting cleanup as patch 5 can suggest that the NUMA claims interfaces depend on it for correct accounting or safe domain startup.

**Disposition.** It uses an interface already present before the series. Patch 2 extends that interface to per-node claims, so explicit cancellation and destruction already release them. The x86 builder cancels claims after memory initialization. Automatic first-unpause cleanup is independently useful lifecycle policy for builders that omit cancellation, not a new accounting dependency introduced by per-node claims.

**Targeted mitigation.** Recommend submitting this patch independently. The patch 5 review contains a review-only message diff explaining that independence; no history changes were made. If omitted from the series, restore eight-patch references in the cover letter while retaining the separate correction about patch 4's assignment behavior.

## Is the old asynchronous-pause concern still relevant to installation?

**The suspicion.** Earlier reviews correctly noted that a positive controller pause count does not by itself prove that an asynchronously debugger-paused domain has stopped running.

**Disposition.** The current patch 6 no longer uses that test. It rejects installation once `creation_finished` is true, so the previously running domain in that counterexample is excluded. A domain still in creation retains its initial pause reference. The state test does not serialize allocations made for it by its domain builder.

**Targeted mitigation.** Restored the builder-serialization requirement in the public header and corrected the cover letter's description of the state check. The patch 6 notes supersede the old pause-count finding for the current implementation without changing the legacy interface.

## Does sharing the FLASK permission grant xenstore access too?

**The suspicion.** Patch 8 describes reuse of `getdomaininfo`, which could be read as inheriting every access exception of the existing domain-information domctl.

**Disposition.** FLASK calls `flask_getdomaininfo()` for the new operation, but dummy XSM authorizes it through the ordinary `xsm_domctl(XSM_PRIV, ...)` path. The older getdomaininfo command has a separate `xsm_getdomaininfo(XSM_XS_PRIV, ...)` dispatch. Reusing the FLASK permission does not copy that dummy-XSM exception.

**Targeted mitigation.** Keep the commit message explicitly qualified with "Under FLASK". The proposed message correction explains the existing aggregate disclosure without claiming identical authorization in all XSM configurations.

## Does every get-side `EFAULT` leave the caller's count unchanged?

**The suspicion.** Get dispatch enables count copy-back only on success or `ENOBUFS`, which can appear to guarantee an unchanged count on any `EFAULT`.

**Disposition.** An array-copy failure prevents the count-copy attempt. However, after a successful snapshot or `ENOBUFS`, the final domctl copy itself can fault after a partial write. That second failure cannot guarantee preservation of the input count, even though the returned error is also `EFAULT`.

**Targeted mitigation.** No production change is needed. Keep this distinction in the review notes and do not document a valid or unchanged output count for all fault paths.

## Can node removal strand claims outside the release loop?

**The suspicion.** Release and other-node redemption iterate online nodes, so a positive claim on a node removed from the online map would escape those loops.

**Disposition.** The inspected x86 hot-add rollback clears a newly online node only before its pages reach the heap. Positive claims cannot pass per-node admission before that point. Once `transfer_pages_to_heap()` is reached, hot-add has passed its rollback point. Offlining individual pages is a different operation and remains a limitation even while the node stays online.

**Targeted mitigation.** No source change is needed for the current node lifecycle. Future node-removal support must explicitly handle claims; the present reasoning must not be treated as a general hot-remove guarantee.