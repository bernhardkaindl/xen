# 06: libxc: add xc_domain_set_memory_claims()

## Review report

### tools/include/xenctrl.h

**Function prototype is correct**

The prototype:
```c
/*
 * Atomically set a domain's host-wide and per-node memory claims before
 * its creation finishes. The domain must have no outstanding claims.
 * Each array entry reserves pages on one NUMA node or host-wide.
 * nr_claims == 0 releases all claims, regardless of creation state.
 */
int xc_domain_set_memory_claims(xc_interface *xch, uint32_t domid,
                                const xen_domctl_memory_claim_t *claims,
                                uint32_t nr_claims);
```

**The function prototype is correct**

- Uses `const` for input pointer
- Parameter order is logical (xch, domid, claims, nr_claims)
- Return type is `int` for error codes

**The comment of the prototype is informative and complete**

- Purpose: set domain's host-wide and per-node memory claims
- Constraint: domain must have no outstanding claims
- Semantics: each entry reserves pages on one NUMA node or host-wide
- Special case: `nr_claims == 0` releases all claims

### tools/libs/ctrl/xc_domain.c

**Implementation is correct**

The function:
- Uses `DECLARE_HYPERCALL_BOUNCE_IN` for input-only array
- Checks bounce pre and post
- Sets up domctl with correct command, domain, nr_claims
- Sets guest handle correctly
- Calls `do_domctl()`
- Returns the result

**Bounce buffer usage is appropriate**

For input-only data, `DECLARE_HYPERCALL_BOUNCE_IN` is the correct choice.

### CHANGELOG.md

**Changelog entry is appropriate**

Adds entry for per-node NUMA memory claims support, listing both set operations.

## Summary
**Status: ✅ APPROVED**

**Code quality:** Good - no functional or style issues found.
