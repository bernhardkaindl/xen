---
reviews:
  - model: Claude Opus 5.5, thinking effort: xhigh
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
  - model: Claude Sonnet 5.5, thinking efforts: high, xhigh, and max
    harness: GitHub Copilot (custom agent: Xen Patch Reviewer and Improver)
    verdict: approve
findings:
  - id: LGM-10
    severity: info
    status: accepted
    title: Bounce size can wrap on 32-bit tools
---

## Code review

- A NULL buffer with nr == 0 is handled by xc__hypercall_bounce_pre().

- HYPERCALL_BOUNCE_SET_SIZE() after the call has precedent in xc_misc.c.

## LGM-10 (info, accepted): Bounce size

Maintainer question: can `sizeof(*claims) * *nr` wrap on 32-bit tools, and does it matter?

- It wraps for `*nr >= 2^28` (16-byte entries, 32-bit `size_t`):
  - Xen clamps the capacity,
  - but copies back as many entries as the domain has,
  - so a wrapped, tiny bounce buffer could be overrun by the copy-back.

- `*nr` is the capacity of the array the caller passed:
  - On 32-bit tools, an array of 2^28 entries is 4 GiB and cannot exist,
  - so a caller with a valid array never reaches the wrap.
