# claims-main-v8: deeper re-review, 2026-10-15

**Result:** no new actionable finding in the eight-patch claims series.
No source, test, commit-message or cover-letter changes were made.
The conclusion is limited to an initially paused domain build with builder-side serialization and the previously accepted memory-offlining limitation.

## Per-Patch Review

### Patch 1: Protection of claims on NUMA nodes
The early skip reaches the existing exact-node and fallback handling.
Without installed node claims, it only skips nodes whose total free count cannot satisfy the request in any zone.
Under normal accounting, adding the domain's own node credit cannot exceed the node's free count.
When offlining invalidates that accounting, clamping prevents subtraction wrap but does not restore the lost reservation guarantee.

### Patch 2: release and array lifetime.
  - The cached node total prevents NULL-array dereferences on the no-node-claim path.
  - Node claims are subtracted from global per-node totals while the heap lock is held.
  - The old array is detached before unlocking and freed afterwards.
  - A concurrent get copies entries under the same heap lock and cannot retain the detached pointer.
  - The sole located runtime node-offline operation is the x86 hot-add rollback before pages reach the heap; it cannot remove a node holding an admitted positive claim.

### Patch 3: redemption.
  - Redemption uses page_to_nid(pg), not the requested node, so fallback uses the actual allocation node's claim first.
  - Node redemption changes the domain node sum and outstanding total together; host redemption changes only the outstanding total.
  - Any remainder after node and host redemption is bounded by claims on other nodes.
  - Anonymous and no-refcount allocations receive no claim credit and do not redeem claims.
  - Failed assignment frees pages without replenishing claims, following the pre-existing domain-build failure assumption.

### Patch 4: validation and installation.
  - Validation keeps the aggregate within max_pages before storing it in narrower domain fields.
  - Duplicate, reserved, offline and out-of-range targets are rejected before published claim state changes.
  - The host-wide subtraction check cannot underflow because its first condition short-circuits requests larger than the free total.
  - Installation rejects a nonzero outstanding total before replacing the array or updating counters.
  - Failure leaves the candidate private; success returns detached old storage to the caller for freeing.
  - An all-zero retained array does not prevent a subsequent host-only legacy claim or a new node-claim installation.

### Patch 5: set dispatch and authorization.
  - The public count field is nr_claims in this patch; the earlier patch-order defect remains fixed.
  - XSM runs before claim-array access, including for zero-count release.
  - Installation and destruction share domctl_lock; a new node claim cannot be installed after domain_kill() releases the claims.
  - State-specific installation checks do not prevent authorized release.
  - Public redemption wording now distinguishes charged domain-heap allocations from auxiliary pages.

### Patch 6: set wrapper.
  - Zero-initialization supplies zero padding, and input-only bounce handling matches the operation.
  - NULL with zero claims reaches release without bouncing the array.
  - The wrapper uses do_domctl(), not its retry-capable variant, so EFAULT does not silently repeat an installation.
  - No new failure for a valid caller-provided buffer was established in the previously reviewed size-multiplication contract.

### Patch 7: get snapshot and copy-back.
  - Lookup retains an RCU reference to the domain; heap_lock protects the claims snapshot.
  - Guest copying occurs after the snapshot lock is released.
  - The clamped capacity still covers every possible claim entry when the input capacity is sufficiently large.
  - Zero capacity permits counting without dereferencing the entries buffer.
  - A too-small private buffer may contain a prefix, but no claims are copied to the guest on ENOBUFS.
  - Success and ENOBUFS copy back the count; an array-copy EFAULT does not.

### Patch 8: get wrapper.
  - Failure suppresses claims-buffer copy-back, and success copies only the returned entries.
  - This preserves unused caller capacity and the ENOBUFS unchanged-array contract.
  - The original allocation size is not inferred from the reduced bounce-copy size when freeing the real buffer.
  - Final domctl copy-back may fault after a partial write; no valid count is inferred for that EFAULT case.

