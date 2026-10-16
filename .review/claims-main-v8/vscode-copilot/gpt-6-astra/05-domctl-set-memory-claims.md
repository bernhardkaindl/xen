# Patch 5: set-memory-claims domctl

No new blocker for the initially paused domain-build use case:
- The common domctl path obtains a referenced target and performs XSM authorization before dispatch, including zero-count release. FLASK reuses the existing `setclaim` permission.
- Destruction and installation share `domctl_lock`. The dying-domain check prevents installing claims after destruction has released them.
- The count is bounded before allocation; the entry array is copied once into Xen-owned storage before the installer rereads it. Every post-allocation path frees the temporary buffer.
- Release bypasses installation-only state checks but still rejects nonzero operation padding. It is safe after destruction's earlier release.
- The previously identified pause-count qualification is real: `_domain_pause_by_systemcontroller()` increments the count before pausing, and GDBSX uses its asynchronous variant. The public contract restricts installation to domain build, and the handler says the domain is still paused; neither should be generalized to an arbitrary debugger-paused running domain.
- Consulted the corresponding Opus, Haiku and earlier claims-v8 notes. The older unqualified statement that the pause check prevents guest allocations needs the initially paused build qualification, as already recorded by Opus.
- The historical cross-series finding that the public count rename arrived too late is fixed: patch 5 already declares `nr_claims`, matching its handler and patch 6. Patch 7 expands its documentation rather than repairing an earlier missing field.

Suggested review-only message addition:
- "The supported case is a domain build before first unpause. The controller pause count is not a general proof that an asynchronously debugger-paused domain has quiesced; this interface does not extend claims to that use case. Builder-side serialization is still required."
- This is proposed text below the patch-message separator, not an amendment of the commit or an added source restriction.

Verification:
- Source-traced authorization, destruction, buffer ownership and the GDBSX pause helpers. Compile and runtime tests were not run.