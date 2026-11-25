# 08: libxc: add xc_domain_get_memory_claims()

## Review report

### tools/include/xenctrl.h

**Function prototype is correct**

```c
/*
 * Get the memory claims of a domain.
 * *nr_claims is the capacity of claims on input and the number of claims on
 * output.  If the capacity is too small, fail with errno ENOBUFS without
 * modifying claims.  Pass *nr_claims == 0 and claims == NULL to query the
 * number of claims.
 */
int xc_domain_get_memory_claims(xc_interface *xch, uint32_t domid,
                                xen_domctl_memory_claim_t *claims,
                                uint32_t *nr_claims);
```

**Comment is comprehensive**

The comment explains:
- Purpose: get memory claims of a domain
- `*nr_claims` semantics: capacity on input, count on output
- `-ENOBUFS` behavior: returns required count without modifying claims
- Query mode: `*nr_claims == 0` and `claims == NULL` to get count

This is excellent documentation.

### tools/libs/ctrl/xc_domain.c

**Implementation is correct**

The function:
- Uses `DECLARE_HYPERCALL_BOUNCE` with `XC_HYPERCALL_BUFFER_BOUNCE_OUT` for output-only
- Checks bounce pre and post
- Sets up `domctl` with `command`, `domain` and `nr_claims`
- Sets guest handle
- Calls `do_domctl()`
- Updates `*nr_claims` with returned count
- Uses `HYPERCALL_BOUNCE_SET_SIZE` to set copy-back size based on success/failure

**Correct bounce buffer handling**

For output-only data, `DECLARE_HYPERCALL_BOUNCE` with `XC_HYPERCALL_BUFFER_BOUNCE_OUT` is correct.

**Copy-back size optimization**

```c
HYPERCALL_BOUNCE_SET_SIZE(claims, ret ? 0 : sizeof(*claims) * *nr_claims);
```

This ensures only the actual returned entries are copied back on success, and nothing on failure. This is correct and efficient.

**`nr_claims` handling**

```c
*nr_claims = domctl.u.memory_claims.nr_claims;
```

Updates the count before the bounce post, which is correct because `HYPERCALL_BOUNCE_SET_SIZE` uses the updated `*nr_claims`.

## Summary

**Status: ✅ APPROVED**

**Code quality:** Excellent - implementation is correct, well-documented, and follows best practices.
