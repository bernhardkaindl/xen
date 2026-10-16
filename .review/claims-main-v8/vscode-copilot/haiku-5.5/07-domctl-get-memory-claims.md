# 07 - xen/domctl: add XEN_DOMCTL_get_memory_claims

**Result:** no open findings.
- heap_lock protects a consistent snapshot of the totals and node entries.
- ENOBUFS may leave a partial internal buffer, but no entries are copied to
  the guest. Only the domctl is copied back with the required count.
- On an array-copy EFAULT, the updated count is not copied back.
- EFAULT can also arise from copying back the domctl itself.
- Zero capacity uses ZERO_BLOCK_PTR and requires no allocation.
- Claimed nodes cannot currently go offline after reaching the heap.
- Both operations appropriately share one documented argument structure.
- claim_request documents nr_entries for set and get, and the sums computed
  by domain_set_claims().
- FLASK shares getdomaininfo with the existing domain-info operation.
