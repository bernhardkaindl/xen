---
type: but-branch-review
series: claims-v8/main
branch: claims-v8/main/libxc-get-memory-claims
reviews:
  - date: 2026-10-03T13:06:38+02:00
    model: Claude Sonnet 5.5
    harness: GitHub Copilot (VS Code, Xen Patch Reviewer and Improver)
    effort: high
    verdict: approve
findings:
  - id: LGM-10
    severity: info
    status: accepted
    title: Bounce size can wrap on 32-bit tools
---

# Review: claims-v8/main/libxc-get-memory-claims

## LGM-10 (info, accepted): Bounce size

Maintainer question: can `sizeof(*claims) * *nr` wrap on 32-bit tools, and
does it matter?

- It wraps for `*nr >= 2^28` (16-byte entries, 32-bit `size_t`). Xen clamps
  the capacity, but copies back as many entries as the domain has, so a
  wrapped, tiny bounce buffer could be overrun by the copy-back.
- `*nr` is the capacity of the array the caller passed. On 32-bit tools, an
  array of 2^28 entries is 4 GiB and cannot exist, so a caller with a valid
  array never reaches the wrap.
- The other wrappers in this file do not check their sizes either, and the
  commit message says that the wrappers do not check this.
