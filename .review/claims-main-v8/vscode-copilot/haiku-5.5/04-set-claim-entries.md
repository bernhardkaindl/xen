# 04 - xen/mm: add domain_set_claims() for NUMA memory claims

**Result:** no open findings within the documented scope.
- Validation bounds all sums by max_pages before narrowing stores.
- The availability comparison checks the request against the available
  total before subtracting it, then compares existing outstanding claims
  against the remainder.
- All counters and the claims pointer are installed atomically under
  heap_lock; failure leaves the claims unchanged.
- The ownership comment describes both the input array and the array the
  caller must free on return.
- The bit-31 literal implements the reserved target range, not merely the
  HOST value. No extra public mask constant is needed.
- The allocation/assignment window remains the accepted SCE-1 limitation.
