# 02 - xen/mm: release per-node claims in domain_set_outstanding_pages()

**Result:** no open findings.
- The loop stops before dereferencing a NULL claims array.
- Per-node subtractions are bounded by the maintained counter invariants.
- The global outstanding total includes the per-node claims.
- The array is detached under heap_lock and freed after dropping the locks;
  the code comment explains that xvfree() can take heap_lock.
- The cached-sum assertion checks that the released node claims sum to zero.
- A node that has reached the heap cannot currently be offlined.
