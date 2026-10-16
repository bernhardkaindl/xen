# 06 - libxc: add xc_domain_set_memory_claims()

**Result:** no open findings.
- The input-only bounce matches the operation.
- A NULL pointer with count zero avoids allocation and releases claims.
- A non-NULL zero-length buffer is supported, but its bounce allocation
  can still fail.
- Invalid NULL/nonzero combinations are rejected by Xen.
- The concise wrapper usage documentation is retained; the detailed
  hypercall contract and error list remain in the public header.
- The implementation keeps its one-line purpose comment.
