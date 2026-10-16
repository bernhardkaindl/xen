# Reviewer concerns and evidence

These are concise review conclusions and clarification candidates. Patch-local actionable findings and message diffs are recorded in the corresponding review files.

## Can the admission check promise memory that offlining removes?

**The suspicion.** Patch 1 clamps unclaimed pages at zero and then adds the allocating domain's own credit, which can appear to admit an allocation despite overcommitted claims.

**Disposition.** With normal accounting, own credit is part of the total node claim and the admitted allocation preserves other domains' reservations after redemption. Offlining can invalidate the initial free-versus-claimed invariant. Clamping avoids unsigned wrap but does not repair that lost guarantee, as the existing reviewer notes acknowledge.

**Targeted mitigation.** Keep patch 1's existing review-only offlining note. Do not describe the clamp as preserving guarantees after offlining or add a broader safety assertion to the code.

## Can another-node redemption underflow its remainder?

**The suspicion.** Patch 3 demands exact redemption from other nodes after consuming the selected-node and host-wide claims, yet allocations may fall back to a node without a claim.

**Disposition.** Redemption starts with at most the domain's outstanding total. Selected-node redemption reduces the node sum and outstanding total equally, leaving the host-wide portion unchanged. Any remainder after the host-wide portion is consumed cannot exceed remaining node claims. A nonzero remainder also means the actual allocation node's claim was exhausted, so skipping that node is correct even after fallback.

**Targeted mitigation.** The three existing step comments and patch 3's ordered message already explain this. No extra source comment or review-only message paragraph is recommended.

## Does checking creation state close the allocation window?

**The suspicion.** Patch 5 now checks `creation_finished`, which could be mistaken for synchronization with all allocations for the target domain.

**Disposition.** The check rejects new installations after first unpause, but another builder thread can still allocate before creation finishes. Holding `page_alloc_lock` through validation does not cover an allocation already between heap allocation and assignment, including a continuation. The public header explicitly assigns serialization to the builder. Historical reviews of controller pause counts describe a superseded check.

**Targeted mitigation.** Keep the public builder-serialization requirement and the cover-letter explanation. The revised patch 5 changes section now explicitly requires serialization with allocations and continuations to keep claims plus parallel allocations within `d->max_pages` despite deferred assignment. This addresses the clarification candidate without changing the supported use case.

## Can release free an array while get still reads it?

**The suspicion.** Lockless domctl dispatch for get appears to race with explicit release, destruction or first-unpause cleanup freeing `d->claims`.

**Disposition.** Get is lockless only with respect to `domctl_lock`: it holds the domain RCU reference and snapshots the array under `heap_lock`. Release detaches under that same heap lock and frees afterwards. Get copies from its private snapshot, not the detached pointer, after releasing the lock.

**Targeted mitigation.** Existing array protection and detach comments are sufficient. No extra locking or duplicated source commentary is needed.

## Does ENOBUFS overwrite the caller's array or lose the count?

**The suspicion.** The get helper may fill a private prefix before reporting insufficient capacity, while libxc uses an output bounce buffer.

**Disposition.** Xen copies entries only on success and returns the required count through domctl copy-back on success or ENOBUFS. Libxc sets the claims bounce-copy size to zero on every failure and to the returned length on success. A fault during the final domctl copy may be partial and provides no valid-count guarantee.

**Targeted mitigation.** Keep patch 7's existing review-only distinction for final-copy EFAULT. No new code or message note is needed; do not restore the historical claim that every EFAULT preserves the input capacity.

## Does first-unpause cleanup change the legacy claim ABI?

**The suspicion.** The added cleanup also clears `XENMEM_claim_pages` reservations, although that legacy setter has no creation-state restriction.

**Disposition.** Both interfaces share `d->outstanding_pages`, so cleanup intentionally releases either kind of leftover build claim. The x86 libxenguest builder already releases them after meminit. Cleanup is a one-time creation transition, not a new prohibition on later legacy requests; later legacy semantics are unchanged.

**Targeted mitigation.** The author now describes cleanup as an independent submission in the cover letter; preserve that choice. Keep the cleanup message's explicit legacy-interface paragraph and finalize discussion. The new reviewer-note wording needs to say that leftover build claims are released at first unpause, not promise that a running domain can never hold claims; the remaining suggestion is in `11-rereview.md`.
