# Reviewer questions and evidence, 2026-10-15

These are concise review-question summaries and source evidence, not a transcript of internal deliberation. No new implementation defect was established by these checks.

## Can zero-count release bypass authorization?

**The suspicion.** set_memory_claims() handles release before its installation-only state checks.

**Disposition.** do_domctl() calls xsm_domctl() before dispatching either claims operation. The early release is inside that already-authorized handler. FLASK applies DOMAIN2__SETCLAIM to the command without a special exemption for zero count. Native denied-release scenarios preserve claims and accounting. The early return bypasses pause, dying-domain and coloring restrictions, not authorization. Existing comments explain the state-check exception; no additional comment is required.

**Targeted mitigation.** None. The existing comments already explain the state-check exception.

## Does reusing GETDOMAININFO also reuse xenstore's dummy-XSM exception?

**The suspicion.** The new get uses the same FLASK permission as getdomaininfo, but reaches it through the generic domctl hook.

**Disposition.** FLASK explicitly routes the new command to flask_getdomaininfo(). Dummy xsm_domctl() uses XSM_PRIV, whereas the existing getdomaininfo hook uses XSM_XS_PRIV. FLASK permission reuse does not imply identical dummy-XSM privilege rules. The series promises a toolstack query, not access by a xenstore-only domain. Avoid claiming identical authorization across every XSM configuration in reviewer explanations.

**Targeted mitigation.** Keep these distinctions explicit in reviewer explanations; no additional implementation change is proposed.

## Can a get operation observe a freed or half-redeemed array?

**The suspicion.** Get does not take domctl_lock and can overlap release, redemption or domain destruction.

**Disposition.** The surrounding RCU reference keeps the domain object alive. Get snapshots all related counters and entries while holding heap_lock. Writers hold the same lock, and release detaches the pointer before freeing it outside the lock. The output operation uses a private snapshot, not a pointer retained into d->claims. Get observes one coherent claim snapshot, not a promise that a later get or installation sees the same state. The existing fields and snapshot code express that rule; no new ownership comment is needed.

**Targeted mitigation.** None. The existing fields and snapshot code express the ownership rule.

## Does every EFAULT preserve the input count and output array?

**The suspicion.** A get may copy claims before final domctl copy-back fails.

**Disposition.** An array-copy error prevents count copy-back in Xen. Final domctl copy-back may instead write a prefix before failing. Raw hypercall clients can observe partial array copying on EFAULT. libxc stages claims in a separate bounce buffer and copies zero claim bytes to its caller whenever the operation fails. The updated native suite exercises these distinct failure locations. ENOBUFS has a defined required-count and unchanged-array contract. Do not extend its count guarantee to EFAULT. Distinguish raw hypercall guest-memory effects from libxc's caller-buffer effects.

**Targeted mitigation.** Keep these distinctions explicit in reviewer explanations; no additional implementation change is proposed.

## Can a fully redeemed node array contaminate a later legacy claim?

**The suspicion.** Redemption retains the array after its entries and cached sum reach zero.

**Disposition.** The legacy installer sets only the outstanding host-wide total. Zero node entries contribute no node credit and get reports only the new host claim. A new node installation replaces the old zero array under heap_lock. Either interface's zero-count release detaches and frees it. Eight mixed-lifecycle scenarios passed in both native compiler runs. Retention is deliberate lifetime management, not a leak of active claims. The existing zero-array replacement comment already supplies the necessary rationale.

**Targeted mitigation.** None. Keep the existing zero-array replacement comment.
