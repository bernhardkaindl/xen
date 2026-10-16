# 05 - add `XEN_DOMCTL_set_memory_claims`

**Result:** No current functional or bisectability finding. The older public-member mismatch is fixed in this revision. No source or commit-message change suggested for this patch.

## Code review

- Reviewed the patch's original diff, its domctl dispatch, the first-unpause writer of `creation_finished`, and the reused FLASK helper.
- The public structure declares `nr_claims` in this patch, and both this handler and the next wrapper use that spelling. The later get patch is not required to repair the declaration.
- Domain lookup retains the target under RCU. `xsm_domctl()` authorizes the operation before the lock-protected dispatch reaches the helper, including zero-count release.
- Installation and destruction run under the domctl lock, preventing installation after `domain_kill()` releases the domain's claims. First unpause uses the same lock for the creation transition.
- Padding is checked before release. A zero count routes directly to release, without applying installation-only LLC-coloring, dying-domain, size, or creation-state restrictions.
- Nonzero input is bounded by `MAX_NUMNODES + 1U`, copied once into private Xen storage, and freed on every path. Validation therefore cannot reread a guest-mutated target or page count.
- FLASK calls `flask_claim_pages()`, which checks `SECCLASS_DOMAIN2` / `DOMAIN2__SETCLAIM`. The access-vector comment includes the new domctl, without changing the existing permission ABI.
- The public error list matches the current creation-state check, including `EEXIST`, rather than the superseded pause-count check.

## Does the previous intermediate `nr_entries` declaration still break patches 5 and 6?

**The suspicion.** An earlier extra-high-effort review found that patch 5 declared the public member as `nr_entries` while its own handler and patch 6 already used `nr_claims`, with the declaration repaired only by patch 7.

**Disposition.** That is historical, not a current blocker. At both this patch and patch 6 the public declaration already contains `uint32_t nr_claims`, and no `nr_entries` member remains in that structure. The internal `claim_request.nr_entries` is intentionally separate. A compile-free source check confirmed the declaration at both revisions.

**Targeted mitigation.** No edit. Do not carry the historical finding forward or rename the internal request count merely to make the two structures look identical.

## Does creation-state gating replace builder-side serialization?

**The suspicion.** Rejecting installation after first unpause could be read as eliminating every concurrent-allocation race, or the older debugger-pause concern could be mistaken for a defect in the current check.

**Disposition.** The current implementation does not inspect `controller_pause_count`; it rejects a domain whose creation has finished. This excludes previously running domains even if later paused. It still does not serialize another builder's allocations or their unfinished continuations. The public header and this patch's review-only changes already state the builder's serialization responsibility.

**Targeted mitigation.** Keep those existing qualifications. No additional review-only paragraph is recommended; repeating the superseded pause-count discussion would make the current patch harder to review.

## Can zero-count release bypass authorization?

**The suspicion.** The helper processes release before the installation-only state checks and can release claims for a dying or already-created domain.

**Disposition.** The early return is inside an already authorized domctl dispatch. It bypasses state restrictions, not `xsm_domctl()`. The same `setclaim` permission governs installation and release under FLASK.

**Targeted mitigation.** No edit. The existing release comment accurately explains why state restrictions do not apply.

## Suggested commit-message diff

None.
