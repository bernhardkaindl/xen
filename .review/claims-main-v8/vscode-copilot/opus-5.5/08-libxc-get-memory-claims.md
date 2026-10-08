# 08 - libxc: add xc_domain_get_memory_claims()

**Result:** the code is correct.

## Code review

- `DECLARE_HYPERCALL_BOUNCE(..., XC_HYPERCALL_BUFFER_BOUNCE_OUT)` sizes the buffer from the input capacity `*nr`:
  - so `nr` must not be NULL, which the in/out contract in `xenctrl.h` implies.
  - Nothing is copied in.

- `claims == NULL` skips the bounce (`xc__hypercall_bounce_pre()`):
  - so `*nr == 0` with `claims == NULL` is the count query.
  - Xen returns 0 and `*nr == 0` for a domain without claims,
  - or ENOBUFS and the count.

- `struct xen_domctl domctl = {}` zeroes `pad`:
  - libxc's `do_domctl()` bounces the domctl in both directions, also on failure,
  - and Xen sets `copyback` on success, ENOBUFS and EFAULT (patch 7), so `*nr` is the count after these.
  - After an early failure (ESRCH, EPERM, EINVAL, ENOMEM) Xen does not copy back:
    - so `*nr` keeps the input capacity.

- `HYPERCALL_BOUNCE_SET_SIZE(claims, ret ? 0 : sizeof(*claims) * *nr)` copies back nothing on failure and only the returned entries on success:
  - so "claims is not modified" holds on ENOBUFS.
  - On success, Xen returned at most the clamped capacity:
    - so the copy stays within the bounce buffer.
  - `xencall_free_buffer()` frees by the page count stored in its header, so the smaller size is safe.
  - Precedent for adjusting the size before the post call: `xc_misc.c` and the affinity wrappers.

- The 32-bit wrap of `sizeof(*claims) * *nr` was accepted earlier (LGM-10):
  - Xen writes at most `MAX_NUMNODES + 1` entries,
  - which fit in the page that the allocation header always uses.

- `xenctrl.h`:
  - the comment documents the in/out `*nr`, ENOBUFS, "claims is not modified" and the count query.
  - The order of the entries stays in `domctl.h`, as the `code-review` skill asks.
  - The prototype needs three lines:
    - because `xen_domctl_memory_claim_t *claims, uint32_t *nr);` after the open parenthesis would end at column 81.
  - The set prototype is laid out the same way.

- CHANGELOG.md:
  - the new sub-item follows the set item,
  - and fits in 74 columns.

- Without `HYPERCALL_BOUNCE_SET_SIZE()`, the whole capacity would be copied back:
  - from the zero-filled bounce buffer,
  - overwriting the caller's array beyond the returned entries,
  - and, on ENOBUFS, the whole array with zeros.

## Changes applied

- The hunk that rewrote the definition comment of `xc_domain_set_memory_claims()` moved to patch 6:
  - it changed code of an earlier patch of the series (see 06).

```diff
-/* Atomically install or release a domain's claims. */
+/* Set a domain's host-wide memory claim and its per-node memory claims. */
```
