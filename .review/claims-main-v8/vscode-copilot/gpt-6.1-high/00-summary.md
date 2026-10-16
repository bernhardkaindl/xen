# claims-main-v8: deeper re-review, 2026-10-14

**Result:** no new actionable finding in the eight-patch claims series.
No source, test, commit-message or cover-letter changes were made.
The conclusion is limited to the documented initially paused domain build, with builder-side serialization and the previously accepted memory-offlining limitation.

## Deeper checks

- Allocation admission and redemption:
  - The actual allocation node, not the requested node, selects the first redemption step.
  - Under normal accounting, per-node admission and own-node redemption preserve other domains' node claims.
  - Host-wide redemption leaves the cached node sum unchanged.
  - Other-node redemption reduces only the allocating domain's claims and must finish because the remainder is bounded by its remaining node total.
  - Buddy fragmentation, address zones and exact-node flags can still prevent allocation; a claim reserves counts, not contiguous extents.

- Legacy/new interface interoperability:
  - Both installers reject an existing nonzero outstanding total.
  - The legacy interface takes a target allocation total and subtracts domain_tot_pages(); the new interface supplies outstanding claims directly.
  - Fully redeemed node arrays may remain attached with every entry zero.
  - A subsequent legacy host claim uses that zero array harmlessly; get reports the host claim and no node entries.
  - A subsequent new installation replaces the zero array under heap_lock and returns the detached storage for freeing.
  - Both zero-count release routes use domain_set_outstanding_pages(d, 0), removing the domain's node and global claims together.

- Array ownership and rejected installation:
  - Validation failure occurs before array installation.
  - Allocation failure leaves the domain's claims untouched.
  - Rejection under heap_lock leaves the private candidate array for the caller to free.
  - Successful installation returns the old detached array instead.
  - Allocation redemption may occur while page_alloc_lock is held elsewhere, but all claims-array accesses remain serialized by heap_lock.
  - Get copies a snapshot into its own buffer while holding heap_lock; it never retains the domain's array after unlocking.
  - Release and replacement do not free an attached array.

- Capacity-zero and copy-back behavior:
  - The real zero-sized Xen allocator returns ZERO_BLOCK_PTR, which xvfree()/xfree() accepts.
  - Get's bounded writes do not dereference that pointer with zero capacity.
  - A too-small private snapshot may contain a prefix, but the handler does not copy it to the guest on ENOBUFS.
  - Success and ENOBUFS trigger count copy-back; array-copy EFAULT does not.
  - Final domctl copy-back can itself fault after a partial copy, so no count guarantee is inferred for that error.

- Real libxc behavior versus the native passthrough:
  - Both wrappers call do_domctl(), which selects zero EFAULT retries; the neighboring retry-capable helper does not silently repeat these requests.
  - The domctl uses a BOTH-direction bounce independently of the claims-array bounce.
  - Get suppresses claims-array copy-back on failure and limits successful copy-back to the returned entry count.
  - The bounce allocator stores its page count in a separate allocation header; reducing the copy length does not affect freeing.
  - No new valid-caller size-multiplication failure was established; the earlier accepted caller-buffer contract remains unchanged.

- Test-layer prerequisites:
  - The core allocator, handlers, claims headers, FLASK hooks and wrappers match claims-main-v8.
  - Later header changes were not treated as proof that each earlier claims patch compiles.
  - The inspected NUMA prerequisite relocates nodeid_t but retains its eight-bit width; it does not widen a claims-related type.
  - No later correction to the reviewed claims implementation was found.

## Existing limits, not new findings

- Allocation awaiting assignment:
  - page_alloc_lock does not account for a page already removed from the heap but not yet assigned.
  - MEMF_keep_scrub population can retain such an allocation in pending_scrub across hypercall continuation.
  - Builder serialization must cover that unfinished operation, not merely simultaneous hypercall entry.
  - This is the previously documented SCE-1 boundary, not a newly introduced per-node accounting defect.

- Pause count:
  - A positive controller pause count is not a universal synchronous-quiescence proof for an asynchronous debugger stop.
  - The review does not extend the supported workflow to arbitrary previously running domains.

## Native harness update: 2026-10-15

- Bounce and guest-copy coverage has since improved:
  - The harness includes production libxc bounce pre/post code with host-backed buffer allocation.
  - Tests exercise synthetic claims-array copy failures and partial final-domctl copy-back failures.
  - This does not exercise real hypervisor guest-address translation, page faults or the production xencall buffer allocator.

- XSM gate coverage added:
  - An optional native decision callback replaces the formerly unconditional xsm_domctl() success stub.
  - Tests call the production do_domctl() through libxc wrappers and verify the command, target domain and number of authorization calls.
  - Fourteen decisions across two target domains cover rejected installation, successful installation, rejected release, rejected array/count reads, successful read and successful release.
  - Rejected installation with a NULL claims buffer returns EACCES rather than EFAULT, checking authorization before array access.
  - Rejected reads preserve the output count and buffer; rejected release preserves claims and accounting.
  - Independent injected set/get decisions check that release is still authorized and reads need not share the set decision.
  - The hook defaults to allowing operations and is cleared after each target-domain trace.

- Remaining XSM limits:
  - The callback supplies decisions; it does not execute the dummy XSM privilege check, FLASK permission routing or the access-vector cache.
  - These tests would detect a missing, ignored or misdirected domctl authorization call, not a wrong setclaim/getdomaininfo mapping in flask_domctl().
  - Actual FLASK policy enforcement still needs a configured Xen runtime or a separate FLASK-focused test environment.