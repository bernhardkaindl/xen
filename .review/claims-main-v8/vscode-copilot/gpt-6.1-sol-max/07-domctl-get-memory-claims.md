# 07 - add `XEN_DOMCTL_get_memory_claims`

**Result:** No functional defect found. Suggested a review-only message clarification that distinguishes hypervisor guest copies from the later libxc bounce behavior; no code change.

## Code review

- Reviewed this patch before the get-libxc patch. The new dispatch remains after `xsm_domctl()` and before domctl-lock acquisition, with an RCU reference protecting the domain.
- `domain_get_claims()` snapshots the total, host-wide portion, and node entries under `heap_lock`. Release detaches under that same lock, so the reader does not retain an array that can be freed after unlocking.
- The snapshot is private Xen storage. Only success copies entries to the guest; `ENOBUFS` copies back the required count, not the private prefix.
- The maximum is one host-wide entry plus one per node. Clamping a larger caller capacity therefore cannot manufacture an insufficient-capacity error.
- Zero capacity causes no snapshot writes. The existing zero-size allocator convention supplies a freeable non-NULL sentinel; the zero-entry path copies zero bytes.
- Compound literals initialize entry padding, and the output allocation remains zeroed as requested in the earlier review.
- FLASK reuses `getdomaininfo`; dummy XSM still performs its ordinary privileged domctl check. Reusing a FLASK permission does not import the separate `XSM_XS_PRIV` getdomaininfo exception into this dispatch.
- Command numbering, public layout, the count spelling, and the internal set/get request comments are consistent at this revision.

## Can lockless get race with release and read freed storage?

**The suspicion.** Get does not take the domctl lock, while destruction, explicit release, or another claim installation can detach and free `d->claims`.

**Disposition.** Lockless here means only that the domctl lock is unnecessary. Both the snapshot reader and every array/counter writer use `heap_lock`; release frees only after detachment and unlocking. The copied entries are owned by the handler, and the enclosing RCU reference keeps the domain alive. No detached pointer survives into the guest-copy stage.

**Targeted mitigation.** No additional lock or source comment. Existing field-protection and detach/free comments explain the relevant ownership boundary.
