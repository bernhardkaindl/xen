# 06 - libxc: add xc_domain_set_memory_claims()

**Result:** the code and the commit message are correct.

The most recent code and comment fixes, found during review, were:
- The comment above the definition no longer repeats what the name already said.
  - It now provides more specific information about the function.
- The prototype comment in `xenctrl.h` now adds that installing claims requires a paused domain.

## Code review

- The wrapper zero-initialises `struct xen_domctl`:
  - so `pad` is zero, and `do_domctl()` sets the interface version.
  - It returns 0, or -1 with errno set, like the neighbouring `xc_domain_*` wrappers.

- `DECLARE_HYPERCALL_BOUNCE_IN()`:
  - is the right direction for an input-only array and accepts the `const` pointer.

- `nr == 0` with `claims == NULL` skips the bounce.

- With a non-NULL pointer, a zero-sized bounce still allocates the header page.

- Either way, the domctl releases the claims.

- `xc_hypercall_bounce_post()` runs after the call on every path.

- `sizeof(*claims) * nr` can wrap for 32-bit tools with `nr >= 2^28`:
  - Xen then returns E2BIG before it reads the buffer, so nothing is read out of bounds.
  - The neighbouring wrappers use the same pattern.

- A bounce failure returns -1 without `PERROR()`:
  - Handled like done by `xc_domain_hvm_getcontext()` and `xc_domain_getinfolist()`.
  - Other wrappers log it. Both styles exist.
  - Bounce allocation failure happens before Xen checks claim availability.
  - Returning -1 with errno set lets the caller decide how to report or recover from it.

- CHANGELOG.md: the entry is under "### Added" of 4.23.0 UNRELEASED:
  - Its indentation and the `XEN_DOMCTL_x/xc_x()` pairing follow the existing 4.22 entries.

## Considered, not edited

- The declaration is not next to `xc_domain_claim_pages()` (line 1323), but after `xc_domain_set_llc_colors()`, the newest domctl wrapper:
  - That is a valid choice.
  - Moving it would also move the get wrapper.

## Decisions

- "per-node NUMA memory claims" in CHANGELOG.md is slightly redundant.
  - The cover letter uses the same wording,
  - and it needs to be clear to readers who do not know the code.

- Argument order:
  - Since 2016, new libxc wrappers pass an array before its count:
    - `xc_domain_set_llc_colors()`
    - `xc_cpu_policy_update_*()`
    - `*_set_mem_access_multi()`
    - the RTDS vCPU wrappers
  - Count-first appears only in older code.
  - Both wrappers now use `claims, nr`.
