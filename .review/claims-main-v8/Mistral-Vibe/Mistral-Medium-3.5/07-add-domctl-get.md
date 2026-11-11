# 07: xen/domctl: add XEN_DOMCTL_get_memory_claims

## Review report

### xen/xsm/flask/policy/access_vectors

**XSM policy updated correctly**

Adds `XEN_DOMCTL_get_memory_claims` to the `getdomaininfo` permission comment:
```ini
# XEN_DOMCTL_getdomaininfo, XEN_SYSCTL_getdomaininfolist,
# XEN_DOMCTL_get_memory_claims
     getdomaininfo
```

This is correct - reusing the existing getdomaininfo permission for the get operation.

### xen/xsm/flask/hooks.c

**Hook added correctly**

```c
case XEN_DOMCTL_get_memory_claims:
    return flask_getdomaininfo(d);
```

Reuses `flask_getdomaininfo()` which is appropriate.

### xen/include/xen/mm.h

**Function prototype added**

```c
int domain_get_claims(const struct domain *d, struct claim_request *request);
```

Correct placement in the "Claim handling" section.

**Comment for nr_entries**

The comment in `struct claim_request`:
```c
/*
 * Set: number of entries.
 * Get: capacity of entries[] on input, number provided/needed on output.
 */
```

This is clear.

### xen/common/domctl.c

**Function implementation is correct**

`get_memory_claims()`:
- Validates pad is zero
- Clamps nr_claims to `MAX_NUMNODES + 1`
- Allocates zero-initialized buffer with `xvzalloc_array`
- Calls `domain_get_claims()` to populate
- Sets `op->nr_claims` to actual count
- Copies to guest on success
- Returns appropriate error codes

**Error handling**

- `-EINVAL` for nonzero pad
- `-ENOMEM` for allocation failure
- `-ENOBUFS` if `capacity < count` (with `count` returned)
- `-EFAULT` for copy_to_guest failure

**Copyback handling**

The do_domctl case correctly sets `copyback = true` for success and `-ENOBUFS`.

### xen/common/page_alloc.c
**`domain_get_claims()` implementation**

The function:
- Takes `heap_lock`
- Asserts `d->outstanding_pages >= d->node_claims`
- Calculates `host_claim`
- If `host_claim` is non-zero, adds host-wide entry first
- Iterates through online nodes, adding non-zero claims
- Returns `0` if all claims fit, `-ENOBUFS` if more claims than capacity

**Correct behavior**
- Returns claims in order: host-wide first, then per-node in ascending order
- Only returns non-zero claims
- Returns count in `request->nr_entries`
---
## Summary
**Status: ✅ APPROVED**

**Code quality:** Good - implementation is correct and follows Xen conventions.
