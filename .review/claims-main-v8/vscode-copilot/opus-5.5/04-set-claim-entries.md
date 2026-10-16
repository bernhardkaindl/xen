# 04 - xen/mm: add domain_set_claims() for NUMA memory claims

**Result:** accounting and ownership are correct. The availability comparison
uses an overflow-independent comparison; see the arithmetic rationale below.
- Wording changes applied to the commit message and to the notes below `---`.

## Code review

- Locking:
  - `domain_set_claims()` takes `d->page_alloc_lock`, which protects:
    - `d->max_pages` (`XEN_DOMCTL_max_mem` writes it under this lock),
    - and `domain_tot_pages()`.
  - Therefore the validation is still valid when `domain_install_claims()` takes `heap_lock`.
  - The lock order is the same as in `domain_set_outstanding_pages()`.
  - `get_free_buddy()` reads `d->claims` only under `heap_lock`:
    - so swapping the pointer under `heap_lock` and freeing the old array after unlocking is safe.

- Validation cannot overflow:
  - `target_pages > d->max_pages - request->total` keeps `total <= max_pages <= UINT_MAX`:
    - so `total` fits in a 32-bit `unsigned long`,
    - and `d->node_claims` and `d->outstanding_pages` (both `unsigned int`) do not truncate.
  - The final test checks `tot_pages > max_pages` before the subtraction:
    - `max_pages` can be below `tot_pages` after `XEN_DOMCTL_max_mem`.

- Targets:
  - HOST twice, reserved bit-31 targets and duplicate nodes return -EINVAL.
  - `target >= MAX_NUMNODES` is tested before `node_online()`.
  - Online nodes without enough free memory fail with -ENOMEM in the memory check.

- `check_memory_for_claim_request()`:
  - The original first test did not, by itself, prevent the following
    addition from wrapping. The current code compares outstanding claims
    against `total_avail_pages - request->total` after checking that the
    subtraction is safe.
  - The per-node sum is `uint64_t`.
- `domain_install_claims()` updates all five counters under `heap_lock`:
  - `d->outstanding_pages`,
  - `d->node_claims`,
  - `d->claims[]`,
  - `outstanding_claims`,
  - and `node_claimed_pages[]`.
  - Every entry has nonzero pages:
    - so `new_claims` is non-NULL whenever the loop writes through it.
  - On failure nothing is modified and the unused array is freed.

- Rejecting a domain with outstanding claims returns -EINVAL:
  - like the legacy "only one active claim per domain" path.

- `xvzalloc_array()` runs under `d->page_alloc_lock` with IRQs enabled:
  - It may take `heap_lock` after `d->page_alloc_lock`,
  - which is the established lock order.

- `page_alloc.c` gets `public/domctl.h` through `xen/sched.h`:
  - which includes it directly.
  - No include is missing.

- A non-static function without a caller builds without warnings:
  - and its static helpers are used,
  - so the patch is bisectable.

- Requirements satisfied by the patch 5 caller:
  - The domctl must refuse dying domains under `domctl_lock`:
    - `domain_kill()` releases the claims under that lock,
    - and claims installed later would leak `outstanding_claims`.
  - It must also bound `nr_entries`.
  - It must also copy the entries into a Xen buffer:
    - because the helpers read `request->entries[i]` more than once.

## Decisions about code

- About the atomic overhead of `node_test_and_set()`:
  - It uses the nodemask API to detect duplicate node targets.
  - The bitmap is local to validation, with one operation per node entry.
  - No cycle count or cache-line ownership guarantee has been established for all supported architectures.
  - Avoiding atomics entirely would need `__set_bit()` on `seen.bits`:
    - it bypasses the `nodemask` API,
    - and is not worth it on this path.

## Decisions about nitpicks

- "This patch and the next one form one logical change" is a forward
  reference, but it is below `---`.
  - Such reviewer-facing text does not end up in git,
    and on xen-devel it is the usual way to explain a split.
  - Earlier reviews agreed.

- Blank lines:
  - The layout matches the author's style of patch 3 (blank line after declarations and after `if` blocks).
  - Not changed:
    - this appears to be the preferred Xen style for new code.

- The `domain_validate_claim_request()` function header comment:
  - `/* Validate a claim request and compute request->total and ->node_pages. */`
  - It names the out-fields that the code shows.
  - It is one line that states the purpose and the side effect that the name "validate" does not show.
  - Keep it, a summary of the effect of the function is perfect.

## Re-review

- The availability comparison uses subtraction rather than addition.
  Checking `request->total <= total_avail_pages` does not prove that
  `request->total + outstanding_claims` is representable. For example,
  32-bit values `total_avail_pages = 0xf0000000`,
  `outstanding_claims = 0xe0000000`, and `request->total = 0x30000000`
  pass the original check despite insufficient unclaimed memory.
  This is an arithmetic counterexample, not a demonstrated reachable
  configuration on supported hardware. The subtraction-based comparison
  removes that dependency and also rejects claims when offlining has
  reduced availability below outstanding claims.

- Re-checked the sums:
  - After each entry `request->total <= max_pages`:
    - `d->max_pages - request->total` never wraps,
    - and the `unsigned int` stores in `domain_install_claims()` do not truncate.

- `0x80000000U` is open-coded in the bit-31 test next to the comment.
  - A named mask would be one more public constant for a private check, so it stays.
