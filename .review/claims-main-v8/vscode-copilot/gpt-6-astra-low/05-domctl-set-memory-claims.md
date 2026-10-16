# Patch 6: set-memory-claims domctl

Restore the builder-serialization contract:
- The current patch replaces the former pause-count test with `creation_finished`, returning `EEXIST` after creation. Older patch-5 reports describe the previous interface and are not evidence for the current state check.
- Initially paused domains cannot allocate for themselves, but their builders can have allocations in flight. Restored the explicit serialization requirement in the canonical public header, including allocation continuations.
- Creation-state restriction is useful independently of automatic first-unpause cleanup. The suggested changelog wording avoids implying a dependency on patch 5.

Authorization and lifetime:
- Common `xsm_domctl()` authorization precedes dispatch and covers zero-count release. FLASK reuses `setclaim`; the dummy implementation uses the privileged default.
- The handler and destruction share `domctl_lock`. Dying-domain rejection prevents reinstallation after destruction's release.
- Bounded guest entries are copied into Xen storage before validation. Every post-allocation path frees the temporary array. Zero-count release bypasses installation-only state restrictions, not authorization or operation-padding validation.
- The domain starts with a controller pause reference, and the first creation-completing unpause marks `creation_finished`. The older concern about asynchronously debugger-paused, previously running domains no longer applies to installation through this domctl.

Verification:
- Compared the header against the current handler and source-traced authorization and creation-state transitions. Whitespace checks passed; compile and runtime tests were not run.