# 01 - xen/mm: protect per-node claims in get_free_buddy()

**Result:** no open findings.
- The node availability subtraction is clamped before crediting the
  domain's claim.
- Anonymous and MEMF_no_refcount allocations get no claim credit.
- The next-node label preserves exact-node failure handling.
- Before claims can be installed, the new counters remain zero.
