# 01: xen/mm: protect per-node claims in get_free_buddy()

## Review report

### xen/common/page_alloc.c:911-920

**Per-node claim checking logic is correct**

The code:
```c
/* Offlining can reduce free pages below outstanding claims. */
unsigned long available =
    node_avail_pages[node] > node_claimed_pages[node]
    ? node_avail_pages[node] - node_claimed_pages[node]
    : 0;

/* Ref-counted allocations may use the domain's per-node claim. */
if ( d && !(memflags & MEMF_no_refcount) && d->claims )
    available += d->claims[node];

if ( available < request )
    goto next_node;
```

- Comment is clear and accurate
- Ternary expression correctly clamps at zero
- Ref-counted allocation handling is correct
- Skip logic is correct

## Summary

**Status: ✅ APPROVED**

All changes in this commit are technically correct and follow Xen coding style. No issues found.