## Deeper Authorization Check

FLASK routing was source-traced, not inferred from the native callback:
  - Set, including release, calls flask_claim_pages(), which checks SECCLASS_DOMAIN2 with DOMAIN2__SETCLAIM.
  - Get calls flask_getdomaininfo(), which checks SECCLASS_DOMAIN with DOMAIN__GETDOMAININFO.
  - create_domain_common already grants both permissions.
  - The access-vector changes document existing permissions without introducing or reordering permission bits.

Dummy XSM remains distinct:
  - Both new operations use the XSM_PRIV fallback of xsm_domctl().
  - The legacy claim hook also uses XSM_PRIV.
  - The old getdomaininfo operation separately uses XSM_XS_PRIV, so this review does not claim identical dummy-XSM read privileges for the old and new get operations.
  - No privilege bypass was established in the new paths.

## Executed Native Tests

Command:
  - `make -C tools/tests/claims/ TARGETS=node-claims run`

Result:
  - GCC x86_64 build and run passed with AddressSanitizer.
  - Clang x86_64 build and run passed with AddressSanitizer.
  - Each executable completed 19 test functions without a reported sanitizer failure.
  - Both executions produced the same scenario counts below.

| Reported Test Group | Scenarios Per Executable |
| --- | ---: |
| XSM gate | 14 decisions |
| Validation boundaries | 9 |
| Get count/capacity model | 30 |
| Competing claims | 128 |
| Protection model | 3,420 |
| Mixed legacy/new lifecycle | 8 |
| Shared-capacity boundaries | 48 |
| Redemption boundaries | 160 |
| Redemption sequences | 2,072 |
| Requested-node fallback | 96 |
| Fragmentation | 12 |

Additional invoked tests:
  - Guest-copy faults and final-domctl partial-copy faults.
  - Basic redemption, wrapper behavior, protection/lifetime, capacity transitions and rejected installation.
  - The reserved-special-target scenario now uses HOST | 1U, fixing the previously identified duplicate-HOST coverage gap.

Current harness capabilities:
  - Includes the production claim allocator, domctl handlers, wrappers and libxc bounce pre/post implementation.
  - Uses host-backed allocations and a test hypercall transport that converts negative Xen errors into -1 with errno.
  - Injects guest-copy and final-domctl-copy faults and checks authorization placement with a decision callback.
  - The historical alias-buffer/no-op-bounce limitations in older reports no longer describe this harness.

Limits of that evidence:
  - Arithmetic fixtures select boundary representatives and finite state combinations, not every machine-width value or concurrent interleaving.
  - Locking, scheduling, domain lookup and guest-address translation still depend on test substitutions.
  - The XSM callback does not execute FLASK permission lookup or policy enforcement.
  - The production xencall allocation backend and a live Xen host are not exercised.
  - GCC and Clang passing do not independently prove the correctness of the test oracle.

## Verification And Remaining Scope

Per-patch source checks:
  - All eight parent-to-commit diffs pass git diff-tree --check.
  - Added C/header lines are below 80 columns.
  - The public count member is consistently nr_claims from patch 5 onward.
  - Both claims structures use aligned fixed-width public fields; domctl already uses the common handler for compatibility dispatch.

Existing limitations remain unchanged:
  - Builder serialization must cover pages awaiting assignment, including a pending scrub continuation.
  - A positive controller pause count is not a general synchronous-quiescence proof for an asynchronous debugger stop.
  - Offlining can reduce capacity below outstanding claims.
  - Claims reserve page counts, not contiguous extents or every requested address zone.
  - Arbitrary grant/sharing ownership changes are not certified by this ordinary domain-build review.

Not performed:
  - No full Xen builds, all-architecture builds, per-commit builds or live Xen system tests.
  - No production FLASK runtime enforcement test.
  - No commits, staging, branch switches or history edits.
  - Only this cycle's review documents were added.