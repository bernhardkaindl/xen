# Patch 9: libxc get-memory-claims wrapper

No new implementation finding for valid caller-provided arrays:
- The output-only bounce uses the input capacity. The post-copy size is reduced to the returned count on success and zero on every failure, preserving unused capacity and the unchanged-array contract on `ENOBUFS`.
- NULL with zero capacity is the documented count query. Count output is meaningful on success and `ENOBUFS`; final domctl copy-back can partially fail, so no valid count is promised on `EFAULT`.
- The 32-bit multiplication concern requires a count that cannot describe a valid caller array in a 32-bit process. Xen's capacity clamp alone would not protect an undersized bounce allocation for arbitrary invalid capacities; page rounding is not a safety proof.
- A separate count/fetch pair is not atomic, but each fetch handles its own capacity. The caller must allow for state changes between calls.
- Consulted the corresponding Opus, Astra and Haiku reports. The real bounce implementation, not native-test shims, establishes copy-back behavior.

Verification:
- Source review against the real bounce and domctl helpers. No compile or runtime tests were run.