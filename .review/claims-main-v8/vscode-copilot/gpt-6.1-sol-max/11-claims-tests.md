# 11 - claims native-test follow-up

**Result:** Found and fixed one stale lifecycle test in the existing test submission. No production behavior change and no new test file.

## Existing coverage inspected

- [The node tests](../../../../tools/tests/claims/node-claims.c) already cover validation boundaries, admission limits, other-domain protection, node/host/other-node redemption, retained-array reuse, and mixed legacy/new claims. The unknown-special-target case now uses `HOST | 1U`, not a duplicate HOST target.
- [The copy-fault tests](../../../../tools/tests/claims/claims-guest-copy.h) exercise real wrapper/bounce code in the native environment, ENOBUFS preservation, synthetic entries-copy failures, and final-domctl partial-copy failures before, within, and after the returned count.
- [The XSM tests](../../../../tools/tests/claims/claims-xsm-denials.h) inject decisions for the actual domctl authorization call, including rejected release and rejected count queries. They do not execute FLASK policy.
- The native framework substitutes lifecycle, scheduling, locks, guest-address translation, and allocation backing. These tests cannot certify production first-unpause cleanup or concurrent interleavings.

## Does the running-domain test still exercise the implemented lifecycle gate?

**The suspicion.** `test_claims_on_running_domain()` unpaused through the native shim and then expected `EBUSY`, although the current handler checks `creation_finished` and returns `EEXIST`.

**Disposition.** This was a real stale-test defect. The native `domain_unpause_by_systemcontroller()` macro only decrements `controller_pause_count`; it never sets `creation_finished` and never executes the release in the production domain lifecycle code. The fixture's earlier claim therefore remains installed and a subsequent request reaches the existing-outstanding-claims check, not the intended post-creation rejection. The obsolete EBUSY expectation cannot validate the current handler.

**Targeted mitigation.** Replaced it with `test_claims_after_creation()` in [the existing host tests](../../../../tools/tests/claims/host-claims.c). The fixture first installs and releases a build claim, then explicitly sets the completed-creation state. It verifies EEXIST with no outstanding claim, successful later legacy installation, preservation of that legacy claim on a rejected new-interface install, and successful new-interface zero-count release. It deliberately does not test or emulate production first-unpause cleanup. Static handler/fixture checks and whitespace checks passed; the editor reports no diagnostics. The C test was not compiled or run.

## Execution constraint

No already-built native binaries or saved outputs were present. The [native rules](../../../../tools/tests/include/native/Rules.mk) make `run` depend on a clean build and remove binaries after recording their output, so invoking that target would violate this cycle's no-compile instruction. No make target or compiler was invoked. A later authorized run should include both `host-claims` and `node-claims`; the changed lifecycle case is in `host-claims`, not the node-only target used by some historical reports.
