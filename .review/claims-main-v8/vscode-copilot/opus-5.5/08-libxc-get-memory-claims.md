# 08 - libxc: add xc_domain_get_memory_claims()

**Result:** the code is correct.

## Code review

- `DECLARE_HYPERCALL_BOUNCE(..., XC_HYPERCALL_BUFFER_BOUNCE_OUT)` sizes the buffer from the input capacity `*nr_claims`:
  - so `nr` must not be NULL, which the in/out contract in `xenctrl.h` implies.
  - Nothing is copied in.

- `claims == NULL` skips the bounce (`xc__hypercall_bounce_pre()`):
  - so `*nr_claims == 0` with `claims == NULL` is the count query.
  - Xen returns 0 and `*nr_claims == 0` for a domain without claims,
  - or ENOBUFS and the count.

- `struct xen_domctl domctl = {}` zeroes `pad`:
  - libxc's `do_domctl()` bounces the domctl in both directions, also on failure,
  - and Xen sets `copyback` only on success and ENOBUFS (patch 7), so `*nr_claims` is the count after these.
  - Errors before copy-back (ESRCH, EPERM, EINVAL, ENOMEM, or EFAULT while
    copying the claim array) leave `copyback` false:
    - so `*nr_claims` keeps the input capacity.
  - EFAULT from the final domctl copy-back is a different case: the copy
    was attempted and may have been partial. The earlier blanket statement
    that every EFAULT preserves the input count was too strong.
    No valid count is promised on EFAULT. The wrapper still copies zero
    entries to the caller's claims buffer whenever the operation fails.

- `HYPERCALL_BOUNCE_SET_SIZE(claims, ret ? 0 : sizeof(*claims) * *nr_claims)` copies back nothing on failure and only the returned entries on success:
  - so "claims is not modified" holds on ENOBUFS.
  - On success, Xen returned at most the clamped capacity:
    - so the copy stays within the bounce buffer.
  - `xencall_free_buffer()` frees by the page count stored in its header, so the smaller size is safe.
  - Precedent for adjusting the size before the post call: `xc_misc.c` and the affinity wrappers.

- The 32-bit wrap of `sizeof(*claims) * *nr_claims` was accepted earlier (LGM-10):
  - `*nr_claims` must describe the capacity of a valid caller-provided array.
  - An array of `2^28` 16-byte entries needs 4 GiB and cannot exist in a 32-bit process.
  - A valid 32-bit caller therefore cannot reach this multiplication wrap.
  - Page rounding is not a general safety argument for invalid capacities: the allocation header occupies 16 bytes before the returned buffer.

- `xenctrl.h`:
  - the comment documents the in/out `*nr_claims`, ENOBUFS, "claims is not modified" and the count query.
  - The order of the entries stays in `domctl.h`, as the `code-review` skill asks.
  - The prototype needs three lines:
    - because `xen_domctl_memory_claim_t *claims, uint32_t *nr_claims);` after the open parenthesis would exceed 80 columns.
  - The set prototype is laid out the same way.

- CHANGELOG.md:
  - the new sub-item follows the set item,
  - and fits in 74 columns.

- Without `HYPERCALL_BOUNCE_SET_SIZE()`, the whole capacity would be copied back:
  - from the zero-filled bounce buffer,
  - overwriting the caller's array beyond the returned entries,
  - and, on ENOBUFS, the whole array with zeros.

## Reference-test limits

- The native tests include the wrapper implementation but replace bounce-pre
  with buffer aliasing, bounce-post with a no-op, and libxc's domctl helper
  with a direct call returning Xen's negative errno value. They do not
  exercise real libxc's `-1`/`errno` conversion, bounce allocation failure,
  or output-buffer copying. Those paths were traced in the real libxc
  implementation rather than inferred from the native results.
- This cycle did not build or run the native or system suites. The tests are
  reference material, not new execution evidence.
