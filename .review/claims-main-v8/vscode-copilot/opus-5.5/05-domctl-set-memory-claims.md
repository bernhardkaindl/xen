# 05 - xen/domctl: add XEN_DOMCTL_set_memory_claims

**Result:** no new functional finding within the documented domain-build scope.

## Code review

- Domain lookup:
  - The op takes the `default:` path of `do_domctl()`:
    - so `d` comes from `rcu_lock_domain_by_id()` and is never NULL.
  - System domains are not found (ESRCH).
  - The XSM check `xsm_domctl(XSM_PRIV, d, op)` runs before `domctl_lock_acquire()`.
  - The handler runs under the domctl lock.

- Destruction is serialized with installation:
  - `domain_kill()` sets `is_dying` and releases the claims under the domctl lock:
    - "Protected by domctl_lock" in `domain.c`.
  - `domain_create()` sets `controller_pause_count = 1`:
    - so the build use case passes the EBUSY check.
  - This settles the open point from patch 4:
    - no claim can be installed after `domain_kill()` released them,
    - so `outstanding_claims` and `d->claims` cannot leak:
      - `set_memory_claims()` runs under `domctl_lock`, so `d->is_dying` is stable.
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

## Decisions

- For an initially paused domain build, the pause requirement prevents
  guest-initiated allocations, not allocations for the domain by another
  builder thread. The public header and the v8 cover letter require the
  builder to serialize installation with allocations.
  This documents the existing single-builder assumption; it does not close
  the alloc_heap_pages()/assign_pages() window discussed in SCE-1.

- The earlier assertion that `controller_pause_count` changes only under
  domctls or at boot was wrong. With CONFIG_GDBSX (x86 only),
  `domain_pause_for_debugger()` calls
  `domain_pause_by_systemcontroller_nosync()` from guest trap/VM-exit paths,
  without `domctl_lock`. It increments the controller count before requesting
  asynchronous vCPU sleep. A positive count alone therefore does not prove
  synchronous quiescence of a previously running, debugger-paused domain.
  This does not invalidate the dying-domain serialization proof above.
  The supported use remains an initially paused domain build, not installing
  claims on a previously running domain after an asynchronous debugger stop.
  If that use is required, the minimum caller-side measure is a synchronous
  `XEN_DOMCTL_pausedomain` before installation, with its matching unpause.
  A hypervisor guarantee for arbitrary paused domains needs separate review;
  no extra pause or creation-state restriction is silently added here.

- The errno list is not in the order of the checks:
  - EINVAL and ENOMEM each come from several checks.
  - A list in check order would suggest a precedence that the code does not have.
  - It means the errno list could be sorted by first possible appearance of the error code:
    - but doing so is not worth it.
  - A logical order that makes sense to a reviewer should be the goal.
