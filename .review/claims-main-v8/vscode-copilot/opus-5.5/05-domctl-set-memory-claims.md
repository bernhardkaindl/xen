# 05 - xen/domctl: add XEN_DOMCTL_set_memory_claims

**Result:** the code is correct.

## Code review

- Domain lookup:
  - The op takes the `default:` path of `do_domctl()`:
    - so `d` comes from `rcu_lock_domain_by_id()` and is never NULL.
  - System domains are not found (ESRCH).
  - The XSM check `xsm_domctl(XSM_PRIV, d, op)` runs before `domctl_lock_acquire()`.
  - The handler runs under the domctl lock.

- The checks are stable while the handler runs:
  - `domain_kill()` sets `is_dying` and releases the claims under the domctl lock:
    - "Protected by domctl_lock" in `domain.c`.
  - `controller_pause_count` changes only through domctls (`pausedomain`, `unpausedomain`, gdbsx) and at boot.
  - `domain_create()` sets `controller_pause_count = 1`:
    - so the build use case passes the EBUSY check.
  - This settles the open point from patch 4:
    - no claim can be installed after `domain_kill()` released them,
    - so `outstanding_claims` and `d->claims` cannot leak:
      - `set_memory_claims()` runs under `domctl_lock`, so `d->is_dying` and `controller_pause_count` are stable.
      - `domain_kill()` is called only from `XEN_DOMCTL_destroydomain`, which takes the same lock.

- Release:
  - `nr_entries == 0` calls `domain_set_outstanding_pages(d, 0)` before the state checks.
  - For a dying domain whose claims `domain_kill()` already released, `d->node_claims == 0`.
  - The release loop of patch 2 then stops before it reads `d->claims[]`:
    - so the NULL array is never dereferenced.

- Install:
  - `nr_entries` is bounded by `MAX_NUMNODES + 1` before the allocation.
  - The entries are copied into a Xen buffer:
    - which settles the second open point from patch 4.
  - The buffer is freed on all paths after it is allocated.

- The `domctl.h` errno list matches the code entry by entry, including:
  - both `pad` fields,
  - `tot_pages > max_pages` (EINVAL),
  - the `MAX_NUMNODES`-based E2BIG,
  - and "fails without changing the claims".
  - "Paused" means `controller_pause_count`, as for `XEN_DOMINF_paused`.

- Compared with `XENMEM_claim_pages`:
  - LLC coloring returns EOPNOTSUPP in both:
    - this op still allows the release.
  - A dying domain gets ESRCH for an install, and EINVAL from the legacy op.

- XSM:
  - dummy `xsm_domctl()` falls to `xsm_default_action(XSM_PRIV, ...)`.
  - `flask_claim_pages(d)` checks `domain2:setclaim`.
  - `tools/flask/policy` already grants `setclaim` in `create_domain_common`.
  - `docs/misc/xsm-flask.txt` covers all domctls not listed as safe:
    - so no policy or doc change is needed.

- ABI:
  - `xen_domctl_memory_claims` is 8 + 4 + 4 bytes without implicit padding.
  - Command 91 follows 90.
  - The `domctl.h` header says pure additions need no interface-version bump.
  - `domctl.c` includes `<xen/llc-coloring.h>` and `<xen/xvmalloc.h>`.

## Decisions for later re-consideration

- The errno list is not in the order of the checks:
  - EINVAL and ENOMEM each come from several checks.
  - A list in check order would suggest a precedence that the code does not have.
  - It means the errno list could be sorted by first possible appearance of the error code:
    - but doing so is not worth it.
  - A logical order that makes sense to a reviewer should be the goal.
