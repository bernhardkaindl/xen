# 05: xen/domctl: add XEN_DOMCTL_set_memory_claims

## Review report

### xen/xsm/flask/policy/access_vectors

**XSM policy updated correctly**

The commit updates the comment for the `setclaim` permission to include `XEN_DOMCTL_set_memory_claims`:
```ini
# XENMEM_claim_pages, XEN_DOMCTL_set_memory_claims
     setclaim
```

This is correct and consistent with Xen's XSM policy convention.

### xen/xsm/flask/hooks.c

**Hook added correctly**

The commit adds the case for `XEN_DOMCTL_set_memory_claims`:
```c
case XEN_DOMCTL_set_memory_claims:
    return flask_claim_pages(d);
```

This reuses the existing `flask_claim_pages()` function, which is correct.

### xen/common/domctl.c

**Function implementation is correct**

The `set_memory_claims()` function:
- Validates `pad` field is zero
- Handles release `(nr_claims == 0)` by calling `domain_set_outstanding_pages(d, 0)`
- Rejects with `-EOPNOTSUPP` if LLC coloring is enabled
- Rejects with `-ESRCH` if domain is dying
- Rejects with `-E2BIG` if nr_claims exceeds `MAX_NUMNODES + 1`
- Rejects with `-EEXIST` if domain creation is finished
- Allocates bounce buffer for guest array
- Copies from guest and validates
- Calls `domain_set_claims()`
- Frees bounce buffer

All error handling is correct.
---
## Summary
**Status: ✅ APPROVED**

**Code quality:** Good - no functional or style issues found in the code changes.
