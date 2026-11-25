# 02: xen/mm: release per-node claims in `domain_set_outstanding_pages()`

## Review report

### xen/common/page_alloc.c:531-533

**Lock comment updated**

Extended to include the new fields:
```c
/*
 * Two locks are needed here:
 *  - d->page_alloc_lock: protects accesses to d->{tot,max,extra}_pages.
 *  - heap_lock: protects accesses to d->outstanding_pages,
 *    d->node_claims, d->claims[], total_avail_pages, outstanding_claims,
 *    and node_claimed_pages[].
 */
```

This is comprehensive and accurate.

### xen/common/page_alloc.c:823-840

**Per-node claim release logic**

The code releases per-node claims by:
1. Iterating through all online nodes
2. Subtracting each node's claim from global counters
3. Detaching and freeing the claims array

### xen/common/page_alloc.c:843

**Deferred free after unlock**

```c
xvfree(old_claims);
```

Correctly placed after unlocking because xvfree can take heap_lock.

## Summary

**Status: ✅ APPROVED**

All changes in this commit are technically correct.
