# Native claims tests

## Lifecycle and policy coverage must remain qualified

**The suspicion.** The explicit-state lifecycle and injected-XSM tests could be presented as exercising production first-unpause cleanup or actual security policy.

**Disposition.** `test_claims_after_creation()` sets completed-creation state directly.
XSM tests inject hook results.
Pointer and prefix-copy fault models do not exercise real protected-page faults.

**Targeted mitigation.** Retain the explicit-state fixture, but use production-helper or hypervisor integration coverage for lifecycle completion and actual policy and fault-containment behavior.

## Handler configuration gates need distinct coverage

**The suspicion.** Broad helper and arithmetic sweeps do not cover every public-handler state and configuration gate.

**Disposition.** Command padding and count limits, dying-domain rejection and LLC-coloring configurations are not all covered by those sweeps.
Helper correctness alone cannot establish their behavior.

**Targeted mitigation.** Add focused handler-level cases for these gates without claiming that existing native models cover concurrent production execution.
