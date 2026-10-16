# Patch 6: libxc set-memory-claims wrapper

No new finding:
- The zeroed domctl supplies zero reserved padding; the common helper sets its interface version. The input-only bounce accepts the const array and is released after the hypercall.
- A NULL array with zero count avoids the entry-array bounce and requests release. A non-NULL zero-length array still follows the bounce allocator and can fail before release reaches Xen.
- On 32-bit tools the byte-size product can wrap for a huge count, but Xen checks the unchanged count against `MAX_NUMNODES + 1` before reading entries. No accepted request obtains a smaller buffer through that wrap. Invalid input is not guaranteed to reach the hypervisor before a local allocation failure.
- The public wrapper comment retains useful target/count/pause guidance without duplicating the full public-hypercall contract and error list.
- The changelog describes the set interface available at this revision. Consulted the corresponding Opus, Haiku and earlier claims-v8 notes.
- The shared library rules generate a version map with `global: *`; no explicit export-list addition is missing for either new wrapper.

Verification:
- Source-traced the common bounce and domctl helpers. No compile or runtime tests were run.