# 06 - add `xc_domain_set_memory_claims()`

**Result:** No wrapper behavior defect found. Applied a focused API-documentation correction; no commit-message change suggested.

## Code review

- The input-only bounce direction matches the domctl contract, including the `const` input array.
- Zero-initializing `domctl` initializes both padding fields. The wrapper preserves the ordinary libxc return and `errno` convention and releases its bounce buffer after the domctl.
- `claims == NULL` with `nr_claims == 0` is the release path. The wrapper does not require a usable claims-array handle for that operation.
- Array-before-count order matches nearby newer libxc APIs. No symbol export list or interface-version change is needed for this addition.
- The changelog entry belongs in the existing unreleased Added section and describes the new reservation capability without claiming a placement guarantee.

## Should this review reopen the accepted libxc size-multiplication concern?

**The suspicion.** `sizeof(*claims) * nr_claims` can wrap for invalid enormous counts on 32-bit tools.

**Disposition.** No new valid-caller failure or trust boundary is introduced here. A valid caller supplies the stated array capacity, and the set handler rejects oversized requests before copying entries. The accepted historical decision was to retain the surrounding libxc caller-buffer convention rather than harden a single wrapper inconsistently.

**Targeted mitigation.** No code change or new blocker. Library-wide invalid-input hardening would require a separate, consistently scoped change.

## Suggested commit-message diff

None.
