# 05 - xen/domctl: add XEN_DOMCTL_set_memory_claims

**Result:** no open findings within the documented scope.
- Release precedes install-only state checks and is harmless without claims.
- Entry count and domain state are checked before copying the request.
- The buffer is freed on every path after allocation.
- The pause check prevents guest-initiated allocations; the public header
  requires the builder to serialize installation with allocations for the
  domain. It does not close the allocation/assignment window.
- ESRCH describes both a missing domain and a dying-domain install request.
- The error list documents outcomes, not check precedence.
- FLASK shares setclaim with the legacy claim operation.
